#!/usr/bin/env python3
"""Opt-in installed-data smoke for independent full-engine co-op processes.

The test never launches from the installed tree.  Each process gets a private
working directory containing copies of the two small configuration files and
read-only-intent symlinks to the installed Data directories.  All mutable game,
campaign, client, and log state therefore stays beneath one private temporary
root owned by this invocation.
"""

from __future__ import annotations

import argparse
import configparser
import hashlib
import os
from pathlib import Path
import shutil
import signal
import socket
import stat
import subprocess
import sys
import tempfile
import time


READY_MARKER = "[co-op client] campaign synchronized and ready"
CREDENTIAL_FILE = "client-reconnect-credential.bin"
CREDENTIAL_SIZE = 224
CAMPAIGN_ID = "installed-smoke"
CAMPAIGN_SEED = "20260829"


class SmokeFailure(RuntimeError):
    pass


class ManagedProcess:
    def __init__(self, name: str, process: subprocess.Popen, log_path: Path,
                 log_file) -> None:
        self.name = name
        self.process = process
        self.log_path = log_path
        self.log_file = log_file

    def close_log(self) -> None:
        if not self.log_file.closed:
            self.log_file.close()


class ProcessSupervisor:
    def __init__(self) -> None:
        self.processes: list[ManagedProcess] = []

    def start(self, name: str, command: list[str], cwd: Path,
              environment: dict[str, str]) -> ManagedProcess:
        log_path = cwd / f"{name}.stdout.log"
        log_file = log_path.open("wb", buffering=0)
        try:
            process = subprocess.Popen(
                command,
                cwd=str(cwd),
                env=environment,
                stdin=subprocess.DEVNULL,
                stdout=log_file,
                stderr=subprocess.STDOUT,
                start_new_session=True,
            )
        except Exception:
            log_file.close()
            raise
        managed = ManagedProcess(name, process, log_path, log_file)
        self.processes.append(managed)
        return managed

    @staticmethod
    def _request_stop(managed: ManagedProcess, force: bool) -> None:
        if managed.process.poll() is not None:
            return
        requested_signal = signal.SIGKILL if force else signal.SIGTERM
        try:
            os.killpg(managed.process.pid, requested_signal)
        except ProcessLookupError:
            return

    def stop(self, managed: ManagedProcess, expected_exit: int,
             timeout_seconds: float) -> None:
        if managed.process.poll() is None:
            self._request_stop(managed, False)
        try:
            exit_code = managed.process.wait(timeout=timeout_seconds)
        except subprocess.TimeoutExpired:
            self._request_stop(managed, True)
            managed.process.wait(timeout=10)
            managed.close_log()
            raise SmokeFailure(
                f"{managed.name} did not stop within {timeout_seconds:.0f}s")
        managed.close_log()
        if exit_code != expected_exit:
            raise SmokeFailure(
                f"{managed.name} exited with {exit_code}, expected {expected_exit}")

    def cleanup(self) -> None:
        for managed in reversed(self.processes):
            try:
                if managed.process.poll() is None:
                    self._request_stop(managed, False)
                    try:
                        managed.process.wait(timeout=10)
                    except subprocess.TimeoutExpired:
                        self._request_stop(managed, True)
                        managed.process.wait(timeout=10)
            except Exception:
                try:
                    managed.process.kill()
                    managed.process.wait(timeout=10)
                except Exception:
                    pass
            finally:
                managed.close_log()


def parse_arguments() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--executable", required=True, type=Path)
    parser.add_argument("--installed-data-root", required=True, type=Path)
    parser.add_argument("--phase-timeout-seconds", type=float, default=180.0)
    parser.add_argument(
        "--keep-artifacts",
        action="store_true",
        help="retain the private run root after completion for diagnostics",
    )
    return parser.parse_args()


def require_absolute_existing_paths(executable: Path, installed_root: Path) \
        -> tuple[Path, Path]:
    if os.name != "posix":
        raise SmokeFailure(
            "the installed-process smoke currently requires POSIX process-group "
            "SIGTERM semantics for the clean final checkpoint")
    if not executable.is_absolute() or not installed_root.is_absolute():
        raise SmokeFailure("the executable and installed-data root must be absolute")
    executable = executable.resolve(strict=True)
    installed_root = installed_root.resolve(strict=True)
    if not executable.is_file() or not os.access(executable, os.X_OK):
        raise SmokeFailure(f"not an executable regular file: {executable}")
    if not installed_root.is_dir():
        raise SmokeFailure(f"not an installed-data directory: {installed_root}")
    return executable, installed_root


