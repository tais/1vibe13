#!/usr/bin/env python3
"""Installed NPC conversation: talk to Fatima in Omerta A9 through Screen input.

Two clients win the starter battle (seeded) and stay in the secured A9 sector.
Client A walks its selected merc next to Fatima (profile 101) and presses K. The
authority opens the single native talk panel; both clients must receive the
same replicated conversation, only client A may answer, and client A's queued
1 (friendly) must advance Fatima's native line on both replicas before Esc
closes the panel on both. Seeded victory only; queued Screen input, not
physical OS input or painted pixels.
"""
import argparse
import json
from pathlib import Path
import tempfile
import time

from full_engine_coop_installed_campaign import qualification_guard
from full_engine_coop_installed_exit_grid import seed_victory
from full_engine_coop_installed_movement import _Probe, _states
from full_engine_coop_installed_peaceful_sector import _secured_marker_sectors
from full_engine_coop_installed_process_smoke import (
    READY_MARKER, TACTICAL_PRESENTATION_READY_MARKER, TACTICAL_SERVER_MARKER, ProcessSupervisor, SmokeFailure,
    client_command, free_loopback_port, installed_content_roots, log_text, make_private_directory, make_run_root,
    process_environment, selected_configuration, server_command, wait_for_marker, wait_for_markers,
)

ROOT = Path(__file__).resolve().parents[1]
FATIMA = 101
LETTER = 227
TALK_RADIUS = 4


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


def pyth_spaces(a, b, columns):
    """The stock PythSpacesAway: integer root of the squared tile distance."""
    dy, dx = a // columns - b // columns, a % columns - b % columns
    return int((dx * dx + dy * dy) ** 0.5)


