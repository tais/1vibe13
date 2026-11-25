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

from full_engine_coop_installed_campaign import (
    apply_time, group_with_id, paired_query, qualification_guard, require_applied, scoped_input, wait_shared, wait_ui,
)
from full_engine_coop_installed_assignments import ASSIGNMENT_REFUSED, TRAIN_MILITIA, assign, press, shared_balance
from full_engine_coop_installed_exit_grid import (
    exit_grids, seed_victory, tactical_counts, travel_and_enter_a10, wait_count, wait_tactical,
)
from full_engine_coop_installed_movement import _Probe, _states
from full_engine_coop_installed_peaceful_sector import (
    _input, _secured_marker_sectors, _ui, return_from_secured_sector,
)
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


def walk_into_talk_range(probes, guard, sender, merc, npc, timeout, skip=()):
    """MOVE the merc onto a free tile within the talk radius; returns the tile."""
    deadline = time.monotonic() + timeout
    identity = (merc["slot"], merc["inc"])
    state = _states(probes, deadline, guard)[sender]
    for target in [tile for tile in talk_tiles(state, merc, npc) if tile not in skip][:8]:
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


def answer(probe, guard, conversation, approach, timeout):
    """Answer (or leave) the open conversation and wait until the authority has
    applied that command; a stale refusal (the view moved on) is pressed again."""
    for _ in range(MAXIMUM_STALE_RETRIES):
        record = conversations([probe], guard, time.monotonic() + timeout)[0]
        if record.get("conversation") != conversation:
            return  # it closed or moved on by itself; nothing left to answer
        previous = last_command(record)
        converse(probe, guard, conversation, approach, timeout)
        applied = lambda r: (r.get("receipt") or {}).get("command", 0) > previous and \
            (r.get("receipt") or {}).get("status") == 3
        if command_outcome(probe, guard, previous, applied, timeout) == "done":
            return
        time.sleep(0.5)
    raise SmokeFailure("a conversation answer stayed stale through every retry")


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
    answer(probes[0], guard, opened[0]["conversation"], 1, timeout)
    answered = wait_conversations(
        probes, guard, lambda r: r[0]["revision"] > opened[0]["revision"] and r[0]["conversation"] ==
        opened[0]["conversation"] and not r[0]["speaking"] and quote_text(r[0]) != evidence["greeting"],
        timeout, "Fatima's friendly answer")
    evidence["friendly_answer"] = quote_text(answered[0])
    answer(probes[0], guard, answered[0]["conversation"], 0, timeout)
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


TRAVERSE_EAST = 5  # QUOTE_ACTION_ID_TRAVERSE_EAST
CHECKPOINT_MARKER = "[dedicated] post-combat strategic checkpoint committed"


def npc_lead(server, npc_profile):
    """The test host's latest scripted-lead line for the NPC, as a dict."""
    prefix = f"[co-op entry test] npc-lead {npc_profile} "
    lines = [line for line in log_text(server).splitlines() if line.startswith(prefix)]
    if not lines:
        return None
    return {key: int(value) for key, value in (part.split("=", 1) for part in lines[-1][len(prefix):].split())}


def follow_into_a10(probes, guard, server, clients, run, timeout, evidence):
    """Fatima's lead ends at A9's east side tagged to traverse east: the squad
    returns to the map, travels to A10 and enters it, and she must be there."""
    evidence["waypoints"] = []
    for _ in range(6):
        waiting = wait_npc_settled(server, FATIMA, timeout)
        lead = npc_lead(server, FATIMA)
        if lead and lead["action"] == TRAVERSE_EAST:
            break
        # Each waypoint waits for a merc within twice the talk radius.
        state = _states(probes, time.monotonic() + timeout, guard)[0]
        selected = tuple(conversations(probes[:1], guard, time.monotonic() + timeout)[0]["selected"])
        merc = find_actor(state, lambda a: (a["slot"], a["inc"]) == selected)
        evidence["waypoints"].append([waiting["grid"], walk_into_talk_range(probes, guard, 0, merc, waiting, timeout)])
        deadline = time.monotonic() + 30
        while time.monotonic() < deadline:
            lead = npc_lead(server, FATIMA)
            if lead and (lead["action"] == TRAVERSE_EAST or lead["grid"] != waiting["grid"]):
                break
            time.sleep(0.2)
    else:
        raise SmokeFailure("Fatima was never tagged to traverse east")
    evidence["traverse_east_at"] = lead["grid"]
    checkpoints = log_text(server).count(CHECKPOINT_MARKER)
    evidence["a9_return"] = return_from_secured_sector(probes, guard, server, timeout)
    wait_count(server, CHECKPOINT_MARKER, checkpoints + 1, timeout)
    guard = qualification_guard(server, clients, planned_reconnects=1)
    probes = [_Probe(client) for client in clients]
    probes = travel_and_enter_a10(probes, guard, server, clients, run, timeout, evidence)
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        lead = npc_lead(server, FATIMA)
        if lead and lead["grid"] != evidence["traverse_east_at"]:
            break
        time.sleep(0.3)
    else:
        raise SmokeFailure("Fatima did not arrive in A10 with the squad")
    evidence["a10_fatima"] = lead
    return probes, guard