def selected_configuration(installed_root: Path) -> tuple[Path, Path]:
    ja2_ini = installed_root / "Ja2.ini"
    if not ja2_ini.is_file():
        raise SmokeFailure(f"installed-data root has no Ja2.ini: {installed_root}")
    parser = configparser.ConfigParser(interpolation=None, strict=False)
    try:
        with ja2_ini.open("r", encoding="utf-8-sig", errors="strict") as source:
            parser.read_file(source)
    except (OSError, UnicodeError, configparser.Error) as error:
        raise SmokeFailure(f"cannot parse installed Ja2.ini: {error}") from error
    section = next(
        (name for name in parser.sections() if name.lower() == "ja2 settings"),
        None,
    )
    if section is None or not parser.has_option(section, "VFS_CONFIG_INI"):
        raise SmokeFailure("installed Ja2.ini has no [Ja2 Settings] VFS_CONFIG_INI")
    configuration_name = parser.get(section, "VFS_CONFIG_INI").strip()
    if (not configuration_name or "/" in configuration_name or
            "\\" in configuration_name or configuration_name in {".", ".."}):
        raise SmokeFailure("VFS_CONFIG_INI must name one file in the installed root")
    configuration = installed_root / configuration_name
    if not configuration.is_file():
        raise SmokeFailure(f"selected VFS configuration is missing: {configuration}")
    return ja2_ini, configuration


def installed_content_roots(installed_root: Path) -> list[Path]:
    roots = sorted(
        (
            child
            for child in installed_root.iterdir()
            if child.is_dir() and
            (child.name.lower() == "data" or
             child.name.lower().startswith("data-"))
        ),
        key=lambda path: path.name.lower(),
    )
    if not any(path.name.lower() == "data" for path in roots):
        raise SmokeFailure(f"installed-data root has no Data directory: {installed_root}")
    return roots


def fingerprint_installed_inputs(files: list[Path], roots: list[Path]) -> str:
    """Hash names and mutation-relevant metadata without reading game payloads."""
    digest = hashlib.sha256()

    def add(path_name: str, entry: Path) -> None:
        metadata = entry.lstat()
        digest.update(path_name.encode("utf-8", errors="surrogateescape"))
        digest.update(b"\0")
        digest.update(str(stat.S_IFMT(metadata.st_mode)).encode("ascii"))
        digest.update(b":")
        digest.update(str(metadata.st_mode & 0o7777).encode("ascii"))
        digest.update(b":")
        digest.update(str(metadata.st_size).encode("ascii"))
        digest.update(b":")
        digest.update(str(metadata.st_mtime_ns).encode("ascii"))
        if stat.S_ISLNK(metadata.st_mode):
            digest.update(b":")
            digest.update(os.readlink(entry).encode("utf-8", errors="surrogateescape"))
        digest.update(b"\n")

    for source in sorted(files, key=lambda path: path.name.lower()):
        add(f"file/{source.name}", source)
    for logical_root in roots:
        resolved_root = logical_root.resolve(strict=True)
        add(f"root/{logical_root.name}", resolved_root)
        for directory, directory_names, file_names in os.walk(
                resolved_root, followlinks=False):
            directory_names.sort()
            file_names.sort()
            directory_path = Path(directory)
            relative_directory = directory_path.relative_to(resolved_root)
            for name in directory_names:
                entry = directory_path / name
                add(f"{logical_root.name}/{relative_directory}/{name}", entry)
            for name in file_names:
                entry = directory_path / name
                add(f"{logical_root.name}/{relative_directory}/{name}", entry)
    return digest.hexdigest()


def make_private_directory(path: Path) -> None:
    path.mkdir(mode=0o700, parents=False, exist_ok=False)
    os.chmod(path, 0o700)


def make_run_root(path: Path, ja2_ini: Path, configuration: Path,
                  content_roots: list[Path]) -> None:
    make_private_directory(path)
    for source in (ja2_ini, configuration):
        destination = path / source.name
        shutil.copyfile(source, destination)
        os.chmod(destination, 0o600)
    for source in content_roots:
        destination = path / source.name
        try:
            destination.symlink_to(source.resolve(strict=True), target_is_directory=True)
        except OSError as error:
            raise SmokeFailure(
                f"cannot create private read-only-content link {destination}: {error}") \
                from error


