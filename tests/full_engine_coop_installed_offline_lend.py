#!/usr/bin/env python3
"""Installed offline lend: a dropped player's mercs go to the one who is here.

Two clients join the starter battle; the server deals the four mercs two
each. Client B's process is killed (a crash, not a leave). After the offline
grace the server lends B's two mercs to A for the world, so A controls all
four. Client B is relaunched with its own state folder (its reconnect
credential, the same player) and gets exactly its own two mercs back. Process
kill and relaunch; replicated state, not physical OS input or painted pixels.
"""
import argparse
import json
from pathlib import Path
import tempfile
import time

from full_engine_coop_installed_campaign import qualification_guard
from full_engine_coop_installed_movement import _Probe, _states
from full_engine_coop_installed_process_smoke import (
    READY_MARKER, TACTICAL_PRESENTATION_READY_MARKER, TACTICAL_SERVER_MARKER, ProcessSupervisor, SmokeFailure,
    client_command, free_loopback_port, installed_content_roots, make_private_directory, make_run_root,
    process_environment, selected_configuration, server_command, wait_for_marker, wait_for_markers,
)
from full_engine_coop_installed_stable_ownership import ownership, wait_ownership

ROOT = Path(__file__).resolve().parents[1]
LENT_MARKER = "[dedicated] an offline player's mercs are lent to the players here (1 left out)"
RETURNED_MARKER = "[dedicated] a returning player takes back their mercs (0 left out)"


def parse_arguments(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--fixture-executable", type=Path, default=ROOT / "build/full_engine_coop_installed_entry")
    parser.add_argument("--client-executable", type=Path, default=ROOT / "build/full_engine_coop_installed_client")
    parser.add_argument("--installed-data-root", type=Path, default=ROOT.parent / "JA2")
    parser.add_argument("--timeout-seconds", type=float, default=300)
    args = parser.parse_args(argv)
    if not 1 <= args.timeout_seconds <= 900:
        parser.error("timeout must be 1..900 seconds")
    return args


def same_split(before, after):
    """None when `after` gives every merc back to its `before` owner; else what differs."""
    moved = sorted(actor for actor, owner in before.items() if after.get(actor) != owner)
    return None if not moved and len(after) == len(before) else f"moved {moved}; after {after}"


def main(argv=None):
    args = parse_arguments(argv)
    fixture, client_exe = (path.resolve(strict=True) for path in (args.fixture_executable, args.client_executable))
    if fixture.name != "full_engine_coop_installed_entry":
        raise SmokeFailure("offline-lend probe requires the test-only host")
    installed = args.installed_data_root.resolve(strict=True)
    ja2_ini, configuration = selected_configuration(installed)
    roots = installed_content_roots(installed)
    run = Path(tempfile.mkdtemp(prefix="ja2-coop-offline-lend-"))
    print(f"Installed offline-lend logs: {run}", flush=True)
    for name in ("server", "client-a", "client-b"):
        make_run_root(run / name, ja2_ini, configuration, roots)
        make_private_directory(run / f"{name}-state")
    evidence = {"qualification_scope": "starter_battle_offline_lend_and_return", "seeded_native_fixture": False,
                "physical_input_verified": False, "paint_verified": False, "passed": False}
    supervisor = ProcessSupervisor()
    environment = process_environment()
    environment.pop("JA2_COOP_ENTRY_TEST_MODE", None)
    timeout = args.timeout_seconds
    try:
        port = free_loopback_port()
        server = supervisor.start("server", server_command(fixture, run / "server-state", port, "new"), run / "server",
                                  dict(environment, JA2_COOP_ENTRY_TEST_MODE="starter-victory"))
        wait_for_marker(server, f"[dedicated] co-op campaign created; admission listening on 127.0.0.1:{port}", timeout)
        clients = [supervisor.start(name, client_command(client_exe, run / f"{name}-state", port), run / name,
                                    environment, driven=True) for name in ("client-a", "client-b")]
        wait_for_markers([(clients[0], READY_MARKER), (clients[1], READY_MARKER), (server, TACTICAL_SERVER_MARKER),
                          (clients[0], TACTICAL_PRESENTATION_READY_MARKER),
                          (clients[1], TACTICAL_PRESENTATION_READY_MARKER)], timeout)
        guard = qualification_guard(server, clients)
        probes = [_Probe(client) for client in clients]
        before = wait_ownership(probes, guard, time.monotonic() + timeout,
                                lambda owners: len(owners) == 4 and sorted(owners.values()) == [0, 0, 1, 1])
        evidence["before"] = {f"{a[0]}:{a[1]}": owner for a, owner in before.items()}
        # Client B crashes.
        crashed = clients[1]
        ProcessSupervisor._request_stop(crashed, True)
        crashed.process.wait(timeout=30)
        crashed.close_log()
        crashed_at = time.monotonic()
        alone_guard = qualification_guard(server, [clients[0]], planned_reconnects=1)
        alone = [_Probe(clients[0])]
        wait_for_marker(server, LENT_MARKER, timeout)
        evidence["lent_after_seconds"] = round(time.monotonic() - crashed_at, 1)
        lent = wait_ownership(alone, alone_guard, time.monotonic() + timeout,
                              lambda owners: len(owners) == 4 and set(owners.values()) == {0})
        evidence["lent"] = {f"{a[0]}:{a[1]}": owner for a, owner in lent.items()}
        # Client B comes back with its own state folder: the same player.
        returned = supervisor.start("client-b-again", client_command(client_exe, run / "client-b-state", port),
                                    run / "client-b", environment, driven=True)
        clients = [clients[0], returned]
        wait_for_markers([(returned, READY_MARKER), (returned, TACTICAL_PRESENTATION_READY_MARKER)], timeout)
        wait_for_marker(server, RETURNED_MARKER, timeout)
        guard = qualification_guard(server, clients, planned_reconnects=1)
        probes = [_Probe(client) for client in clients]
        back = wait_ownership(probes, guard, time.monotonic() + timeout,
                              lambda owners: len(owners) == 4 and sorted(owners.values()) == [0, 0, 1, 1])
        time.sleep(3)
        settled = ownership(_states(probes, time.monotonic() + 10, guard)) or {}
        evidence["returned"] = {f"{a[0]}:{a[1]}": owner for a, owner in settled.items()}
        problem = same_split(before, settled) if back is not None else "no shared ownership"
        if problem:
            raise SmokeFailure(f"the returning player did not get their own mercs back: {problem}")
        for client in clients:
            supervisor.stop(client, expected_exit=0, timeout_seconds=30)
        evidence["passed"] = True
        print(f"PASS: client B's mercs were lent to A {evidence['lent_after_seconds']}s after B crashed; the "
              "relaunched B got exactly its own two back.", flush=True)
        return 0
    except Exception as error:
        evidence["failure"] = str(error)
        print(f"Installed offline lend failed: {error}", flush=True)
        return 1
    finally:
        supervisor.cleanup()
        with (run / "offline-lend-evidence.json").open("w", encoding="utf-8") as output:
            json.dump(evidence, output, indent=2, default=str)
            output.write("\n")


if __name__ == "__main__":
    raise SystemExit(main())
