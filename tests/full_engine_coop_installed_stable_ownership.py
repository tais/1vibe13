#!/usr/bin/env python3
"""Installed stable merc ownership.

Two clients join the starter battle; the server deals the four mercs two
each. The test host then knocks one merc unconscious through the stock damage
path. That merc leaves its owner's control (it cannot act), and every other
merc keeps its owner on both clients. Before the ownership ledger the server
re-dealt the remaining three round-robin, so two mercs changed hands
mid-battle. Seeded fixture; replicated state, not physical OS input or paint.
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

ROOT = Path(__file__).resolve().parents[1]
KNOCKED_MARKER = "[co-op entry test] knocked out:"


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


def ownership(states):
    """{actor: client index} from each client's own assigned list; None if
    any actor is claimed by both clients."""
    owners = {}
    for index, state in enumerate(states):
        for actor in state.get("assigned", []):
            actor = tuple(actor)
            if actor in owners:
                return None
            owners[actor] = index
    return owners


def changed_owners(before, after, removed):
    """Actors (other than the removed one) whose owner differs or vanished."""
    return sorted(actor for actor, owner in before.items() if actor != removed and after.get(actor) != owner)


def wait_ownership(probes, guard, deadline, predicate):
    last = None
    while time.monotonic() < deadline:
        last = ownership(_states(probes, deadline, guard))
        if last is not None and predicate(last):
            return last
        time.sleep(0.2)
    raise SmokeFailure(f"ownership never reached the expected shape: {last}")


def main(argv=None):
    args = parse_arguments(argv)
    fixture, client_exe = (path.resolve(strict=True) for path in (args.fixture_executable, args.client_executable))
    if fixture.name != "full_engine_coop_installed_entry":
        raise SmokeFailure("stable-ownership probe requires the test-only host")
    installed = args.installed_data_root.resolve(strict=True)
    ja2_ini, configuration = selected_configuration(installed)
    roots = installed_content_roots(installed)
    run = Path(tempfile.mkdtemp(prefix="ja2-coop-ownership-"))
    print(f"Installed stable-ownership logs: {run}", flush=True)
    for name in ("server", "client-a", "client-b"):
        make_run_root(run / name, ja2_ini, configuration, roots)
        make_private_directory(run / f"{name}-state")
    evidence = {"qualification_scope": "seeded_knockout_stable_ownership", "seeded_native_fixture": True,
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
        deadline = time.monotonic() + timeout
        before = wait_ownership(probes, guard, deadline,
                                lambda owners: len(owners) == 4 and sorted(owners.values()) == [0, 0, 1, 1])
        evidence["before"] = {f"{a[0]}:{a[1]}": owner for a, owner in before.items()}
        victim = sorted(before)[1]
        evidence["victim"] = list(victim)
        trigger = run / "server" / "knockout-trigger.request"
        trigger.write_text(f"{victim[0]} {victim[1]}\n", encoding="ascii")
        trigger.rename(run / "server" / "knockout-trigger.txt")
        wait_for_marker(server, KNOCKED_MARKER, timeout)
        after = wait_ownership(probes, guard, time.monotonic() + timeout, lambda owners: victim not in owners)
        time.sleep(5)
        settled = ownership(_states(probes, time.monotonic() + 10, guard))
        evidence["after"] = {f"{a[0]}:{a[1]}": owner for a, owner in (settled or {}).items()}
        flipped = changed_owners(before, settled or {}, victim) if settled is not None else ["double-claimed"]
        evidence["changed"] = [list(actor) if isinstance(actor, tuple) else actor for actor in flipped]
        if flipped or after is None:
            raise SmokeFailure(f"mercs changed hands when one went down: {evidence['changed']}")
        for client in clients:
            supervisor.stop(client, expected_exit=0, timeout_seconds=30)
        evidence["passed"] = True
        print(f"PASS: merc {victim[0]}:{victim[1]} went down; the other three kept their owners on both clients.",
              flush=True)
        return 0
    except Exception as error:
        evidence["failure"] = str(error)
        print(f"Installed stable ownership failed: {error}", flush=True)
        return 1
    finally:
        supervisor.cleanup()
        with (run / "ownership-evidence.json").open("w", encoding="utf-8") as output:
            json.dump(evidence, output, indent=2, default=str)
            output.write("\n")


if __name__ == "__main__":
    raise SystemExit(main())