def free_loopback_port() -> int:
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as listener:
        listener.bind(("127.0.0.1", 0))
        return int(listener.getsockname()[1])


def log_text(managed: ManagedProcess) -> str:
    try:
        return managed.log_path.read_text(encoding="utf-8", errors="replace")
    except OSError:
        return ""


def log_tail(managed: ManagedProcess, maximum_lines: int = 80) -> str:
    lines = log_text(managed).splitlines()
    return "\n".join(lines[-maximum_lines:])


def wait_for_marker(managed: ManagedProcess, marker: str,
                    timeout_seconds: float) -> None:
    deadline = time.monotonic() + timeout_seconds
    while time.monotonic() < deadline:
        if marker in log_text(managed):
            return
        exit_code = managed.process.poll()
        if exit_code is not None:
            managed.close_log()
            raise SmokeFailure(
                f"{managed.name} exited with {exit_code} before marker: {marker}")
        time.sleep(0.1)
    raise SmokeFailure(
        f"{managed.name} did not publish marker within {timeout_seconds:.0f}s: "
        f"{marker}")


def credential_record(state_root: Path) -> tuple[Path, bytes]:
    matches: list[Path] = []
    for directory, directory_names, file_names in os.walk(
            state_root, followlinks=False):
        directory_names[:] = sorted(
            name for name in directory_names
            if not (Path(directory) / name).is_symlink())
        if CREDENTIAL_FILE in file_names:
            matches.append(Path(directory) / CREDENTIAL_FILE)
    if len(matches) != 1:
        raise SmokeFailure(
            f"expected one durable reconnect credential, found {len(matches)}")
    credential = matches[0]
    metadata = credential.lstat()
    if not stat.S_ISREG(metadata.st_mode) or metadata.st_size != CREDENTIAL_SIZE:
        raise SmokeFailure(
            f"credential is not a {CREDENTIAL_SIZE}-byte regular file: {credential}")
    if metadata.st_mode & 0o077:
        raise SmokeFailure(f"credential permissions are not private: {credential}")
    record = credential.read_bytes()
    if len(record) != CREDENTIAL_SIZE:
        raise SmokeFailure("credential changed while it was being captured")
    return credential, record


def process_environment() -> dict[str, str]:
    environment = os.environ.copy()
    environment["SDL_VIDEODRIVER"] = "dummy"
    environment["SDL_AUDIODRIVER"] = "dummy"
    environment["SDL_RENDER_DRIVER"] = "software"
    return environment


def server_command(executable: Path, state_root: Path, port: int,
                   action: str) -> list[str]:
    command = [
        str(executable),
        "--dedicated",
        "--dedicated-mode=coop",
        f"--campaign={CAMPAIGN_ID}",
        f"--campaign-action={action}",
        f"--dedicated-state-dir={state_root}",
        "--checkpoint-seconds=30",
        "--dedicated-coop-bind=127.0.0.1",
        f"--dedicated-coop-port={port}",
    ]
    if action == "new":
        command.append(f"--campaign-seed={CAMPAIGN_SEED}")
    return command


def client_command(executable: Path, state_root: Path, port: int) -> list[str]:
    return [
        str(executable),
        "--coop-client",
        "--coop-server=127.0.0.1",
        f"--coop-port={port}",
        f"--coop-state-dir={state_root}",
    ]


