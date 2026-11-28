#!/usr/bin/env python3
"""Installed grenade thrown at a real enemy.

Two clients join the untouched starter battle. The test host puts two
fragmentation grenades from the game data in every player merc's hand. A
merc's owner throws one at a native enemy's tile through the shipping
controller (the stock toss); the server runs the stock flight and explosion.
Required: one grenade gone from that hand on both clients and the explosion of
that grenade in the world. The enemy's health is recorded, not required: it
may have moved, and a throw may scatter. Seeded weapons; controller input,
not physical OS input or painted pixels.

With --launcher the test host instead loads a grenade launcher from the game
data with a launchable fragmentation grenade (stock AttachObject), and the
owner fires it at the enemy's tile the same way (H: the stock trajectory
launch at a tile). Required: the Applied receipt and the explosion of that
grenade; the launcher stays in the hand on both clients.
"""
import argparse
import json
from pathlib import Path
import re
import tempfile
import time

from full_engine_coop_installed_campaign import qualification_guard
from full_engine_coop_installed_movement import _Probe, _states, _actor
from full_engine_coop_installed_process_smoke import (
    READY_MARKER, TACTICAL_PRESENTATION_READY_MARKER, TACTICAL_SERVER_MARKER, ProcessSupervisor, SmokeFailure,
    client_command, free_loopback_port, installed_content_roots, log_text, make_private_directory, make_run_root,
    process_environment, selected_configuration, server_command, wait_for_marker, wait_for_markers,
)

ROOT = Path(__file__).resolve().parents[1]
SEEDED = re.compile(r"\[co-op entry test\] combat grenade seeded: mercs=(\d+); item=(\d+); count=(\d+)")
LAUNCHER_SEEDED = re.compile(r"\[co-op entry test\] combat launcher seeded: mercs=(\d+); launcher=(\d+); item=(\d+)")
EXPLODED = re.compile(r"\[co-op entry test\] grenade exploded: item=(\d+); grid=(\d+)")
MAXIMUM_ATTEMPTS = 24