CARLOS = 58
HIDEOUT_ACCEPTED = "[dedicated] hideout traversal accepted to (10,1,1)"


def follow_to_hideout(probes, guard, server, clients, timeout, evidence):
    """In A10: keep up with Fatima's waypoints and close every conversation an
    NPC opens (only its merc's owner may answer) until her hideout action asks
    the runtime to take the squad down; then require the checkpointed
    traversal, the peaceful basement entry and Carlos's welcome on both."""
    evidence["a10_lines"] = []
    deadline = time.monotonic() + 3 * timeout
    last_lead, since = None, time.monotonic()
    while HIDEOUT_ACCEPTED not in log_text(server):
        if time.monotonic() > deadline:
            raise SmokeFailure(f"the hideout traversal never started; Fatima={npc_lead(server, FATIMA)}")
        records = conversations(probes, guard, time.monotonic() + timeout)
        if not all(record["ready"] for record in records):
            # A replica without a ready world (e.g. a native world change that
            # bypassed co-op) cannot follow; the deadline reports it.
            time.sleep(0.3)
            continue
        if records[0]["conversation"]:
            if not records[0]["speaking"]:
                text = quote_text(records[0])
                if not evidence["a10_lines"] or evidence["a10_lines"][-1] != [records[0]["npc_profile"], text]:
                    evidence["a10_lines"].append([records[0]["npc_profile"], text])
                answering = [index for index, record in enumerate(records) if record["answerable"]]
                if answering:
                    time.sleep(1.0)
                    again = conversations(probes, guard, time.monotonic() + timeout)
                    if again[0]["conversation"] == records[0]["conversation"] and not again[0]["speaking"] and \
                            quote_text(again[0]) == text:
                        answer(probes[answering[0]], guard, records[0]["conversation"], 0, timeout)
            time.sleep(0.3)
            continue
        lead = npc_lead(server, FATIMA)
        if lead != last_lead:
            last_lead, since = lead, time.monotonic()
        if lead and time.monotonic() - since > 4 and lead["distance"] > 4:
            state = _states(probes, time.monotonic() + timeout, guard)[0]
            selected = tuple(conversations(probes[:1], guard, time.monotonic() + timeout)[0]["selected"])
            merc = find_actor(state, lambda a: (a["slot"], a["inc"]) == selected)
            where = find_actor(state, lambda a: a["profile"] == FATIMA) or {"grid": lead["grid"], "level": 0}
            try:
                evidence.setdefault("a10_walks", []).append(walk_into_talk_range(probes, guard, 0, merc, where, timeout))
            except SmokeFailure as error:
                evidence.setdefault("a10_walk_failures", []).append(str(error))
            since = time.monotonic()
        time.sleep(0.3)
    evidence["hideout_accepted"] = True
    counts = tactical_counts(server, clients)
    guard = qualification_guard(server, clients, planned_reconnects=2)
    wait_for_marker(server, "[dedicated] exit-grid group relocated to (10,1,1)", timeout)
    wait_for_marker(server, "[dedicated] peaceful sector entered (10,1,1)", timeout)
    wait_tactical(server, clients, counts, timeout)
    wait_for_marker(server, "[dedicated] hideout welcome started", timeout)
    probes = [_Probe(client) for client in clients]
    evidence["basement_entered"] = True
    welcome = wait_conversations(probes, guard, lambda r: r[0]["conversation"] != 0 and r[0]["npc_profile"] == CARLOS,
                                 timeout, "Carlos's welcome in the basement")
    evidence["carlos_welcome"] = quote_text(welcome[0])
    return probes, guard


FACT_MIGUEL_READ_LETTER = 23