def run_smoke(arguments: argparse.Namespace, temporary_root: Path,
              supervisor: ProcessSupervisor, executable: Path,
              installed_root: Path, ja2_ini: Path, configuration: Path,
              content_roots: list[Path]) -> None:
    del installed_root  # The child processes receive only private launch roots.
    server_state = temporary_root / "server-state"
    client_state = temporary_root / "client-state"
    make_private_directory(server_state)
    make_private_directory(client_state)

    run_roots = {
        name: temporary_root / name
        for name in ("server-create", "client-first", "client-reconnect",
                     "server-resume")
    }
    for run_root in run_roots.values():
        make_run_root(run_root, ja2_ini, configuration, content_roots)

    environment = process_environment()
    first_port = free_loopback_port()
    server = supervisor.start(
        "server-create",
        server_command(executable, server_state, first_port, "new"),
        run_roots["server-create"],
        environment,
    )
    wait_for_marker(
        server,
        f"[dedicated] co-op campaign created; admission listening on "
        f"127.0.0.1:{first_port}",
        arguments.phase_timeout_seconds,
    )

    first_client = supervisor.start(
        "client-first",
        client_command(executable, client_state, first_port),
        run_roots["client-first"],
        environment,
    )
    wait_for_marker(first_client, READY_MARKER, arguments.phase_timeout_seconds)
    credential_path, first_credential = credential_record(client_state)
    supervisor.stop(first_client, expected_exit=0, timeout_seconds=30)

    # Give the authoritative main loop a committed frame to observe the closed
    # transport and reset its ten-second starter gather grace before reconnect.
    time.sleep(1.0)
    reconnect_client = supervisor.start(
        "client-reconnect",
        client_command(executable, client_state, first_port),
        run_roots["client-reconnect"],
        environment,
    )
    wait_for_marker(
        reconnect_client, READY_MARKER, arguments.phase_timeout_seconds)
    second_credential_path, second_credential = credential_record(client_state)
    if second_credential_path != credential_path:
        raise SmokeFailure("same-root reconnect moved the durable credential")
    if second_credential != first_credential:
        raise SmokeFailure("same-epoch process reconnect changed its credential")
    supervisor.stop(reconnect_client, expected_exit=0, timeout_seconds=30)

    # Stop before the one-peer gather grace can launch the tactical world.  A
    # zero exit is proof that the main-thread SIGTERM path stopped admission,
    # committed the final cold strategic checkpoint, and completed teardown.
    time.sleep(1.0)
    supervisor.stop(server, expected_exit=0, timeout_seconds=120)

    second_port = free_loopback_port()
    resumed_server = supervisor.start(
        "server-resume",
        server_command(executable, server_state, second_port, "resume"),
        run_roots["server-resume"],
        environment,
    )
    wait_for_marker(
        resumed_server,
        f"[dedicated] co-op campaign resumed; admission listening on "
        f"127.0.0.1:{second_port}",
        arguments.phase_timeout_seconds,
    )
    supervisor.stop(resumed_server, expected_exit=0, timeout_seconds=120)


def main() -> int:
    arguments = parse_arguments()
    supervisor = ProcessSupervisor()
    temporary_root: Path | None = None
    protected_before: str | None = None
    protected_after: str | None = None
    error: Exception | None = None
    try:
        executable, installed_root = require_absolute_existing_paths(
            arguments.executable, arguments.installed_data_root)
        ja2_ini, configuration = selected_configuration(installed_root)
        content_roots = installed_content_roots(installed_root)
        protected_before = fingerprint_installed_inputs(
            [ja2_ini, configuration], content_roots)
        temporary_root = Path(tempfile.mkdtemp(prefix="ja2-coop-installed-smoke-"))
        os.chmod(temporary_root, 0o700)
        run_smoke(
            arguments,
            temporary_root,
            supervisor,
            executable,
            installed_root,
            ja2_ini,
            configuration,
            content_roots,
        )
    except Exception as caught:  # Keep diagnostics consistent for CTest.
        error = caught
    finally:
        supervisor.cleanup()

    try:
        if protected_before is not None:
            executable, installed_root = require_absolute_existing_paths(
                arguments.executable, arguments.installed_data_root)
            del executable
            ja2_ini, configuration = selected_configuration(installed_root)
            content_roots = installed_content_roots(installed_root)
            protected_after = fingerprint_installed_inputs(
                [ja2_ini, configuration], content_roots)
            if protected_after != protected_before:
                raise SmokeFailure("installed game-data inputs changed during smoke")
    except Exception as caught:
        if error is None:
            error = caught

    if error is not None:
        print(f"FAIL: {error}", file=sys.stderr)
        for managed in supervisor.processes:
            tail = log_tail(managed)
            if tail:
                print(f"\n--- {managed.name} log tail ---\n{tail}", file=sys.stderr)
    else:
        credential = next(
            (
                path
                for path in temporary_root.rglob(CREDENTIAL_FILE)
                if path.is_file()
            ),
            None,
        )
        credential_sha = (
            hashlib.sha256(credential.read_bytes()).hexdigest()
            if credential is not None else "unavailable"
        )
        print(
            "installed full-engine co-op process smoke passed: "
            f"create, Ready, exact reconnect credential {credential_sha}, "
            "clean checkpoint, resume"
        )

    if temporary_root is not None:
        if arguments.keep_artifacts:
            print(f"artifacts retained at {temporary_root}")
        else:
            shutil.rmtree(temporary_root)
    return 0 if error is None else 1


if __name__ == "__main__":
    raise SystemExit(main())
