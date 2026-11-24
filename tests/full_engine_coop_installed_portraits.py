"""Read-only installed portrait descriptors and native roster screenshot evidence.

PNG checks prove capture structure, not that a face is visually correct. The
retained screenshots require human/root visual inspection before claiming that.
No native actor/item state or client-local simulation is modified by this probe.
"""

from __future__ import annotations

import copy
import time

from full_engine_coop_installed_fire import _same_world
from full_engine_coop_installed_inventory import (
    NO_SLOT, UI_SCREENSHOT, _inventories, _ui_read, _wait_inventory_png,
)
from full_engine_coop_installed_movement import MovementFailure, _Probe, _guard, _states


class PortraitFailure(MovementFailure):
    pass


ROSTER_SCREENSHOTS = (UI_SCREENSHOT, "Screenshots/SCREEN00001.png")


def _integer(value, minimum, maximum):
    return type(value) is int and minimum <= value <= maximum


def _protected(states, world=None):
    if (len(states) != 2 or
            any(state.get("ready") is not True or state.get("resynchronizing") is not False or
                type(state.get("outstanding")) is not int or state["outstanding"] != 0 or
                type(state.get("commands_blocked")) is not bool or
                any(not _integer(state.get(field), 1, 18446744073709551615)
                    for field in ("session", "world")) for state in states) or
            not _same_world(states, world)):
        raise PortraitFailure("portrait capture requires two ready, unlocked current replicas")
    scope = (states[0]["session"], states[0]["world"])
    if any(not _integer(value, 1, 18446744073709551615) for value in scope):
        raise PortraitFailure("portrait capture has an invalid world identity")
    inventories = _inventories(states)
    assignments = []
    for state, cache in zip(states, inventories):
        identities = state["assigned"]
        if (not isinstance(identities, list) or not 1 <= len(identities) <= 256 or
                any(not isinstance(identity, list) or len(identity) != 2 or
                    not _integer(identity[0], 0, 255) or
                    not _integer(identity[1], 1, 4294967295) for identity in identities) or
                len({tuple(identity) for identity in identities}) != len(identities) or
                set(cache) != {tuple(identity) for identity in identities}):
            raise PortraitFailure("portrait roster has malformed/duplicate ownership or incomplete private scope")
        assignments.append(copy.deepcopy(identities))
    owned = sorted(tuple(identity) for roster in assignments for identity in roster)
    descriptors = []
    for identity in owned:
        replicas = []
        for state in states:
            matches = [actor for actor in state["actors"] if (actor.get("slot"), actor.get("inc")) == identity]
            if len(matches) != 1:
                raise PortraitFailure("portrait roster lost an exact public actor identity")
            actor = matches[0]
            portrait = actor.get("portrait")
            if (not _integer(actor.get("profile"), 0, 254) or actor["profile"] == 200 or
                    actor.get("active") is not True or actor.get("in_sector") is not True or
                    not isinstance(portrait, dict) or set(portrait) != {"family", "index", "camouflage"} or
                    not _integer(portrait["family"], 1, 2) or
                    not _integer(portrait["index"], 0, 255) or
                    not _integer(portrait["camouflage"], 0, 4)):
                raise PortraitFailure("owned actor portrait is absent or malformed; no visual face claim")
            replicas.append({"actor": list(identity), "profile": actor["profile"], "portrait": copy.deepcopy(portrait)})
        if replicas[0] != replicas[1]:
            raise PortraitFailure("clients disagree on an owned actor's profile or portrait descriptor")
        descriptors.append(replicas[0])
    return {"session": scope[0], "world": scope[1], "assignments": assignments,
            "inventories": copy.deepcopy(inventories), "descriptors": descriptors,
            "receipts": copy.deepcopy([state.get("receipt") for state in states])}


def _capture_path(client, sender, inventory_evidence):
    root = client.log_path.parent
    if root.is_symlink() or not root.is_dir():
        raise PortraitFailure("portrait client work directory is unavailable or redirected")
    first, second = (root / name for name in ROSTER_SCREENSHOTS)
    if first.parent.is_symlink() or (first.parent.exists() and not first.parent.is_dir()):
        raise PortraitFailure("portrait screenshot directory is redirected or invalid")
    if first.is_symlink() or second.is_symlink() or second.exists():
        raise PortraitFailure("portrait capture refuses redirected or pre-existing output")
    prior = (inventory_evidence or {}).get("ui", {})
    inventory_here = prior.get("sender") == sender + 1
    if first.exists():
        if (not first.is_file() or not inventory_here or prior.get("closed") is not True or
                prior.get("screenshot") != str(first)):
            raise PortraitFailure("pre-existing first screenshot has no exact completed inventory evidence")
        return second, ROSTER_SCREENSHOTS[1]
    if inventory_here:
        raise PortraitFailure("completed inventory screenshot disappeared before roster capture")
    return first, ROSTER_SCREENSHOTS[0]


