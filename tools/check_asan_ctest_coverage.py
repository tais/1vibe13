#!/usr/bin/env python3
"""Require Linux ASan CI to build every executable invoked by CTest."""

from collections import defaultdict, deque
from pathlib import Path
import re
import shlex
import sys
from typing import Iterable, Optional


ROOT = Path(__file__).resolve().parent.parent
WORKFLOW = ROOT / ".github" / "workflows" / "build_unix.yml"
CMAKE_MANIFESTS = (
    ROOT / "CMakeLists.txt",
    ROOT / "tests" / "CMakeLists.txt",
)
TARGET_PATTERN = re.compile(r"^[A-Za-z0-9_.:+-]+$")


def fail(message: str) -> None:
    raise RuntimeError(message)


def _cmake_code_mask(contents: str) -> str:
    """Mask comments and strings while preserving offsets and parentheses."""

    masked = list(contents)
    index = 0
    while index < len(contents):
        if contents[index] == "#":
            end = contents.find("\n", index)
            if end < 0:
                end = len(contents)
            masked[index:end] = " " * (end - index)
            index = end
            continue
        if contents[index] == '"':
            end = index + 1
            while end < len(contents):
                if contents[end] == '"' and contents[end - 1] != "\\":
                    end += 1
                    break
                end += 1
            masked[index:end] = " " * (end - index)
            index = end
            continue
        bracket = re.match(r"\[(=*)\[", contents[index:])
        if bracket:
            closing = "]" + bracket.group(1) + "]"
            end = contents.find(closing, index + len(bracket.group(0)))
            end = len(contents) if end < 0 else end + len(closing)
            masked[index:end] = " " * (end - index)
            index = end
            continue
        index += 1
    return "".join(masked)


def _cmake_call_spans(contents: str, command: str) -> list[tuple[int, int]]:
    """Return body offsets for calls to one CMake command."""

    mask = _cmake_code_mask(contents)
    pattern = re.compile(
        rf"(?i)(?<![A-Za-z0-9_]){re.escape(command)}\s*\(")
    spans: list[tuple[int, int]] = []
    for match in pattern.finditer(mask):
        body_start = match.end()
        depth = 1
        cursor = body_start
        while cursor < len(mask) and depth:
            if mask[cursor] == "(":
                depth += 1
            elif mask[cursor] == ")":
                depth -= 1
            cursor += 1
        if depth:
            fail(f"unterminated {command}() call in CMake manifest")
        spans.append((body_start, cursor - 1))
    return spans


def _cmake_calls(contents: str, command: str) -> list[str]:
    return [contents[start:end]
            for start, end in _cmake_call_spans(contents, command)]


def _cmake_arguments(body: str) -> list[str]:
    """Tokenize the simple argument forms used by target/test declarations."""

    arguments: list[str] = []
    index = 0
    while index < len(body):
        if body[index].isspace():
            index += 1
            continue
        if body[index] == "#":
            end = body.find("\n", index)
            index = len(body) if end < 0 else end + 1
            continue
        if body[index] == '"':
            cursor = index + 1
            value: list[str] = []
            while cursor < len(body):
                if body[cursor] == '"' and body[cursor - 1] != "\\":
                    cursor += 1
                    break
                value.append(body[cursor])
                cursor += 1
            arguments.append("".join(value))
            index = cursor
            continue
        bracket = re.match(r"\[(=*)\[", body[index:])
        if bracket:
            closing = "]" + bracket.group(1) + "]"
            start = index + len(bracket.group(0))
            end = body.find(closing, start)
            if end < 0:
                fail("unterminated bracket argument in CMake manifest")
            arguments.append(body[start:end])
            index = end + len(closing)
            continue
        cursor = index
        while cursor < len(body) and not body[cursor].isspace():
            cursor += 1
        arguments.append(body[index:cursor])
        index = cursor
    return arguments


def _test_command(arguments: list[str]) -> Optional[str]:
    if not arguments:
        return None
    if arguments[0].upper() == "NAME":
        try:
            command_index = next(
                index for index, argument in enumerate(arguments)
                if argument.upper() == "COMMAND")
        except StopIteration:
            return None
        if command_index + 1 >= len(arguments):
            return None
        return arguments[command_index + 1]
    return arguments[1] if len(arguments) >= 2 else None


def _target_test_factories(contents: str) -> dict[str, int]:
    """Find simple functions whose argument becomes an executable CTest."""

    factories: dict[str, int] = {}
    function_spans = _cmake_call_spans(contents, "function")
    end_spans = _cmake_call_spans(contents, "endfunction")
    for function_start, function_end in function_spans:
        declaration = _cmake_arguments(contents[function_start:function_end])
        if len(declaration) < 2:
            continue
        body_end = next(
            (start for start, _end in end_spans if start > function_end),
            None)
        if body_end is None:
            fail(f"function {declaration[0]} has no endfunction()")
        body = contents[function_end + 1:body_end]
        for parameter_index, parameter in enumerate(declaration[1:]):
            placeholder = "${" + parameter + "}"
            creates_target = any(
                (arguments := _cmake_arguments(call)) and
                arguments[0] == placeholder
                for call in _cmake_calls(body, "add_executable"))
            registers_target = any(
                _test_command(_cmake_arguments(call)) == placeholder
                for call in _cmake_calls(body, "add_test"))
            if creates_target and registers_target:
                factories[declaration[0]] = parameter_index
                break
    return factories


