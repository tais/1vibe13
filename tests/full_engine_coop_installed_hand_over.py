#!/usr/bin/env python3
"""Installed merc hand-over through Screen input.

Two clients join the starter battle; the server deals the four mercs two
each. One player selects a merc of theirs, presses Alt+J and then Enter; the
server gives that merc to the teammate. Both clients then show it under the
teammate's control and every other merc where it was. Queued Screen input
and replicated state, not physical OS input or painted pixels.
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
from full_engine_coop_installed_stable_ownership import changed_owners, ownership, wait_ownership

ROOT = Path(__file__).resolve().parents[1]
KEY_ARM, KEY_CONFIRM = 1, 2
UI_KEYS = {"request", "kind", "ready", "session", "selected", "armed", "enabled", "pending"}


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


def hand_over_ui(probe, deadline, guard):
    answer = probe.receive(probe.send("HAND_OVER_UI"), "hand_over_ui", deadline, guard)
    if answer.get("ready") is True and set(answer) != UI_KEYS:
        raise SmokeFailure(f"hand-over Screen observation has an unexpected shape: {sorted(answer)}")
    return answer


def wait_ui(probe, deadline, guard, predicate):
    last = None
    while time.monotonic() < deadline:
        last = hand_over_ui(probe, deadline, guard)
        if last.get("ready") and predicate(last):
            return last
        time.sleep(0.2)
    raise SmokeFailure(f"the hand-over Screen never reached the expected state: {last}")


def press(probe, ui, key, deadline, guard):
    armed = ui["armed"][0] | (ui["armed"][1] << 16)
    answer = probe.receive(probe.send("HAND_OVER_INPUT", ui["session"], *ui["selected"], armed, 0, key),
                           ("hand_over_input", "error"), deadline, guard)
    if answer.get("kind") != "hand_over_input" or answer.get("queued") is not True:
        raise SmokeFailure(f"the Screen driver did not queue the hand-over key: {answer}")
    return answer


def main(argv=None):
    args = parse_arguments(argv)
    fixture, client_exe = (path.resolve(strict=True) for path in (args.fixture_executable, args.client_executable))
    if fixture.name != "full_engine_coop_installed_entry":
        raise SmokeFailure("hand-over probe requires the test-only host")
    installed = args.installed_data_root.resolve(strict=True)
    ja2_ini, configuration = selected_configuration(installed)
    roots = installed_content_roots(installed)
    run = Path(tempfile.mkdtemp(prefix="ja2-coop-hand-over-"))
    print(f"Installed hand-over logs: {run}", flush=True)
    for name in ("server", "client-a", "client-b"):
        make_run_root(run / name, ja2_ini, configuration, roots)
        make_private_directory(run / f"{name}-state")
    evidence = {"qualification_scope": "screen_hand_over_in_starter_battle", "seeded_native_fixture": False,
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
        giver = 0
        ui = wait_ui(probes[giver], deadline, guard,
                     lambda value: value["enabled"] and before.get(tuple(value["selected"])) == giver)
        merc = tuple(ui["selected"])
        evidence["merc"] = list(merc)
        press(probes[giver], ui, KEY_ARM, deadline, guard)
        armed = wait_ui(probes[giver], deadline, guard, lambda value: tuple(value["armed"]) == merc)
        press(probes[giver], armed, KEY_CONFIRM, deadline, guard)
        wait_for_marker(server, f"[dedicated] merc {merc[0]}:{merc[1]} handed to another player", timeout)
        after = wait_ownership(probes, guard, time.monotonic() + timeout, lambda owners: owners.get(merc) == 1 - giver)
        time.sleep(3)
        settled = ownership(_states(probes, time.monotonic() + 10, guard)) or {}
        evidence["after"] = {f"{a[0]}:{a[1]}": owner for a, owner in settled.items()}
        moved = changed_owners(before, settled, merc)
        if moved or settled.get(merc) != 1 - giver or after is None:
            raise SmokeFailure(f"the hand-over moved the wrong mercs: merc={settled.get(merc)}, others={moved}")
        for client in clients:
            supervisor.stop(client, expected_exit=0, timeout_seconds=30)
        evidence["passed"] = True
        print(f"PASS: Alt+J + Enter gave merc {merc[0]}:{merc[1]} to the teammate on both clients; "
              "the other mercs kept their owners.", flush=True)
        return 0
    except Exception as error:
        evidence["failure"] = str(error)
        print(f"Installed hand-over failed: {error}", flush=True)
        return 1
    finally:
        supervisor.cleanup()
        with (run / "hand-over-evidence.json").open("w", encoding="utf-8") as output:
            json.dump(evidence, output, indent=2, default=str)
            output.write("\n")


if __name__ == "__main__":
    raise SystemExit(main())