def fixture_fact(server, fact):
    """The test host's latest value for one logged Omerta fact, or None."""
    lines = [line for line in log_text(server).splitlines() if line.startswith("[co-op entry test] facts ")]
    if not lines:
        return None
    values = dict(part.split("=", 1) for part in lines[-1][len("[co-op entry test] facts "):].split())
    return values.get(str(fact)) == "1"


def miguel_reads_letter(probes, guard, server, timeout, evidence):
    """The basement conversations play through on their own (each NPC line
    opens, is spoken on both replicas and closes); Fatima hands Miguel the
    letter NPC to NPC and he reads it, setting FACT_MIGUEL_READ_LETTER."""
    evidence["basement_lines"] = []
    deadline = time.monotonic() + 2 * timeout
    while not fixture_fact(server, FACT_MIGUEL_READ_LETTER):
        if time.monotonic() > deadline:
            raise SmokeFailure(f"Miguel never read the letter; lines={evidence['basement_lines']}")
        records = conversations(probes, guard, time.monotonic() + timeout)
        if all(record["ready"] for record in records) and records[0]["conversation"]:
            line = [records[0]["npc_profile"], quote_text(records[0])]
            if line[1] and (not evidence["basement_lines"] or evidence["basement_lines"][-1] != line):
                evidence["basement_lines"].append(line)
        time.sleep(0.3)
    evidence["miguel_read_letter"] = True
    return evidence


def settle_basement(probes, guard, timeout, evidence, quiet=6.0):
    """Close every idle panel left open (a scripted NPC-to-NPC scene may be
    closed by any player) until no conversation has been open for `quiet`."""
    evidence["closed_panels"] = []
    deadline = time.monotonic() + 2 * timeout
    since = time.monotonic()
    idle_key, idle_since = None, None
    while time.monotonic() - since < quiet:
        if time.monotonic() > deadline:
            raise SmokeFailure("the basement conversations never settled")
        records = conversations(probes, guard, time.monotonic() + timeout)
        if not all(record["ready"] for record in records):
            time.sleep(0.3)
            continue
        if not records[0]["conversation"]:
            idle_key = None
            time.sleep(0.3)
            continue
        since = time.monotonic()
        key = (records[0]["revision"], records[0]["speaking"])
        if key != idle_key:
            idle_key, idle_since = key, time.monotonic()
        answering = [index for index, record in enumerate(records) if record["answerable"]]
        if not records[0]["speaking"] and answering and time.monotonic() - idle_since > 2:
            closer = answering[-1]
            evidence["closed_panels"].append([records[0]["npc_profile"], records[0]["merc_profile"],
                                              records[0].get("npc_source"), closer, quote_text(records[0])])
            answer(probes[closer], guard, records[0]["conversation"], 0, timeout)
            idle_key = None
        time.sleep(0.3)
    return evidence


IRA = 59