def compiled_ctest_targets(cmake_sources: Iterable[str]) -> set[str]:
    """Return CMake executable targets used as registered test commands."""

    sources = tuple(cmake_sources)
    executables: set[str] = set()
    generated_tests: set[str] = set()
    for contents in sources:
        for call in _cmake_calls(contents, "add_executable"):
            arguments = _cmake_arguments(call)
            if arguments and TARGET_PATTERN.fullmatch(arguments[0]):
                executables.add(arguments[0])
        for factory, parameter_index in _target_test_factories(contents).items():
            for call in _cmake_calls(contents, factory):
                arguments = _cmake_arguments(call)
                if (parameter_index < len(arguments) and
                        TARGET_PATTERN.fullmatch(arguments[parameter_index])):
                    target = arguments[parameter_index]
                    executables.add(target)
                    generated_tests.add(target)

    test_targets = set(generated_tests)
    for contents in sources:
        for call in _cmake_calls(contents, "add_test"):
            command = _test_command(_cmake_arguments(call))
            target_file = re.fullmatch(r"\$<TARGET_FILE:([^>]+)>", command or "")
            if target_file:
                command = target_file.group(1)
            if command in executables:
                test_targets.add(command)
    return test_targets


def _dependency_map(cmake_sources: Iterable[str]) -> dict[str, set[str]]:
    dependencies: dict[str, set[str]] = defaultdict(set)
    for contents in cmake_sources:
        for call in _cmake_calls(contents, "add_dependencies"):
            arguments = _cmake_arguments(call)
            if not arguments or not TARGET_PATTERN.fullmatch(arguments[0]):
                continue
            dependencies[arguments[0]].update(
                argument for argument in arguments[1:]
                if TARGET_PATTERN.fullmatch(argument))
    return dependencies


def _job_block(workflow: str, name: str) -> str:
    matches = list(re.finditer(
        rf"(?m)^ {{2}}{re.escape(name)}:\s*$", workflow))
    if len(matches) != 1:
        fail(f"build workflow must contain exactly one {name} job")
    start = matches[0].start()
    next_job = re.search(
        r"(?m)^ {2}[A-Za-z_][A-Za-z0-9_-]*:\s*$",
        workflow[matches[0].end():])
    end = (matches[0].end() + next_job.start()
           if next_job is not None else len(workflow))
    return workflow[start:end]


def _job_steps(job: str) -> list[str]:
    matches = list(re.finditer(r"(?m)^ {6}- name:\s*[^\n]+$", job))
    return [job[match.start():
                matches[index + 1].start()
                if index + 1 < len(matches) else len(job)]
            for index, match in enumerate(matches)]


def _unconditional_run_texts(job: str) -> list[str]:
    runs: list[str] = []
    for step in _job_steps(job):
        if re.search(r"(?m)^ {8}(?:if|continue-on-error):", step):
            continue
        marker = re.search(r"(?m)^ {8}run:\s*([^\n]*)$", step)
        if marker is None:
            continue
        value = marker.group(1).strip()
        if value in {"|", "|-", "|+", ">", ">-", ">+"}:
            runs.append(step[marker.end():])
        else:
            runs.append(value)
    return runs


def _shell_lines(script: str) -> list[list[str]]:
    logical = re.sub(r"\\\r?\n", " ", script)
    commands: list[list[str]] = []
    for line in logical.splitlines():
        line = line.strip()
        if not line or line.startswith("#"):
            continue
        try:
            commands.append(shlex.split(line, comments=True, posix=True))
        except ValueError as error:
            fail(f"cannot parse sanitizer shell command: {error}")
    return commands


def asan_explicit_build_targets(workflow: str) -> set[str]:
    job = _job_block(workflow, "sanitizer")
    scripts = _unconditional_run_texts(job)
    commands = [command for script in scripts for command in _shell_lines(script)]
    if not any("-DADDRESS_SANITIZER=ON" in command
               for command in commands):
        fail("sanitizer job must configure with -DADDRESS_SANITIZER=ON")
    if not any(command and command[0] == "ctest" and
               any(command[index:index + 2] == ["--test-dir", "build"]
                   for index in range(len(command) - 1))
               for command in commands):
        fail("sanitizer job must run the configured CTest suite")

    targets: set[str] = set()
    for command in commands:
        if len(command) < 5 or command[:4] != [
                "cmake", "--build", "build", "--target"]:
            continue
        for argument in command[4:]:
            if argument in {"&&", "||", ";"} or argument.startswith("-"):
                break
            if TARGET_PATTERN.fullmatch(argument):
                targets.add(argument)
    if not targets:
        fail("sanitizer job has no unconditional explicit test-target build")
    return targets


def validate_asan_ctest_coverage(
        workflow: str, cmake_sources: Iterable[str]) -> tuple[set[str], set[str]]:
    sources = tuple(cmake_sources)
    required = compiled_ctest_targets(sources)
    explicit = asan_explicit_build_targets(workflow)
    dependencies = _dependency_map(sources)
    covered = set(explicit)
    queue = deque(explicit)
    while queue:
        for dependency in dependencies.get(queue.popleft(), ()):
            if dependency not in covered:
                covered.add(dependency)
                queue.append(dependency)
    missing = sorted(required - covered)
    if missing:
        fail("sanitizer job does not build registered CTest executable targets: "
             + ", ".join(missing))
    return required, explicit


def main() -> int:
    try:
        required, explicit = validate_asan_ctest_coverage(
            WORKFLOW.read_text(encoding="utf-8"),
            [path.read_text(encoding="utf-8") for path in CMAKE_MANIFESTS])
    except (OSError, RuntimeError) as error:
        print(f"ASan CTest coverage validation failed: {error}", file=sys.stderr)
        return 1
    print(f"ASan CTest coverage validated: {len(required)} executable targets "
          f"covered by {len(explicit)} explicit build targets plus dependencies")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