def _unblocked_state(probes, guard, deadline, *, initial=None):
    world = (initial["session"], initial["world"]) if initial else None
    while time.monotonic() < deadline:
        states = _states(probes, deadline, guard)
        protected = _protected(states, world)
        if initial is not None and protected != initial:
            raise PortraitFailure("portrait protected owner state changed before capture; no replay")
        if all(state.get("commands_blocked") is False for state in states):
            return protected
        time.sleep(0.1)
    raise PortraitFailure("portrait capture native command-block gate did not clear; no capture submitted")


def certify_two_client_portraits(clients, server, timeout_seconds, *, inventory_evidence=None):
    """Capture each current Screen roster once, never replaying an ambiguous request."""
    try:
        if len(clients) != 2:
            raise PortraitFailure("portrait certification requires exactly two clients")
        if type(timeout_seconds) not in (int, float) or not 0 < timeout_seconds <= 300:
            raise PortraitFailure("portrait deadline must be positive and at most 300 seconds")
        deadline = time.monotonic() + timeout_seconds
        probes = [_Probe(client) for client in clients]
        guard = _guard([server, *clients])
        initial = _unblocked_state(probes, guard, deadline)
        world = initial["session"], initial["world"]
        # Resolve both destinations before issuing either capture, so an old
        # artifact on client B cannot permit a partial/repeated A capture.
        paths = [_capture_path(client, sender, inventory_evidence) for sender, client in enumerate(clients)]
        screenshots = []
        for sender, probe in enumerate(probes):
            if time.monotonic() >= deadline:
                raise PortraitFailure("portrait capture exceeded its bounded deadline; no replay")
            _unblocked_state(probes, guard, deadline, initial=initial)
            ui = _ui_read(probe, guard, world, deadline)
            identity = tuple(ui["actor"])
            owner = initial["inventories"][sender].get(identity)
            if (not _integer(identity[0], 0, 255) or not _integer(identity[1], 1, 4294967295) or
                    owner is None or ui["open"] or ui["source"] != NO_SLOT or ui["inspected"] != NO_SLOT or
                    any(not _integer(ui.get(field), 1, 18446744073709551615)
                        for field in ("baseline", "inventory_revision")) or
                    (ui["baseline"], ui["inventory_revision"]) != (owner["baseline"], owner["revision"])):
                raise PortraitFailure("roster capture requires the exact Screen-selected owner's closed panel")
            path, relative = paths[sender]
            if path.exists() or path.is_symlink() or path.parent.is_symlink():
                raise PortraitFailure("portrait output appeared or was redirected before capture")
            request = probe.send("ROSTER_CAPTURE", *identity, *world, owner["baseline"], owner["revision"])
            reply = probe.receive(request, "roster_capture", deadline, guard)
            if (reply.get("queued") is not True or reply.get("screenshot") != relative or
                    reply.get("actor") != list(identity) or
                    any(type(value) is not int for value in reply.get("actor", [])) or
                    any(not _integer(reply.get(field), 1, 18446744073709551615)
                        for field in ("session", "world", "baseline", "inventory_revision")) or
                    (reply.get("session"), reply.get("world"), reply.get("baseline"), reply.get("inventory_revision")) !=
                    (*world, owner["baseline"], owner["revision"]) or
                    (reply.get("width"), reply.get("height")) != (ui["width"], ui["height"]) or
                    type(reply.get("width")) is not int or type(reply.get("height")) is not int):
                raise PortraitFailure(f"roster capture reply was rejected or ambiguous; no replay: {reply}")
            size = _wait_inventory_png(path, (ui["width"], ui["height"]), guard, deadline)
            closed = _ui_read(probe, guard, world, deadline, identity, owner)
            if closed["open"] or closed["source"] != NO_SLOT or closed["inspected"] != NO_SLOT:
                raise PortraitFailure("roster capture changed the closed Screen panel")
            if _protected(_states(probes, deadline, guard), world) != initial:
                raise PortraitFailure("roster capture changed assignments, descriptors, inventory or receipts")
            screenshots.append({"sender": sender + 1, "selected_actor": list(identity),
                                "screenshot": str(path), "screenshot_bytes": size,
                                "pixel_size": [ui["width"], ui["height"]],
                                "baseline": owner["baseline"], "inventory_revision": owner["revision"]})
        guard()
        print("installed portraits: both owned rosters agree; two native PNGs captured without gameplay mutation; "
              "visual inspection still required", flush=True)
        return {"session": world[0], "world": world[1], "assignments": initial["assignments"],
                "descriptors": initial["descriptors"], "screenshots": screenshots,
                "no_gameplay_submission": True, "visual_inspection_required": True}
    except PortraitFailure:
        raise
    except (MovementFailure, KeyError, TypeError, ValueError, IndexError, OSError) as error:
        raise PortraitFailure(f"portrait certification failed: {error}") from error