def attempt_recruit(probes, guard, server, recruiter, npc_profile, timeout, evidence):
    """One stock recruitment attempt by `recruiter`'s selected merc: walk up,
    K, answer 4. True once the NPC is on the player team with one owner;
    False after a stock refusal (the panel is then closed)."""
    probe = probes[recruiter]
    states = _states(probes, time.monotonic() + timeout, guard)
    selected = tuple(conversations([probe], guard, time.monotonic() + timeout)[0]["selected"])
    merc = find_actor(states[recruiter], lambda a: (a["slot"], a["inc"]) == selected)
    npc = find_actor(states[recruiter], lambda a: a["profile"] == npc_profile and a["life"] > 0)
    where = npc or fixture_npc_position(server, npc_profile)
    if not merc or not where:
        raise SmokeFailure(f"profile {npc_profile} or the recruiter is not in the sector")
    attempt = {"client": recruiter, "merc": list(selected), "walks": []}
    evidence.setdefault("recruit_attempts", []).append(attempt)
    # K addresses the nearest NPC: stand only where the recruit is strictly the
    # nearest visible NPC (Miguel and others stand close to Ira).
    for _ in range(6):
        state = _states(probes, time.monotonic() + timeout, guard)[recruiter]
        columns = state["columns"]
        others = [a["grid"] for a in state["actors"] if a["team"] != 0 and a["profile"] and a["profile"] != npc_profile
                  and a["life"] > 0 and a["level"] == where["level"]]
        def squared(a, b):
            return (a // columns - b // columns) ** 2 + (a % columns - b % columns) ** 2
        ambiguous = [tile for tile in talk_tiles(state, merc, where)
                     if any(squared(tile, other) <= squared(tile, where["grid"]) for other in others)]
        try:
            attempt["walks"].append(walk_into_talk_range(probes, guard, recruiter, merc, where, timeout,
                                                         list(attempt["walks"]) + ambiguous))
        except SmokeFailure:
            # No unambiguous tile was reachable: try the rest, checking the
            # talk target after each walk as before.
            attempt["walks"].append(walk_into_talk_range(probes, guard, recruiter, merc, where, timeout,
                                                         list(attempt["walks"])))
        time.sleep(1.0)
        target = conversations([probe], guard, time.monotonic() + timeout)[0]["talk_target"]
        if npc and tuple(target) == (npc["slot"], npc["inc"]):
            break
        states = _states(probes, time.monotonic() + timeout, guard)
        merc = find_actor(states[recruiter], lambda a: (a["slot"], a["inc"]) == selected)
        npc = find_actor(states[recruiter], lambda a: a["profile"] == npc_profile and a["life"] > 0) or npc
    else:
        raise SmokeFailure(f"profile {npc_profile} never became the nearest talk target")
    for _ in range(MAXIMUM_STALE_RETRIES):
        previous = last_command(conversations([probe], guard, time.monotonic() + timeout)[0])
        converse(probe, guard, 0, 0, timeout)
        if command_outcome(probe, guard, previous, lambda r: r["conversation"] != 0, timeout) == "done":
            break
        time.sleep(0.5)
    else:
        raise SmokeFailure("talking to the recruit stayed stale")
    opened = wait_conversations(probes, guard, lambda r: r[0]["conversation"] != 0 and not r[0]["speaking"] and
                                r[0]["npc_profile"] == npc_profile, timeout, "the recruit's panel")
    attempt["greeting"] = quote_text(opened[0])
    answer(probe, guard, opened[0]["conversation"], 4, timeout)
    deadline = time.monotonic() + timeout
    attempt["lines"] = []
    while time.monotonic() < deadline:
        records = conversations(probes, guard, time.monotonic() + timeout)
        states = _states(probes, time.monotonic() + timeout, guard)
        joined = [find_actor(state, lambda a: a["profile"] == npc_profile and a["team"] == 0) for state in states]
        if all(joined):
            identity = [joined[0]["slot"], joined[0]["inc"]]
            owners = [index for index, state in enumerate(states) if identity in state["assigned"]]
            if len(owners) == 1:
                evidence["recruited"] = {"actor": identity, "owner": owners[0], "recruiter": attempt["merc"]}
                return True
        if all(record["ready"] for record in records) and records[0]["conversation"]:
            line = quote_text(records[0])
            if line and attempt["lines"][-1:] != [line]:
                attempt["lines"].append(line)
            answering = [index for index, record in enumerate(records) if record["answerable"]]
            if not records[0]["speaking"] and answering and records[0]["revision"] > opened[0]["revision"]:
                time.sleep(3)
                again = conversations(probes, guard, time.monotonic() + timeout)
                still = [find_actor(state, lambda a: a["profile"] == npc_profile and a["team"] == 0)
                         for state in _states(probes, time.monotonic() + timeout, guard)]
                if not all(still) and again[0]["conversation"] == records[0]["conversation"] and \
                        not again[0]["speaking"] and again[0]["revision"] == records[0]["revision"]:
                    # A stock refusal ("I'm not sure of you"): close and let
                    # another merc try, as a player would.
                    answer(probes[answering[0]], guard, records[0]["conversation"], 0, timeout)
                    wait_conversations(probes, guard, lambda r: r[0]["conversation"] == 0, timeout, "the closed refusal")
                    return False
        time.sleep(0.4)
    raise SmokeFailure(f"the recruit attempt neither succeeded nor ended; lines={attempt['lines']}")


def recruit_npc(probes, guard, server, npc_profile, timeout, evidence):
    """Recruit the NPC with the first merc the stock rules accept: each of the
    acting player's mercs in turn (Tab), then the other player's."""
    for recruiter in (0, 1):
        assigned = _states(probes, time.monotonic() + timeout, guard)[recruiter]["assigned"]
        for _ in range(len(assigned)):
            if attempt_recruit(probes, guard, server, recruiter, npc_profile, timeout, evidence):
                return evidence
            before = tuple(conversations([probes[recruiter]], guard, time.monotonic() + timeout)[0]["selected"])
            request = probes[recruiter].send("SELECT_NEXT_INPUT")
            if not probes[recruiter].receive(request, "select_next_input", time.monotonic() + timeout, guard).get("queued"):
                raise SmokeFailure("Tab was not queued")
            deadline = time.monotonic() + timeout
            while tuple(conversations([probes[recruiter]], guard, deadline)[0]["selected"]) == before:
                if time.monotonic() > deadline:
                    raise SmokeFailure("Tab did not select another merc")
                time.sleep(0.2)
    raise SmokeFailure(f"profile {npc_profile} refused every merc; attempts={evidence.get('recruit_attempts')}")


NATIVE_TRAIN_TOWN = 48  # Strategic/Assignments.h: ON_DUTY is 40, TRAIN_TOWN 48
LEAVE_ACCEPTED = "[dedicated] leave-hideout traversal accepted to (10,1,0)"


def leave_basement(probes, guard, server, clients, timeout, evidence):
    """Walk the acting merc onto the basement's exit grid up to A10 and take it
    with U + Enter: the checkpointed exit-grid traversal back to the surface."""
    exits = exit_grids(server, (10, 1, 0))
    if not exits:
        raise SmokeFailure("the basement lists no exit grid up to A10")
    source, landing = exits[-1]
    evidence["basement_exit"] = {"source": source, "landing": landing}
    selected = conversations(probes[:1], guard, time.monotonic() + timeout)[0]["selected"]
    counts = tactical_counts(server, clients)
    checkpoints = log_text(server).count(CHECKPOINT_MARKER)
    request = probes[0].send("MOVE", selected[0], selected[1], source)
    reply = probes[0].receive(request, "move", time.monotonic() + timeout, guard)
    if not reply.get("submitted"):
        raise SmokeFailure(f"the move onto the basement exit grid was not submitted: {reply}")
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        if LEAVE_ACCEPTED in log_text(server):
            break  # Miguel sent the squad up while it walked to the exit.
        states = _states(probes, deadline, guard)
        actors = [find_actor(state, lambda a: [a["slot"], a["inc"]] == list(selected)) for state in states]
        if all(actor and actor["grid"] == source for actor in actors) and not states[0]["outstanding"]:
            break
        time.sleep(0.2)
    else:
        raise SmokeFailure("the merc did not settle on the basement exit grid")
    # Coming near the exit can make Miguel send the squad up himself (stock
    # NPC_ACTION_LEAVE_HIDEOUT, deferred to the co-op traversal); otherwise
    # the players take the exit grid with U + Enter.
    settle = time.monotonic() + 5
    while time.monotonic() < settle and LEAVE_ACCEPTED not in log_text(server):
        time.sleep(0.2)
    if LEAVE_ACCEPTED in log_text(server):
        evidence["basement_left_by"] = "leave-hideout"
        guard = qualification_guard(server, clients, planned_reconnects=3)
    else:
        evidence["basement_left_by"] = "exit-grid"
        deadline = time.monotonic() + timeout
        ready = _ui(probes[0], deadline, guard)
        _input(probes[0], ready, 4, deadline, guard)
        while time.monotonic() < deadline:
            armed = _ui(probes[0], deadline, guard)
            if armed["ready"] and armed["exit_armed"]:
                break
            time.sleep(0.05)
        else:
            raise SmokeFailure("U did not arm the basement exit grid")
        _input(probes[0], armed, 2, deadline, guard)
        guard = qualification_guard(server, clients, planned_reconnects=3)
        wait_for_marker(server, "[dedicated] exit-grid traversal accepted to (10,1,0)", timeout)
    wait_count(server, CHECKPOINT_MARKER, checkpoints + 1, timeout)
    wait_for_marker(server, "[dedicated] exit-grid group relocated to (10,1,0)", timeout)
    wait_tactical(server, clients, counts, timeout)
    evidence["left_basement"] = True
    return [_Probe(client) for client in clients], guard


def travel_to_a9(probes, guard, timeout, evidence):
    """On the map, the non-leader orders the selected group A10 -> A9 (West +
    Enter); the leader runs time until it arrives, then pauses."""
    initial = wait_shared(probes, guard, timeout, lambda snapshot: snapshot["campaign"][0]["phase"] == 2)
    leader = next(index for index, value in enumerate(initial["campaign"]) if value["local_leader"])
    follower = 1 - leader
    deadline = time.monotonic() + timeout
    selected = press(probes[follower], guard, deadline,
                     lambda value: value["ready"] and value["enabled"] and value["selected"] != [0, 0] and
                     value["destination"] == [0, 0], 5)
    identity = selected["selected"]
    destination = press(probes[follower], guard, deadline, lambda value: value["ready"] and value["enabled"] and
                        value["destination"] == [9, 1] and value["selected"] == identity, 7)
    earlier = (destination["action_result"] or {}).get("request_id", 0)
    travelled = wait_ui(probes[follower], deadline, guard,
                        lambda value: not value["action_pending"] and value["action_result"] is not None and
                        value["action_result"]["request_id"] > earlier)
    require_applied(travelled["action_result"], destination, 1, identity, [9, 1])
    _, running = apply_time(probes[leader], 16, 4, time.monotonic() + timeout, guard)
    arrived = wait_shared(probes, guard, timeout, lambda snapshot: (lambda group: group["x"] == 9 and not group["between"])(
        group_with_id(snapshot, identity)) and not snapshot["campaign"][0]["arrival"]["decision"])
    apply_time(probes[leader], 13, 1, time.monotonic() + timeout, guard, running["time_result"]["request_id"])
    evidence["a9_group"] = identity
    return arrived


def militia_in_omerta(probes, guard, server, clients, timeout, evidence):
    """Back on the map the squad walks to A9. The installed 1.13 data places no
    militia-training facility in Omerta (A9/A10; the first is Drassen), so every
    merc's militia order must be a stock refusal that charges nothing. The paid
    path needs a militia town and is left to the campaign's Drassen leg."""
    latest = max(_secured_marker_sectors(server))
    checkpoints = log_text(server).count(CHECKPOINT_MARKER)
    evidence["surface_return"] = return_from_secured_sector(probes, guard, server, timeout, previous=latest - 1)
    wait_count(server, CHECKPOINT_MARKER, checkpoints + 1, timeout)
    guard = qualification_guard(server, clients, planned_reconnects=4)
    probes = [_Probe(client) for client in clients]
    travel_to_a9(probes, guard, timeout, evidence)
    before = shared_balance(probes, guard, timeout)
    evidence["militia_attempts"] = []
    members = next(group for group in paired_query(probes, "GROUPS", "groups", time.monotonic() + timeout, guard)[0]["groups"]
                   if [group["slot"], group["inc"]] == evidence["a9_group"])["members"]
    for step in range(len(members)):
        actor, receipt = assign(probes[0], guard, timeout, TRAIN_MILITIA, member_steps=step)
        evidence["militia_attempts"].append([actor, receipt["outcome"], receipt["native_detail"]])
        if receipt["outcome"] != 6 or receipt["native_detail"] != ASSIGNMENT_REFUSED:
            raise SmokeFailure(f"Omerta has no militia facility; the order must be a stock refusal: {receipt}")
    after = shared_balance(probes, guard, timeout)
    if after != before:
        raise SmokeFailure(f"refused militia orders changed the balance {before} -> {after}")
    evidence["omerta_militia_refused_without_charge"] = True
    return probes, guard


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
        ordered = [probe.managed for probe in probes]
        probes, guard = follow_into_a10(probes, guard, server, ordered, run / "server", timeout, evidence)
        probes, guard = follow_to_hideout(probes, guard, server, ordered, timeout, evidence)
        miguel_reads_letter(probes, guard, server, timeout, evidence)
        settle_basement(probes, guard, timeout, evidence)
        recruit_npc(probes, guard, server, IRA, timeout, evidence)
        probes, guard = leave_basement(probes, guard, server, ordered, timeout, evidence)
        probes, guard = militia_in_omerta(probes, guard, server, ordered, timeout, evidence)
        for client in clients:
            supervisor.stop(client, expected_exit=0, timeout_seconds=30)
        evidence["passed"] = True
        print("PASS: K opened Fatima's conversation on both replicas (only the initiator answers), 1 advanced and "
              "Esc closed it; the letter holder gave her Enrico's letter, she led the squad across A9, and after a "
              "shared return, travel and peaceful entry she led it through A10 until her hideout action took the squad down "
              "into Miguel's basement through the checkpointed co-op traversal, where the basement conversations played "
              "through, Miguel read Enrico's letter, Ira joined the squad, Miguel sent the squad back up to A10, it walked "
              "to A9 on the map, and every merc's militia order was a stock refusal there (no facility in Omerta).",
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
