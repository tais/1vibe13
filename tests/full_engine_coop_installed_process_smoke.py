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
import re
import shutil
import json
import signal
import socket
import stat
import subprocess
import sys
import tempfile
import time

from full_engine_coop_installed_movement import (
    _Probe,
    _guard,
    _states,
    certify_two_client_movement,
    verify_reconnected_movement,
)
from full_engine_coop_installed_fire import (
    certify_two_client_fire,
    wait_for_native_fire_readiness,
)
from full_engine_coop_installed_reload import certify_two_client_reload
from full_engine_coop_installed_turns import certify_two_client_turn_cycle
from full_engine_coop_installed_inventory import certify_two_client_inventory
from full_engine_coop_installed_transfer import certify_two_client_transfer
from full_engine_coop_installed_portraits import certify_two_client_portraits
from full_engine_coop_installed_loot import certify_two_client_loot_views
from full_engine_coop_installed_drop import certify_two_client_drop
from full_engine_coop_installed_drop_input import certify_two_client_drop_input
from full_engine_coop_installed_first_aid import certify_two_client_first_aid


READY_MARKER = "[co-op client] campaign synchronized and ready"
TACTICAL_READY_MARKER = (
    "[co-op client] tactical baseline committed; assigned actors ready"
)
TACTICAL_MAP_VERIFIED_MARKER = (
    "[co-op client] exact tactical map verified: "
)
TACTICAL_PRESENTATION_READY_MARKER = (
    "[co-op client] tactical presentation ready: world="
)
TACTICAL_SERVER_MARKER = "[dedicated] co-op tactical encounter playable with "
TACTICAL_SHUTDOWN_MARKER = (
    "[dedicated] final co-op checkpoint failed: checkpoint not eligible"
)
RECONNECTING_MARKER = "[co-op client] reconnecting to server; reason="
CONNECTION_RESTORED_MARKER = "[co-op client] server connection restored"
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
        if self.process.stdin is not None and not self.process.stdin.closed:
            self.process.stdin.close()


