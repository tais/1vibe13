#!/usr/bin/env python3
"""Installed late join: the second player gets mercs.

The starter battle begins once the first player is ready (after a short
gather grace), so a second player who starts a minute later used to own
nothing: all four mercs were already dealt to the first, and stable
ownership never re-deals. Client A joins alone and is dealt all four; then
client B joins the running battle and takes a fair share, the last two mercs.
Client A keeps the first two, and both clients agree. Replicated state, not
physical OS input or painted pixels.
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
SHARE_MARKER = "[dedicated] a joining player took over 2 merc(s)"


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


def late_share(before, after):
    """None when `after` is the expected share of `before` (all four held by
    client 0): client 0 keeps the first two in actor order, client 1 holds
    the last two. Otherwise a description of what differs."""
    order = sorted(before)
    if len(order) != 4 or set(before.values()) != {0}:
        return f"client A did not hold all four first: {before}"
    expected = {actor: (0 if index < 2 else 1) for index, actor in enumerate(order)}
    if after != expected:
        return f"expected {expected}, got {after}"
    return None


def main(argv=None):
    args = parse_arguments(argv)
    fixture, client_exe = (path.resolve(strict=True) for path in (args.fixture_executable, args.client_executable))
    if fixture.name != "full_engine_coop_installed_entry":
        raise SmokeFailure("late-join probe requires the test-only host")
    installed = args.installed_data_root.resolve(strict=True)
    ja2_ini, configuration = selected_configuration(installed)
    roots = installed_content_roots(installed)
    run = Path(tempfile.mkdtemp(prefix="ja2-coop-late-join-"))
    print(f"Installed late-join logs: {run}", flush=True)
    for name in ("server", "client-a", "client-b"):
        make_run_root(run / name, ja2_ini, configuration, roots)
        make_private_directory(run / f"{name}-state")
    evidence = {"qualification_scope": "second_player_joins_running_starter_battle", "seeded_native_fixture": False,
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
        first = supervisor.start("client-a", client_command(client_exe, run / "client-a-state", port), run / "client-a",
                                 environment, driven=True)
        wait_for_markers([(first, READY_MARKER), (server, TACTICAL_SERVER_MARKER),
                          (first, TACTICAL_PRESENTATION_READY_MARKER)], timeout)
        deadline = time.monotonic() + timeout
        alone = wait_ownership([_Probe(first)], qualification_guard(server, [first]), deadline,
                               lambda owners: len(owners) == 4)
        evidence["alone"] = {f"{a[0]}:{a[1]}": owner for a, owner in alone.items()}
        second = supervisor.start("client-b", client_command(client_exe, run / "client-b-state", port),
                                  run / "client-b", environment, driven=True)
        clients = [first, second]
        wait_for_markers([(second, READY_MARKER), (second, TACTICAL_PRESENTATION_READY_MARKER)], timeout)
        guard = qualification_guard(server, clients)
        probes = [_Probe(first), _Probe(second)]
        shared = wait_ownership(probes, guard, time.monotonic() + timeout,
                                lambda owners: len(owners) == 4 and sorted(owners.values()) == [0, 0, 1, 1])
        wait_for_marker(server, SHARE_MARKER, timeout)
        time.sleep(3)
        settled = ownership(_states(probes, time.monotonic() + 10, guard)) or {}
        evidence["shared"] = {f"{a[0]}:{a[1]}": owner for a, owner in settled.items()}
        problem = late_share(alone, settled) if shared is not None else "no shared ownership"
        if problem:
            raise SmokeFailure(f"the late player's share is wrong: {problem}")
        for client in clients:
            supervisor.stop(client, expected_exit=0, timeout_seconds=30)
        evidence["passed"] = True
        print("PASS: client B joined the running battle and took the last two mercs; client A kept the first two.",
              flush=True)
        return 0
    except Exception as error:
        evidence["failure"] = str(error)
        print(f"Installed late join failed: {error}", flush=True)
        return 1
    finally:
        supervisor.cleanup()
        with (run / "late-join-evidence.json").open("w", encoding="utf-8") as output:
            json.dump(evidence, output, indent=2, default=str)
            output.write("\n")


if __name__ == "__main__":
    raise SystemExit(main())