def parse_arguments(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--fixture-executable", type=Path, default=ROOT / "build/full_engine_coop_installed_entry")
    parser.add_argument("--client-executable", type=Path, default=ROOT / "build/full_engine_coop_installed_client")
    parser.add_argument("--installed-data-root", type=Path, default=ROOT.parent / "JA2")
    parser.add_argument("--timeout-seconds", type=float, default=300)
    parser.add_argument("--launcher", action="store_true", help="fire a loaded grenade launcher instead")
    args = parser.parse_args(argv)
    if not 1 <= args.timeout_seconds <= 900:
        parser.error("timeout must be 1..900 seconds")
    return args


def parse_seed(text, launcher=False):
    """The seeded hand: the grenade stack, or (launcher) the loaded launcher
    as the hand item with count 1 and the launched grenade as the item."""
    if launcher:
        match = LAUNCHER_SEEDED.search(text)
        if not match:
            return None
        mercs, gun, item = (int(value) for value in match.groups())
        return {"mercs": mercs, "hand": gun, "item": item, "count": 1}
    match = SEEDED.search(text)
    if not match:
        return None
    mercs, item, count = (int(value) for value in match.groups())
    return {"mercs": mercs, "hand": item, "item": item, "count": count}


def parse_explosion(text):
    match = EXPLODED.search(text)
    return {"item": int(match.group(1)), "grid": int(match.group(2))} if match else None


def hand(state, identity):
    primary = _actor(state, identity)["primary"]
    return primary["item"], primary["quantity"]


def candidates(states, item, count):
    """(distance, owner, thrower, target identity, target grid), nearest first:
    a merc a client controls holding the seeded stack, and a living enemy."""
    found = []
    for owner, state in enumerate(states):
        assigned = {tuple(actor) for actor in state.get("assigned", [])}
        enemies = [actor for actor in state["actors"]
                   if actor["active"] and actor["in_sector"] and actor["hostile"] and actor["life"] > 0]
        for merc in state["actors"]:
            identity = (merc["slot"], merc["inc"])
            if (identity not in assigned or merc["life"] <= 0 or merc["hostile"] or
                    (merc["primary"]["item"], merc["primary"]["quantity"]) != (item, count)):
                continue
            for enemy in enemies:
                distance = (merc["world_x"] - enemy["world_x"]) ** 2 + (merc["world_y"] - enemy["world_y"]) ** 2
                found.append((distance, owner, identity, (enemy["slot"], enemy["inc"]), enemy["grid"]))
    return sorted(found)


def wait_receipt(probes, guard, owner, command, deadline):
    while time.monotonic() < deadline:
        states = _states(probes, deadline, guard)
        receipt = states[owner].get("receipt") or {}
        if receipt.get("command") == command and receipt.get("status") in (2, 3, 4, 5):
            return receipt, states
        time.sleep(0.1)
    raise SmokeFailure(f"the throw {command} never got a final receipt")


def main(argv=None):
    args = parse_arguments(argv)
    fixture, client_exe = (path.resolve(strict=True) for path in (args.fixture_executable, args.client_executable))
    if fixture.name != "full_engine_coop_installed_entry":
        raise SmokeFailure("grenade probe requires the test-only host")
    installed = args.installed_data_root.resolve(strict=True)
    ja2_ini, configuration = selected_configuration(installed)
    roots = installed_content_roots(installed)
    run = Path(tempfile.mkdtemp(prefix="ja2-coop-launcher-" if args.launcher else "ja2-coop-grenade-"))
    print(f"Installed grenade logs: {run}", flush=True)
    for name in ("server", "client-a", "client-b"):
        make_run_root(run / name, ja2_ini, configuration, roots)
        make_private_directory(run / f"{name}-state")
    evidence = {"qualification_scope": "seeded_launcher_at_native_enemy" if args.launcher
                else "seeded_grenade_at_native_enemy", "seeded_native_fixture": True,
                "physical_input_verified": False, "paint_verified": False, "passed": False, "attempts": []}
    supervisor = ProcessSupervisor()
    environment = process_environment()
    environment.pop("JA2_COOP_ENTRY_TEST_MODE", None)
    timeout = args.timeout_seconds
    try:
        port = free_loopback_port()
        server = supervisor.start("server", server_command(fixture, run / "server-state", port, "new"), run / "server",
                                  dict(environment, JA2_COOP_ENTRY_TEST_MODE="starter-victory",
                                       JA2_COOP_ENTRY_COMBAT_GRENADE="launcher" if args.launcher else "1"))
        wait_for_marker(server, f"[dedicated] co-op campaign created; admission listening on 127.0.0.1:{port}", timeout)
        clients = [supervisor.start(name, client_command(client_exe, run / f"{name}-state", port), run / name,
                                    environment, driven=True) for name in ("client-a", "client-b")]
        wait_for_markers([(clients[0], READY_MARKER), (clients[1], READY_MARKER), (server, TACTICAL_SERVER_MARKER),
                          (clients[0], TACTICAL_PRESENTATION_READY_MARKER),
                          (clients[1], TACTICAL_PRESENTATION_READY_MARKER)], timeout)
        guard = qualification_guard(server, clients)
        wait_for_marker(server, "[co-op entry test] combat launcher seeded" if args.launcher
                        else "[co-op entry test] combat grenade seeded", timeout)
        seed = parse_seed(log_text(server), args.launcher)
        if not seed or seed["count"] < (1 if args.launcher else 2):
            raise SmokeFailure(f"the grenade seed is unusable: {seed}")
        evidence["seed"] = seed
        probes = [_Probe(client) for client in clients]
        deadline = time.monotonic() + timeout
        thrown = None
        while time.monotonic() < deadline and len(evidence["attempts"]) < MAXIMUM_ATTEMPTS:
            states = _states(probes, deadline, guard)
            if any(state["outstanding"] for state in states):
                time.sleep(0.2)
                continue
            options = candidates(states, seed["hand"], seed["count"])
            if not options:
                time.sleep(0.5)
                continue
            _, owner, thrower, target, grid = options[0]
            life_before = _actor(states[owner], target)["life"]
            reply = probes[owner].receive(probes[owner].send("THROW", *thrower, grid), ("throw", "error"), deadline, guard)
            attempt = {"owner": owner, "thrower": list(thrower), "target": list(target), "grid": grid, "reply": reply}
            evidence["attempts"].append(attempt)
            if reply.get("kind") != "throw" or not reply.get("submitted"):
                time.sleep(1.0)
                continue
            receipt, _ = wait_receipt(probes, guard, owner, reply["command"], min(deadline, time.monotonic() + 30))
            attempt["receipt"] = receipt
            if receipt["status"] == 3:
                thrown = {"owner": owner, "thrower": thrower, "target": target, "grid": grid, "life_before": life_before}
                break
            time.sleep(1.5)
        if not thrown:
            raise SmokeFailure(f"no throw at an enemy was applied in {len(evidence['attempts'])} attempts")
        wait_for_marker(server, "[co-op entry test] grenade exploded", timeout)
        evidence["explosion"] = parse_explosion(log_text(server))
        deadline = time.monotonic() + timeout
        while time.monotonic() < deadline:
            states = _states(probes, deadline, guard)
            hands = [hand(state, thrown["thrower"]) for state in states]
            # A launcher stays in the hand; a thrown grenade leaves the stack.
            if hands == [(seed["hand"], 1 if args.launcher else seed["count"] - 1)] * 2:
                break
            time.sleep(0.2)
        else:
            raise SmokeFailure(f"both clients never showed one grenade gone: {hands}")
        try:
            thrown["life_after"] = _actor(states[thrown["owner"]], thrown["target"])["life"]
        except Exception:
            thrown["life_after"] = None
        evidence["thrown"] = {key: list(value) if isinstance(value, tuple) else value for key, value in thrown.items()}
        if evidence["explosion"]["item"] != seed["item"]:
            raise SmokeFailure(f"the explosion was not the thrown grenade: {evidence['explosion']}")
        for client in clients:
            supervisor.stop(client, expected_exit=0, timeout_seconds=30)
        evidence["passed"] = True
        print(f"PASS: a grenade {'launched' if args.launcher else 'thrown'} at enemy {thrown['target'][0]}:{thrown['target'][1]} exploded at grid "
              f"{evidence['explosion']['grid']} (aimed {thrown['grid']}); both clients show "
              f"{'the launcher still in hand' if args.launcher else 'one gone'}; enemy life "
              f"{thrown['life_before']} -> {thrown['life_after']}.", flush=True)
        return 0
    except Exception as error:
        evidence["failure"] = str(error)
        print(f"Installed grenade failed: {error}", flush=True)
        return 1
    finally:
        supervisor.cleanup()
        with (run / "grenade-evidence.json").open("w", encoding="utf-8") as output:
            json.dump(evidence, output, indent=2, default=str)
            output.write("\n")


if __name__ == "__main__":
    raise SystemExit(main())