class ProcessSupervisor:
    def __init__(self) -> None:
        self.processes: list[ManagedProcess] = []

    def start(self, name: str, command: list[str], cwd: Path,
              environment: dict[str, str], *, driven: bool = False) -> ManagedProcess:
        log_path = cwd / f"{name}.stdout.log"
        log_file = log_path.open("wb", buffering=0)
        try:
            process = subprocess.Popen(
                command,
                cwd=str(cwd),
                env=environment,
                stdin=subprocess.PIPE if driven else subprocess.DEVNULL,
                bufsize=0,
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
    parser.add_argument(
        "--gameplay-client-executable", type=Path,
        help="opt-in full_engine_coop_installed_client test target for real movement",
    )
    parser.add_argument("--movement-timeout-seconds", type=float, default=60.0)
    parser.add_argument("--verify-campaign-status", action="store_true",
                        help="read-only shared clock/leader, leader disconnect/reconnect and stale-status cleanup")
    parser.add_argument("--campaign-status-timeout-seconds", type=float, default=90.0)
    parser.add_argument("--verify-campaign-groups", action="store_true",
                        help="read-only friendly group agreement, reconnect and stale-roster cleanup; no travel orders")
    parser.add_argument("--campaign-groups-timeout-seconds", type=float, default=90.0)
    parser.add_argument(
        "--verify-inventory", action="store_true",
        help="before fire, equip/restore through controller and normal UI clicks; verify owner-only pocket replacements",
    )
    parser.add_argument("--inventory-timeout-seconds", type=float, default=90.0)
    parser.add_argument(
        "--verify-equipment", action="store_true",
        help="after inventory, certify supported real equipment roundtrips; no physical OS input or paint claim",
    )
    parser.add_argument("--equipment-timeout-seconds", type=float, default=90.0)
    parser.add_argument(
        "--verify-transfer", action="store_true",
        help="before fire, give and return an ordinary stack between nearby owned mercs through the controller",
    )
    parser.add_argument("--transfer-timeout-seconds", type=float, default=60.0)
    parser.add_argument(
        "--verify-drop", action="store_true",
        help="drop one ordinary pocket stack and recover its exact ground identity through the controller",
    )
    parser.add_argument("--drop-timeout-seconds", type=float, default=90.0)
    parser.add_argument(
        "--verify-drop-input", action="store_true",
        help="after optional drop, certify shipping Screen queued drop/pickup input; not physical OS input or pixels",
    )
    parser.add_argument("--drop-input-timeout-seconds", type=float, default=90.0)
    parser.add_argument(
        "--verify-loot", action="store_true",
        help="read-only owner/nearby-loot model correlation; empty views do not prove marker painting or pickup",
    )
    parser.add_argument("--loot-timeout-seconds", type=float, default=30.0)
    parser.add_argument(
        "--verify-portraits", action="store_true",
        help="before fire, verify owned portrait descriptors and capture both native rosters for visual inspection",
    )
    parser.add_argument("--portraits-timeout-seconds", type=float, default=30.0)
    parser.add_argument(
        "--verify-fire", action="store_true",
        help="after movement/reconnect, require one native shot's ammunition outcome on both clients",
    )
    parser.add_argument("--fire-timeout-seconds", type=float, default=90.0)
    parser.add_argument(
        "--verify-reload", action="store_true",
        help="after native fire settles, require loaded-round replenishment on both clients",
    )
    parser.add_argument("--reload-timeout-seconds", type=float, default=30.0)
    parser.add_argument(
        "--verify-turn-cycle", action="store_true",
        help="after combat actions, require a full shared player/AI/player turn cycle",
    )
    parser.add_argument("--turn-cycle-timeout-seconds", type=float, default=120.0)
    parser.add_argument(
        "--verify-first-aid", action="store_true",
        help="last, require an already equipped real kit and adjacent injured teammate; verify treatment without setup gameplay",
    )
    parser.add_argument("--first-aid-timeout-seconds", type=float, default=90.0)
    parser.add_argument("--phase-timeout-seconds", type=float, default=180.0)
    parser.add_argument(
        "--tactical-timeout-seconds",
        type=float,
        default=360.0,
        help=(
            "shared deadline for concurrent client cold-load, tactical-world "
            "start, and assigned-baseline evidence"
        ),
    )
    parser.add_argument(
        "--keep-artifacts",
        action="store_true",
        help="retain the private run root after completion for diagnostics",
    )
    arguments = parser.parse_args()
    for name in ("equipment", "drop-input", "first-aid", "campaign-status", "campaign-groups"):
        attribute = name.replace("-", "_")
        if getattr(arguments, f"verify_{attribute}") and arguments.gameplay_client_executable is None:
            parser.error(f"--verify-{name} requires --gameplay-client-executable")
        if not 1 <= getattr(arguments, f"{attribute}_timeout_seconds") <= 300:
            parser.error(f"--{name}-timeout-seconds must be between 1 and 300 seconds")
    if arguments.verify_inventory and arguments.gameplay_client_executable is None:
        parser.error("--verify-inventory requires --gameplay-client-executable")
    if not 0 < arguments.inventory_timeout_seconds <= 300:
        parser.error("--inventory-timeout-seconds must be positive and at most 300 seconds")
    if arguments.verify_transfer and arguments.gameplay_client_executable is None:
        parser.error("--verify-transfer requires --gameplay-client-executable")
    if not 0 < arguments.transfer_timeout_seconds <= 300:
        parser.error("--transfer-timeout-seconds must be positive and at most 300 seconds")
    if arguments.verify_drop and arguments.gameplay_client_executable is None:
        parser.error("--verify-drop requires --gameplay-client-executable")
    if not 0 < arguments.drop_timeout_seconds <= 300:
        parser.error("--drop-timeout-seconds must be positive and at most 300 seconds")
    if arguments.verify_loot and arguments.gameplay_client_executable is None:
        parser.error("--verify-loot requires --gameplay-client-executable")
    if not 0 < arguments.loot_timeout_seconds <= 300:
        parser.error("--loot-timeout-seconds must be positive and at most 300 seconds")
    if arguments.verify_portraits and arguments.gameplay_client_executable is None:
        parser.error("--verify-portraits requires --gameplay-client-executable")
    if not 0 < arguments.portraits_timeout_seconds <= 300:
        parser.error("--portraits-timeout-seconds must be positive and at most 300 seconds")
    if arguments.verify_reload and not arguments.verify_fire:
        parser.error("--verify-reload requires --verify-fire")
    if arguments.verify_turn_cycle and not arguments.verify_fire:
        parser.error("--verify-turn-cycle requires --verify-fire")
    if arguments.verify_fire and arguments.gameplay_client_executable is None:
        parser.error("--verify-fire requires --gameplay-client-executable")
    if not 0 < arguments.fire_timeout_seconds <= 300:
        parser.error("--fire-timeout-seconds must be positive and at most 300 seconds")
    if not 0 < arguments.reload_timeout_seconds <= 300:
        parser.error("--reload-timeout-seconds must be positive and at most 300 seconds")
    if not 0 < arguments.turn_cycle_timeout_seconds <= 300:
        parser.error("--turn-cycle-timeout-seconds must be positive and at most 300 seconds")
    return arguments


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
    # STATE replies can contain the entire public actor roster. Keep the last
    # two probes while retaining ordinary lifecycle/failure diagnostics; the
    # complete replies remain in the private artifact log.
    probe_indices = [index for index, line in enumerate(lines)
                     if line.startswith("[co-op test] ")]
    omitted = set(probe_indices[:-2])
    lines = [line for index, line in enumerate(lines) if index not in omitted]
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


def wait_for_markers(requirements: list[tuple[ManagedProcess, str]],
                     timeout_seconds: float) -> None:
    """Wait for concurrent process evidence against one shared deadline."""
    pending = list(requirements)
    deadline = time.monotonic() + timeout_seconds
    while pending and time.monotonic() < deadline:
        logs = {
            managed: log_text(managed)
            for managed, _marker in pending
        }
        remaining: list[tuple[ManagedProcess, str]] = []
        for managed, marker in pending:
            if marker in logs[managed]:
                continue
            exit_code = managed.process.poll()
            if exit_code is not None:
                managed.close_log()
                raise SmokeFailure(
                    f"{managed.name} exited with {exit_code} before marker: "
                    f"{marker}")
            remaining.append((managed, marker))
        pending = remaining
        if pending:
            time.sleep(0.1)
    if pending:
        missing = "; ".join(
            f"{managed.name}: {marker}" for managed, marker in pending)
        raise SmokeFailure(
            f"concurrent tactical phase did not publish all markers within "
            f"{timeout_seconds:.0f}s: {missing}")


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


def tactical_identity(managed: ManagedProcess) -> tuple[int, str]:
    matches = re.findall(
        r"\[co-op client\] tactical presentation ready: world=(\d+); "
        r"revision=\d+; map=([^\s;]+)", log_text(managed))
    if len(matches) != 1 or int(matches[0][0]) == 0:
        raise SmokeFailure(
            f"{managed.name} did not report one valid tactical world identity")
    return int(matches[0][0]), matches[0][1]


def require_running(processes: list[ManagedProcess]) -> None:
    for managed in processes:
        exit_code = managed.process.poll()
        if exit_code is not None:
            raise SmokeFailure(
                f"{managed.name} exited unexpectedly with {exit_code}")


def require_reconnecting(clients: tuple[ManagedProcess, ...],
                         before_stop: dict[ManagedProcess, str],
                         timeout_seconds: float, *,
                         verify_public_state: bool = False) -> None:
    # Check only evidence emitted after the authority stopped. An old retry
    # during startup cannot certify the live connection-loss transition.
    offsets = {client: len(before_stop[client]) for client in clients}
    ready_counts = {client: before_stop[client].count(READY_MARKER)
                    for client in clients}
    deadline = time.monotonic() + timeout_seconds
    stable_since: float | None = None
    probes = [_Probe(client) for client in clients] if verify_public_state else []
    # The authority is deliberately stopped. Only the still-running clients
    # belong in this guard while their public presentation is inspected.
    guard = _guard(clients) if probes else None
    while time.monotonic() < deadline:
        require_running(list(clients))
        ready = True
        for client in clients:
            current = log_text(client)
            after_stop = current[offsets[client]:]
            if "[co-op client] runtime failed:" in after_stop:
                raise SmokeFailure(
                    f"{client.name} failed instead of retaining its reconnectable seat")
            if (current.count(READY_MARKER) != ready_counts[client] or
                    CONNECTION_RESTORED_MARKER in after_stop):
                raise SmokeFailure(
                    f"{client.name} claimed recovery while the server was absent")
            count = after_stop.count(RECONNECTING_MARKER)
            if count > 1:
                raise SmokeFailure(
                    f"{client.name} repeated its connection-loss transition log")
            ready = ready and count == 1
        if ready:
            if probes:
                snapshots = _states(probes, deadline, guard)
                if len(snapshots) != len(clients):
                    raise SmokeFailure("connection-loss probes did not cover every client")
                for client, snapshot in zip(clients, snapshots):
                    # STATE exposes the public presentation view. Its absent
                    # view emits these empty fields; this does not inspect an
                    # internal command queue or prove ambiguous-intent replay.
                    if (snapshot.get("ready") is not False or
                            snapshot.get("world") != 0 or
                            snapshot.get("revision") != 0 or
                            snapshot.get("turn_serial") != 0 or
                            snapshot.get("assigned") != [] or
                            snapshot.get("actors") != [] or
                            snapshot.get("outstanding") != 0 or
                            snapshot.get("receipt") is not None):
                        raise SmokeFailure(
                            f"{client.name} retained stale public tactical state "
                            "after its connection-loss transition")
            if stable_since is None:
                stable_since = time.monotonic()
            # Exercise several one-second retry intervals without a terminal
            # failure, false Ready, or a log line on every retry/frame.
            if time.monotonic() - stable_since >= 3.0:
                return
        else:
            stable_since = None
        time.sleep(0.1)
    raise SmokeFailure("clients did not remain in automatic reconnection")


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


def record_bounded_probe_evidence(temporary_root: Path, name: str, evidence: dict) -> Path:
    """Keep new summary/input proofs explicit about what they do not certify."""
    if name not in ("equipment", "drop-input", "first-aid", "campaign-status", "campaign-groups"):
        raise SmokeFailure("unknown bounded installed probe evidence kind")
    if not isinstance(evidence, dict) or any(evidence.get(field) is not False for field in
            ("paint_verified", "physical_input_verified", "full_object_graphs_verified")):
        raise SmokeFailure(f"{name} evidence must disclaim paint, physical OS input and hidden object-graph coverage")
    if name == "campaign-status" and any(evidence.get(field) is not False for field in
            ("time_controls_verified", "offline_pause_verified")):
        raise SmokeFailure("campaign status observations do not certify time controls or offline pause")
    if name == "campaign-groups" and evidence.get("travel_orders_verified") is not False:
        raise SmokeFailure("group observations do not certify travel orders")
    path = temporary_root / f"{name}-evidence.json"
    with path.open("x", encoding="utf-8") as evidence_file:
        json.dump(evidence, evidence_file, indent=2, allow_nan=False)
        evidence_file.write("\n")
    print(f"installed {name} evidence: {path}; no physical OS input, painted pixels or hidden object-graph claim", flush=True)
    return path


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
    tactical_client_states = (
        temporary_root / "tactical-client-a-state",
        temporary_root / "tactical-client-b-state",
    )
    make_private_directory(server_state)
    make_private_directory(client_state)
    for state_root in tactical_client_states:
        make_private_directory(state_root)

    run_roots = {
        name: temporary_root / name
        for name in ("server-create", "client-first", "client-reconnect",
                     "server-resume", "tactical-client-a",
                     "tactical-client-b", "tactical-client-b-reconnect", "tactical-client-a-reconnect")
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

    print("installed lifecycle: campaign created; starting first client", flush=True)
    first_client = supervisor.start(
        "client-first",
        client_command(executable, client_state, first_port),
        run_roots["client-first"],
        environment,
    )
    wait_for_marker(first_client, READY_MARKER, arguments.phase_timeout_seconds)
    print("installed lifecycle: first client Ready; checking restart", flush=True)
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
    print("installed lifecycle: reconnect credential preserved; checking checkpoint", flush=True)
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
    print("installed lifecycle: checkpoint resumed; starting tactical clients", flush=True)
    # Keep the successfully resumed server and take it into tactical play. Both
    # clients start before waiting on either one, allowing the server's bounded
    # gather window to observe them concurrently. The client marker is emitted
    # only after a baseline transaction commits, its ACK is sent, and a non-empty
    # assigned-actor set is installed for that authoritative world. The tactical
    # deadline separately covers two concurrent installed-data cold loads plus
    # the server's bounded starter-actor arrival window.
    tactical_port = second_port
    tactical_server = resumed_server
    gameplay_enabled = arguments.gameplay_client_executable is not None
    tactical_executable = arguments.gameplay_client_executable or executable
    tactical_clients = (
        supervisor.start(
            "tactical-client-a",
            client_command(tactical_executable, tactical_client_states[0], tactical_port),
            run_roots["tactical-client-a"],
            environment,
            driven=gameplay_enabled,
        ),
        supervisor.start(
            "tactical-client-b",
            client_command(tactical_executable, tactical_client_states[1], tactical_port),
            run_roots["tactical-client-b"],
            environment,
            driven=gameplay_enabled,
        ),
    )
    wait_for_markers(
        [
            (tactical_clients[0], READY_MARKER),
            (tactical_clients[1], READY_MARKER),
            (tactical_server, TACTICAL_SERVER_MARKER),
            (tactical_clients[0], TACTICAL_MAP_VERIFIED_MARKER),
            (tactical_clients[1], TACTICAL_MAP_VERIFIED_MARKER),
            (tactical_clients[0], TACTICAL_READY_MARKER),
            (tactical_clients[1], TACTICAL_READY_MARKER),
            (tactical_clients[0], TACTICAL_PRESENTATION_READY_MARKER),
            (tactical_clients[1], TACTICAL_PRESENTATION_READY_MARKER),
        ],
        arguments.tactical_timeout_seconds,
    )

    require_running([tactical_server, *tactical_clients])
    initial_identity = tactical_identity(tactical_clients[0])
    if tactical_identity(tactical_clients[1]) != initial_identity:
        raise SmokeFailure("two clients presented different tactical worlds")
    print("installed lifecycle: both tactical clients renderer-ready", flush=True)
    campaign_evidence = None
    groups_evidence = None
    if arguments.verify_campaign_groups:
        from full_engine_coop_installed_groups import wait_shared as wait_groups, verify_cleared as groups_cleared
        groups_evidence = {"before": wait_groups(tactical_clients, tactical_server, arguments.campaign_groups_timeout_seconds),
                           "paint_verified": False, "physical_input_verified": False,
                           "full_object_graphs_verified": False, "travel_orders_verified": False}
        print("installed campaign: both clients agree on all four live grouped mercenaries", flush=True)
    reconnect_index = 1
    if arguments.verify_campaign_status:
        from full_engine_coop_installed_campaign_status import wait_shared, wait_leader_offline, verify_cleared
        shared = wait_shared(tactical_clients, tactical_server, arguments.campaign_status_timeout_seconds)
        reconnect_index = next(index for index, status in enumerate(shared) if status["local_leader"])
        campaign_evidence = {"before": shared, "leader_client_index": reconnect_index,
                             "paint_verified": False, "physical_input_verified": False,
                             "full_object_graphs_verified": False, "time_controls_verified": False,
                             "offline_pause_verified": False}
        print("installed campaign: both clients agree on server clock and exactly one time leader", flush=True)
    movement_evidence = None
    if gameplay_enabled:
        movement_evidence = certify_two_client_movement(
            tactical_clients, tactical_server, arguments.movement_timeout_seconds)
        print("installed gameplay: both clients completed authoritative movement", flush=True)
    tactical_credential_path, tactical_credential = credential_record(
        tactical_client_states[reconnect_index])
    observer_index = 1 - reconnect_index
    supervisor.stop(tactical_clients[reconnect_index], expected_exit=0, timeout_seconds=30)
    if arguments.verify_campaign_status:
        campaign_evidence["leader_offline"] = wait_leader_offline(
            tactical_clients[observer_index], tactical_server, campaign_evidence["before"][observer_index],
            arguments.campaign_status_timeout_seconds)
        print("installed campaign: remaining client observes offline leader without taking over", flush=True)
    time.sleep(1.0)
    reconnect_name = f"tactical-client-{'a' if reconnect_index == 0 else 'b'}-reconnect"
    restarted_client = supervisor.start(
        reconnect_name,
        client_command(tactical_executable, tactical_client_states[reconnect_index], tactical_port),
        run_roots[reconnect_name],
        environment,
        driven=gameplay_enabled,
    )
    wait_for_markers(
        [(restarted_client, marker) for marker in (
            READY_MARKER, TACTICAL_MAP_VERIFIED_MARKER,
            TACTICAL_READY_MARKER, TACTICAL_PRESENTATION_READY_MARKER)],
        arguments.tactical_timeout_seconds,
    )
    require_running([tactical_server, tactical_clients[observer_index], restarted_client])
    if tactical_identity(restarted_client) != initial_identity:
        raise SmokeFailure("reconnected client did not restore the live tactical world")
    if credential_record(tactical_client_states[reconnect_index]) != (
            tactical_credential_path, tactical_credential):
        raise SmokeFailure("live tactical reconnect changed its durable credential")
    if movement_evidence is not None:
        verify_reconnected_movement(
            restarted_client, movement_evidence, arguments.movement_timeout_seconds,
            observer_client=tactical_clients[observer_index])
        print("installed gameplay: moved actor state survived client restart", flush=True)
    tactical_clients = tuple(restarted_client if index == reconnect_index else client
                             for index, client in enumerate(tactical_clients))
    if arguments.verify_campaign_groups:
        groups_evidence["reconnected"] = wait_groups(tactical_clients, tactical_server, arguments.campaign_groups_timeout_seconds,
                                                    previous=groups_evidence["before"])
        print("installed campaign: live friendly groups restored after reconnect", flush=True)
    if arguments.verify_campaign_status:
        campaign_evidence["reconnected"] = wait_shared(
            tactical_clients, tactical_server, arguments.campaign_status_timeout_seconds,
            previous=[campaign_evidence["leader_offline"]] * 2)
        if not campaign_evidence["reconnected"][reconnect_index]["local_leader"]:
            raise SmokeFailure("reconnected identity did not regain its original time-leader role")
        print("installed campaign: original time leader retained after reconnect", flush=True)

    inventory_evidence = None
    if arguments.verify_inventory:
        print("installed gameplay: checking owner-only inventory equip and exact restoration", flush=True)
        inventory_evidence = certify_two_client_inventory(
            tactical_clients, tactical_server, arguments.inventory_timeout_seconds)
        evidence_path = temporary_root / "inventory-evidence.json"
        with evidence_path.open("x", encoding="utf-8") as evidence_file:
            json.dump(inventory_evidence, evidence_file, indent=2, allow_nan=False)
            evidence_file.write("\n")
        print("installed gameplay: owner inventory controller/UI equip/restore and pocket-only replacement certified; "
              "original primary loadout restored before combat tests", flush=True)
        print(f"installed inventory evidence: {evidence_path}", flush=True)

    if arguments.verify_equipment:
        # Import this opt-in helper only when requested, like its gameplay.
        from full_engine_coop_installed_equipment import certify_two_client_equipment
        print("installed gameplay: checking supported real equipment roundtrips and exact owner summaries", flush=True)
        equipment_evidence = certify_two_client_equipment(
            tactical_clients, tactical_server, arguments.equipment_timeout_seconds)
        record_bounded_probe_evidence(temporary_root, "equipment", equipment_evidence)

    if arguments.verify_transfer:
        print("installed gameplay: checking same-owner inventory transfer and exact return", flush=True)
        transfer_evidence = certify_two_client_transfer(
            tactical_clients, tactical_server, arguments.transfer_timeout_seconds)
        transfer_path = temporary_root / "transfer-evidence.json"
        with transfer_path.open("x", encoding="utf-8") as evidence_file:
            json.dump(transfer_evidence, evidence_file, indent=2, allow_nan=False)
            evidence_file.write("\n")
        print(f"installed transfer evidence: {transfer_path}", flush=True)

    if arguments.verify_drop:
        print("installed gameplay: checking one pocket drop and exact ground-item recovery", flush=True)
        drop_evidence = certify_two_client_drop(
            tactical_clients, tactical_server, arguments.drop_timeout_seconds)
        drop_path = temporary_root / "drop-evidence.json"
        with drop_path.open("x", encoding="utf-8") as evidence_file:
            json.dump(drop_evidence, evidence_file, indent=2, allow_nan=False)
            evidence_file.write("\n")
        print(f"installed drop evidence: {drop_path}; controller/summary proof, not physical input or full objects", flush=True)

    if arguments.verify_drop_input:
        print("installed gameplay: checking shipping Screen queued drop/pickup input and exact recovery", flush=True)
        drop_input_evidence = certify_two_client_drop_input(
            tactical_clients, tactical_server, arguments.drop_input_timeout_seconds)
        record_bounded_probe_evidence(temporary_root, "drop-input", drop_input_evidence)

    if arguments.verify_loot:
        print("installed loot: checking read-only current owner scopes and projected nearby markers", flush=True)
        loot_evidence = certify_two_client_loot_views(
            tactical_clients, tactical_server, arguments.loot_timeout_seconds)
        loot_path = temporary_root / "loot-evidence.json"
        with loot_path.open("x", encoding="utf-8") as evidence_file:
            json.dump(loot_evidence, evidence_file, indent=2, allow_nan=False)
            evidence_file.write("\n")
        print(f"installed loot evidence: {loot_path}; no screenshot, mouse or pickup claim", flush=True)

    if arguments.verify_portraits:
        print("installed gameplay: checking authoritative portrait descriptors and capturing both native rosters", flush=True)
        portraits_evidence = certify_two_client_portraits(
            tactical_clients, tactical_server, arguments.portraits_timeout_seconds,
            inventory_evidence=inventory_evidence)
        portraits_path = temporary_root / "portraits-evidence.json"
        with portraits_path.open("x", encoding="utf-8") as evidence_file:
            json.dump(portraits_evidence, evidence_file, indent=2, allow_nan=False)
            evidence_file.write("\n")
        print(f"installed portrait evidence: {portraits_path}; retained PNGs still require visual inspection", flush=True)

    # Combat can legitimately change health, stance and turn state. Exercise it
    # only after the historical movement/reconnect assertions have completed.
    if arguments.verify_fire:
        print("installed gameplay: checking native fire and replicated ammunition", flush=True)
        fire_evidence = certify_two_client_fire(
            tactical_clients, tactical_server, arguments.fire_timeout_seconds)
        print("installed gameplay: waiting for replicated command-block gate recovery", flush=True)
        wait_for_native_fire_readiness(
            tactical_clients, tactical_server, fire_evidence, 30.0)
        print("installed gameplay: native command-block gate cleared and both replicas converged", flush=True)
        if arguments.verify_reload:
            print("installed gameplay: checking native reload and replicated loaded-round "
                  f"increase ({arguments.reload_timeout_seconds:g}s limit)", flush=True)
            reload_evidence = certify_two_client_reload(
                tactical_clients, tactical_server, fire_evidence,
                arguments.reload_timeout_seconds,
                verify_reserve_rounds=arguments.verify_inventory)
            if arguments.verify_inventory:
                reload_path = temporary_root / "reload-reserve-evidence.json"
                with reload_path.open("x", encoding="utf-8") as evidence_file:
                    json.dump(reload_evidence, evidence_file, indent=2, allow_nan=False)
                    evidence_file.write("\n")
                print("installed gameplay: typed ordinary reserve-round conservation certified; "
                      "hidden LBE and full object graphs are not covered", flush=True)
                print(f"installed reload reserve evidence: {reload_path}", flush=True)
            print("installed gameplay: native reload produced a positive loaded-round "
                  "increase on both replicas", flush=True)

    if arguments.verify_turn_cycle:
        print("installed gameplay: checking full native player/AI/player turn cycle", flush=True)
        certify_two_client_turn_cycle(
            tactical_clients, tactical_server, arguments.turn_cycle_timeout_seconds)
        print("installed gameplay: both clients completed the shared native turn cycle", flush=True)

    if arguments.verify_first_aid:
        print("installed gameplay: checking actual first-aid service, kit consumption and patient improvement; "
              "requires a real equipped kit and eligible adjacent patient, with no setup gameplay", flush=True)
        first_aid_evidence = certify_two_client_first_aid(
            tactical_clients, tactical_server, arguments.first_aid_timeout_seconds)
        record_bounded_probe_evidence(temporary_root, "first-aid", first_aid_evidence)

    # Active tactical worlds intentionally cannot produce cold strategic
    # checkpoints. SIGTERM must therefore fail closed with exit 2 and the exact
    # ineligibility marker, rather than silently claiming that this encounter
    # was persisted. Clients still own an ordinary clean process exit.
    # Stop only the authority first: both clients must surface connection loss
    # and keep retrying instead of claiming a campaign transfer is in progress.
    # SIGTERM is asynchronous, so record offsets before requesting it below.
    before_stop = {client: log_text(client) for client in tactical_clients}
    supervisor.stop(tactical_server, expected_exit=2, timeout_seconds=120)
    if TACTICAL_SHUTDOWN_MARKER not in log_text(tactical_server):
        raise SmokeFailure(
            "active tactical server did not report checkpoint ineligibility")
    if arguments.verify_fire and "Attack busy problem" in log_text(tactical_server):
        raise SmokeFailure("native firing or server cleanup reported an attack-busy underflow")
    require_reconnecting(tactical_clients, before_stop,
                         arguments.phase_timeout_seconds,
                         verify_public_state=gameplay_enabled)
    if gameplay_enabled:
        print("installed gameplay: disconnected clients expose no stale public tactical state", flush=True)
    if arguments.verify_campaign_status:
        campaign_evidence["disconnected"] = verify_cleared(tactical_clients, arguments.campaign_status_timeout_seconds)
        record_bounded_probe_evidence(temporary_root, "campaign-status", campaign_evidence)
    if arguments.verify_campaign_groups:
        groups_evidence["disconnected"] = groups_cleared(tactical_clients, arguments.campaign_groups_timeout_seconds)
        record_bounded_probe_evidence(temporary_root, "campaign-groups", groups_evidence)
    for tactical_client in tactical_clients:
        supervisor.stop(tactical_client, expected_exit=0, timeout_seconds=30)


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
        if arguments.gameplay_client_executable is not None:
            arguments.gameplay_client_executable, _ = require_absolute_existing_paths(
                arguments.gameplay_client_executable, installed_root)
            if not 0 < arguments.movement_timeout_seconds <= 600:
                raise SmokeFailure("movement timeout must be positive and at most 600 seconds")
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
        else:
            error = SmokeFailure(
                f"{error}; installed-input verification also failed: {caught}")

    if error is not None:
        print(f"FAIL: {error}", file=sys.stderr)
        for managed in supervisor.processes:
            tail = log_tail(managed)
            if tail:
                print(f"\n--- {managed.name} log tail ---\n{tail}", file=sys.stderr)
    else:
        print(
            "installed full-engine co-op process smoke passed: "
            "create, Ready, exact private reconnect credential, clean checkpoint, "
            "resume, two-client tactical world, committed assigned baselines, "
            "renderer-ready map/actor presentation, live tactical reconnect, "
            "and automatic client reconnection after server shutdown"
        )
        if arguments.gameplay_client_executable is not None:
            print("installed gameplay certification passed: A/B movement, replica agreement, and reconnect")
        if arguments.verify_fire:
            print("installed fire certification passed: ammunition outcome agreed and native command-block gate cleared on both replicas")
        if arguments.verify_inventory:
            print("installed inventory certification passed: owner-only summaries, controller/UI equip/restore, pocket-only replacements, and matching public primary loadout")
        if arguments.verify_campaign_status:
            print("installed campaign status passed: shared native clock, retained leader across disconnect/reconnect, stale status cleared; no time-speed or offline-pause claim")
        if arguments.verify_campaign_groups:
            print("installed campaign groups passed: shared friendly roster/destinations, reconnect and stale-state cleanup; no travel-order claim")
        if arguments.verify_equipment:
            print("installed equipment certification passed: see supported roundtrip categories in evidence; "
                  "no physical OS input, painted pixels or hidden object-graph claim")
        if arguments.verify_transfer:
            print("installed transfer certification passed: controller give/return, both private revisions advanced, exact owner summaries restored")
        if arguments.verify_drop:
            print("installed drop certification passed: controller drop/pickup, exact ground identity retired, owner summaries restored")
        if arguments.verify_drop_input:
            print("installed drop-input certification passed: shipping Screen queued input and exact recovery; "
                  "not physical OS input, painted pixels or hidden objects")
        if arguments.verify_loot:
            print("installed loot model correlation passed: see evidence for nonempty-view coverage; no paint, mouse or pickup claim")
        if arguments.verify_portraits:
            print("installed portrait descriptor/capture checks passed: both native roster PNGs retained; visual inspection still required")
        if arguments.verify_reload:
            print("installed reload certification passed: positive loaded-round increase agreed on both replicas; " +
                  ("typed ordinary reserve-round conservation also verified (not hidden LBE/full objects)"
                   if arguments.verify_inventory else "private reserve-round conservation was not requested"))
        if arguments.verify_turn_cycle:
            print("installed turn-cycle certification passed: shared player/AI/player progression and replica agreement")
        if arguments.verify_first_aid:
            print("installed first-aid certification passed: actual service relation, kit consumption and patient improvement; "
                  "not full healing, physical OS input, painted pixels or hidden objects")

    if temporary_root is not None:
        if arguments.keep_artifacts:
            print(f"artifacts retained at {temporary_root}")
        else:
            shutil.rmtree(temporary_root)
    return 0 if error is None else 1


if __name__ == "__main__":
    raise SystemExit(main())