def talk_tiles(state, merc, npc):
    """Free same-level tiles within the talk radius of `npc`, nearest to `merc` first."""
    columns, rows = state["columns"], state["rows"]
    occupied = {actor["grid"] for actor in state["actors"] if actor["level"] == npc["level"] and actor["life"] > 0}
    row, column = divmod(npc["grid"], columns)
    tiles = []
    for dr in range(-2, 3):
        for dc in range(-2, 3):
            r, c = row + dr, column + dc
            grid = r * columns + c
            if (dr or dc) and 0 <= r < rows and 0 <= c < columns and grid not in occupied and \
                    pyth_spaces(grid, npc["grid"], columns) < TALK_RADIUS:
                tiles.append(grid)
    # Nearest to the NPC first: K addresses the nearest visible NPC, so an
    # adjacent tile keeps a bystander from tying with the intended one.
    def squared(a, b):
        return (a // columns - b // columns) ** 2 + (a % columns - b % columns) ** 2
    return sorted(tiles, key=lambda grid: (squared(grid, npc["grid"]), squared(grid, merc["grid"]), grid))


def find_actor(state, predicate):
    matches = [actor for actor in state["actors"] if predicate(actor)]
    return matches[0] if len(matches) == 1 else None


def conversations(probes, guard, deadline):
    requests = [probe.send("CONVERSATION") for probe in probes]
    return [probe.receive(request, "conversation", deadline, guard) for probe, request in zip(probes, requests)]


def quote_text(record):
    return "".join(chr(unit) for unit in record.get("quote", []))


def wait_conversations(probes, guard, predicate, timeout, what):
    """Poll both replicas until `predicate(records)` holds on identical content."""
    deadline = time.monotonic() + timeout
    last = None
    while time.monotonic() < deadline:
        records = conversations(probes, guard, deadline)
        last = records
        same = all(r["ready"] for r in records) and len({
            (r["revision"], r["conversation"], r["npc_profile"], r["merc_profile"], tuple(r["npc"]), tuple(r["merc"]),
             r["speaking"], tuple(r["quote"])) for r in records}) == 1
        if same and predicate(records):
            return records
        time.sleep(0.1)
    raise SmokeFailure(f"replicas never agreed on {what}; last={json.dumps(last)[:600]}")


def walk_into_talk_range(probes, guard, sender, merc, npc, timeout):
    """MOVE the merc onto a free tile within the talk radius; returns the tile."""
    deadline = time.monotonic() + timeout
    identity = (merc["slot"], merc["inc"])
    state = _states(probes, deadline, guard)[sender]
    for target in talk_tiles(state, merc, npc)[:8]:
        request = probes[sender].send("MOVE", *identity, target)
        reply = probes[sender].receive(request, "move", deadline, guard)
        if not reply.get("submitted") or not reply.get("command"):
            continue
        attempt = min(deadline, time.monotonic() + 45)
        while time.monotonic() < attempt:
            states = _states(probes, deadline, guard)
            receipt = states[sender].get("receipt")
            if receipt and receipt["command"] == reply["command"] and receipt["status"] == 2:
                break
            actors = [find_actor(s, lambda a: (a["slot"], a["inc"]) == identity) for s in states]
            if all(a and a["grid"] == target for a in actors) and states[sender]["outstanding"] == 0:
                return target
            time.sleep(0.1)
    raise SmokeFailure("no talk-range tile next to the NPC accepted the merc")


# RevisionMismatch / NotBaselineReady: explicit refusals before execution
# (the view moved on under the key). A player simply presses again.
STALE_REASONS = (5, 9)
MAXIMUM_STALE_RETRIES = 6


def last_command(record):
    return (record.get("receipt") or {}).get("command", 0)


def command_outcome(probe, guard, previous, done, timeout):
    """After one submitting key: 'done' once done() holds, 'stale' when the
    authority refused the newer command before executing it; raise otherwise."""
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        record = conversations([probe], guard, deadline)[0]
        if done(record):
            return "done"
        receipt = record.get("receipt") or {}
        # Status 2 is a refusal before execution, 4 a native discard, 5 a cancel.
        if receipt.get("command", 0) > previous and receipt.get("status") in (2, 4, 5) and record.get("outstanding") == 0:
            if receipt.get("status") == 2 and receipt.get("reason") in STALE_REASONS:
                return "stale"
            raise SmokeFailure(f"the authority rejected the command: {receipt}")
        time.sleep(0.1)
    raise SmokeFailure(f"{probe.managed.name}: the command neither completed nor was refused")


def converse(probe, guard, conversation, approach, timeout):
    request = probe.send("CONVERSATION_INPUT", conversation, approach)
    reply = probe.receive(request, "conversation_input", time.monotonic() + timeout, guard)
    if not reply.get("queued"):
        raise SmokeFailure(f"conversation key was not queued: {reply}")
    return reply


def fixture_npc_position(server, npc_profile):
    """The test host's latest authority-side position for an unseen NPC."""
    prefix = f"[co-op entry test] npc profile={npc_profile} "
    lines = [line for line in log_text(server).splitlines() if line.startswith(prefix)]
    if not lines:
        return None
    fields = dict(part.split("=", 1) for part in lines[-1][len("[co-op entry test] npc "):].split())
    return {"grid": int(fields["grid"]), "level": int(fields["level"])}


def talk_to_npc(probes, guard, server, npc_profile, timeout, evidence=None):
    """Walk A's selected merc to the NPC, open, answer friendly and leave.

    Records progress into `evidence` as it goes, so a failure keeps its stage."""
    evidence = {} if evidence is None else evidence
    evidence["walks"] = []
    idle = wait_conversations(probes, guard, lambda r: r[0]["conversation"] == 0, timeout, "no open conversation")
    selected = tuple(idle[0]["selected"])
    npc = None
    for _ in range(4):
        states = _states(probes, time.monotonic() + timeout, guard)
        if list(selected) not in states[0]["assigned"]:
            raise SmokeFailure("client A's selected merc is not one of its assigned actors")
        merc = find_actor(states[0], lambda a: (a["slot"], a["inc"]) == selected)
        npc = find_actor(states[0], lambda a: a["profile"] == npc_profile and a["life"] > 0)
        talk = conversations(probes, guard, time.monotonic() + timeout)[0]
        if npc and tuple(talk["talk_target"]) == (npc["slot"], npc["inc"]):
            break
        # The team has not seen the NPC (stock fog), or it stands too far:
        # walk toward where it is, replicated if seen, else per the fixture log.
        where = npc or fixture_npc_position(server, npc_profile)
        if not merc or not where:
            raise SmokeFailure(f"profile {npc_profile} is not in the loaded sector")
        evidence["walks"].append(walk_into_talk_range(probes, guard, 0, merc, where, timeout))
        # Line of sight (and so the public projection) updates after arrival.
        settle = time.monotonic() + 10
        while time.monotonic() < settle:
            talk = conversations(probes, guard, time.monotonic() + timeout)[0]
            if tuple(talk["talk_target"]) != (65535, 0) and talk["talk_target"][1]:
                break
            time.sleep(0.2)
    else:
        raise SmokeFailure(f"profile {npc_profile} never became client A's talk target")
    evidence["npc"] = [npc["slot"], npc["inc"]]
    ready = wait_conversations(probes, guard, lambda r: tuple(r[0]["talk_target"]) == (npc["slot"], npc["inc"]),
                               timeout, "the NPC as client A's talk target")
    # Client B cannot answer a conversation that does not exist; nor press K
    # on A's behalf: K only ever addresses B's own selected merc.
    for attempt in range(MAXIMUM_STALE_RETRIES):
        previous = last_command(conversations([probes[0]], guard, time.monotonic() + timeout)[0])
        converse(probes[0], guard, 0, 0, timeout)
        if command_outcome(probes[0], guard, previous, lambda r: r["conversation"] != 0, timeout) == "done":
            break
        evidence["stale_talk_retries"] = attempt + 1
        time.sleep(0.3)
    else:
        raise SmokeFailure("talk stayed stale through every retry")
    opened = wait_conversations(
        probes, guard, lambda r: r[0]["conversation"] != 0 and not r[0]["speaking"], timeout, "the opened panel")
    evidence["greeting"] = quote_text(opened[0])
    if (opened[0]["npc_profile"], tuple(opened[0]["merc"]), tuple(opened[0]["npc"])) != \
            (npc_profile, selected, (npc["slot"], npc["inc"])):
        raise SmokeFailure(f"the panel opened for the wrong pair: {opened[0]}")
    if not opened[0]["answerable"] or opened[1]["answerable"]:
        raise SmokeFailure("only the initiating player may answer the conversation")
    request = probes[1].send("CONVERSATION_INPUT", opened[1]["conversation"], 1)
    refused = probes[1].receive(request, "error", time.monotonic() + timeout, guard)
    evidence["observer_answer_refused"] = refused.get("error", "")
    converse(probes[0], guard, opened[0]["conversation"], 1, timeout)
    answered = wait_conversations(
        probes, guard, lambda r: r[0]["revision"] > opened[0]["revision"] and r[0]["conversation"] ==
        opened[0]["conversation"] and not r[0]["speaking"] and quote_text(r[0]) != evidence["greeting"],
        timeout, "Fatima's friendly answer")
    evidence["friendly_answer"] = quote_text(answered[0])
    converse(probes[0], guard, answered[0]["conversation"], 0, timeout)
    wait_conversations(probes, guard, lambda r: r[0]["conversation"] == 0 and r[0]["revision"] > answered[0]["revision"],
                       timeout, "the closed panel")
    evidence["closed"] = True
    return evidence


def letter_holder(state):
    """(actor identity, slot) of the one owned object of `item`, or None."""
    found = [(tuple(inventory["actor"]), slot["slot"]) for inventory in state.get("inventories", [])
             for slot in inventory["slots"] if slot.get("item") == LETTER]
    return found[0] if len(found) == 1 else None


def inventory_ui(probe, guard, timeout):
    request = probe.send("INVENTORY_UI")
    return probe.receive(request, "inventory_ui", time.monotonic() + timeout, guard)


def wait_inventory_ui(probe, guard, predicate, timeout, what):
    deadline = time.monotonic() + timeout
    last = None
    while time.monotonic() < deadline:
        last = inventory_ui(probe, guard, timeout)
        if last["ready"] and predicate(last):
            return last
        time.sleep(0.1)
    raise SmokeFailure(f"inventory never showed {what}; last={last}")


def give_letter(probes, guard, timeout, evidence):
    """Client A opens its inventory, selects the letter and presses K at Fatima."""
    holder = letter_holder(_states(probes, time.monotonic() + timeout, guard)[0])
    if not holder:
        raise SmokeFailure("client A does not hold exactly one letter")
    (slot_id, incarnation), source = holder
    evidence["letter_slot"] = source
    def letter_gone(_record):
        return letter_holder(_states(probes[:1], time.monotonic() + timeout, guard)[0]) is None
    for attempt in range(MAXIMUM_STALE_RETRIES):
        closed = wait_inventory_ui(probes[0], guard, lambda ui: not ui["open"] and ui["outstanding"] == 0 and
                                   tuple(ui["actor"]) == (slot_id, incarnation), timeout,
                                   "the closed inventory of the holder")
        for action, slot, predicate, what in (
                (1, 0xFFFF, lambda ui: ui["open"], "the open inventory"),
                (2, source, lambda ui: ui["open"] and ui["source"] == source, "the letter as the selected source")):
            request = probes[0].send("INVENTORY_INPUT", action, slot_id, incarnation, slot, closed["inventory_revision"])
            reply = probes[0].receive(request, "inventory_input", time.monotonic() + timeout, guard)
            if not reply.get("queued"):
                raise SmokeFailure(f"inventory input {action} was not queued: {reply}")
            wait_inventory_ui(probes[0], guard, predicate, timeout, what)
        previous = last_command(conversations([probes[0]], guard, time.monotonic() + timeout)[0])
        request = probes[0].send("GIVE_INPUT", slot_id, incarnation, source, closed["inventory_revision"])
        reply = probes[0].receive(request, "give_input", time.monotonic() + timeout, guard)
        if not reply.get("queued"):
            raise SmokeFailure(f"the give key was not queued: {reply}")
        evidence["give_target"] = reply["target"]
        if command_outcome(probes[0], guard, previous, letter_gone, timeout) == "done":
            break
        evidence["stale_give_retries"] = attempt + 1
        time.sleep(0.3)
    else:
        raise SmokeFailure("the give stayed stale through every retry")
    evidence["letter_given"] = True
    reaction = wait_conversations(probes, guard, lambda r: r[0]["conversation"] != 0 and r[0]["npc_profile"] == FATIMA,
                                  timeout, "Fatima's reaction to the letter")
    evidence["letter_reaction"] = quote_text(reaction[0])
    return evidence


def wait_npc_settled(server, npc_profile, timeout, quiet=4.0):
    """Fixture position of the NPC once it has not changed for `quiet` seconds."""
    deadline = time.monotonic() + timeout
    last, since = fixture_npc_position(server, npc_profile), time.monotonic()
    while time.monotonic() < deadline:
        now = fixture_npc_position(server, npc_profile)
        if now != last:
            last, since = now, time.monotonic()
        elif time.monotonic() - since >= quiet:
            return last
        time.sleep(0.2)
    raise SmokeFailure(f"profile {npc_profile} never stood still")


def follow_npc_once(probes, guard, server, npc_profile, timeout, evidence):
    """After "Follow me": the giver (no longer locked by its spent give) walks
    up to the NPC, and the stock PC-near trigger moves the NPC on."""
    wait_conversations(probes, guard, lambda r: r[0]["conversation"] == 0, timeout, "the letter conversation ending")
    waiting = wait_npc_settled(server, npc_profile, timeout)
    evidence["lead_waypoint"] = waiting["grid"]
    state = _states(probes, time.monotonic() + timeout, guard)[0]
    selected = tuple(conversations(probes[:1], guard, time.monotonic() + timeout)[0]["selected"])
    merc = find_actor(state, lambda a: (a["slot"], a["inc"]) == selected)
    if not merc:
        raise SmokeFailure("the giver is not in the replicated sector")
    evidence["followed_to"] = walk_into_talk_range(probes, guard, 0, merc, waiting, timeout)
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        now = fixture_npc_position(server, npc_profile)
        if now and now["grid"] != waiting["grid"]:
            evidence["led_on"] = True
            return evidence
        time.sleep(0.2)
    raise SmokeFailure(f"profile {npc_profile} did not move on when the giver came near")


def main(argv=None):
    args = parse_arguments(argv)
    fixture, client_exe = (path.resolve(strict=True) for path in (args.fixture_executable, args.client_executable))
    if fixture.name != "full_engine_coop_installed_entry":
        raise SmokeFailure("NPC dialogue probe requires the test-only host")
    installed = args.installed_data_root.resolve(strict=True)
    ja2_ini, configuration = selected_configuration(installed)
    roots = installed_content_roots(installed)
    run = Path(tempfile.mkdtemp(prefix="ja2-coop-npc-dialogue-"))
    print(f"Installed NPC dialogue logs: {run}", flush=True)
    for name in ("server", "client-a", "client-b"):
        make_run_root(run / name, ja2_ini, configuration, roots)
        make_private_directory(run / f"{name}-state")
    evidence = {"qualification_scope": "seeded_native_victory_peaceful_npc_conversation", "seeded_native_fixture": True,
                "physical_input_verified": False, "paint_verified": False, "audio_verified": False, "passed": False}
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
        seed_victory(run / "server", _states(probes, time.monotonic() + timeout, guard), 1)
        deadline = time.monotonic() + timeout
        while not _secured_marker_sectors(server):
            if time.monotonic() > deadline:
                raise SmokeFailure("the seeded A9 victory never secured the sector")
            time.sleep(0.2)
        # The client owning Enrico's letter acts; the other only observes.
        states = _states(probes, time.monotonic() + timeout, guard)
        if letter_holder(states[1]) and not letter_holder(states[0]):
            probes.reverse()
            evidence["acting_client"] = "client-b"
        else:
            evidence["acting_client"] = "client-a"
        talk_to_npc(probes, guard, server, FATIMA, timeout, evidence)
        give_letter(probes, guard, timeout, evidence)
        follow_npc_once(probes, guard, server, FATIMA, timeout, evidence)
        for client in clients:
            supervisor.stop(client, expected_exit=0, timeout_seconds=30)
        evidence["passed"] = True
        print("PASS: K opened Fatima's conversation on both replicas (only the initiator answers), 1 advanced and "
              "Esc closed it; the letter holder gave her Enrico's letter, she read it and led on when followed.",
              flush=True)
        return 0
    except Exception as error:
        evidence["failure"] = str(error)
        print(f"Installed NPC dialogue failed: {error}", flush=True)
        return 1
    finally:
        supervisor.cleanup()
        with (run / "npc-dialogue-evidence.json").open("w", encoding="utf-8") as output:
            json.dump(evidence, output, indent=2, default=str)
            output.write("\n")
        if "server" in locals():
            lines = [line for line in log_text(server).splitlines() if "runtime failed" in line or "talk" in line]
            print("\n".join(lines[-20:]), flush=True)


if __name__ == "__main__":
    raise SystemExit(main())
