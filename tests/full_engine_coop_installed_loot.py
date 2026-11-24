"""Read-only correlation of installed nearby-loot models and owner authority.

This inspects the real client's projected marker model, not pixels or mouse
interaction. Empty coherent views are valid regression evidence, never proof
that a nonempty marker was painted or an item was acquired.
"""

from __future__ import annotations

import copy
import time

from full_engine_coop_installed_inventory import _inventories, _valid_slot
from full_engine_coop_installed_movement import MovementFailure, _Probe, _guard, _states


class LootFailure(MovementFailure):
    pass


class _RetryView(Exception):
    pass


MAX_MARKERS = 128
RADIUS = 12
MAX_ATTEMPTS = 128
U64_MAX = 18446744073709551615


def _integer(value, minimum, maximum):
    return type(value) is int and minimum <= value <= maximum


def _identity(value):
    return (isinstance(value, list) and len(value) == 2 and
            _integer(value[0], 0, 255) and _integer(value[1], 1, 4294967295))


def _ground(inventory, columns, rows):
    ground = inventory.get("ground")
    nearby = inventory.get("nearby_loot")
    if (not isinstance(ground, dict) or set(ground) != {"grid", "level", "truncated", "items"} or
            not _integer(ground["grid"], -1, columns * rows - 1) or
            not _integer(ground["level"], -1, 1) or type(ground["truncated"]) is not bool or
            not isinstance(ground["items"], list) or len(ground["items"]) > 64 or
            (ground["truncated"] and len(ground["items"]) != 64) or
            not isinstance(nearby, dict) or set(nearby) != {"truncated", "markers"} or
            type(nearby["truncated"]) is not bool or not isinstance(nearby["markers"], list) or
            len(nearby["markers"]) > MAX_MARKERS or
            (nearby["truncated"] and len(nearby["markers"]) != MAX_MARKERS)):
        raise LootFailure("malformed ground/nearby inventory scope, count or truncation")
    if ground["grid"] == -1 or ground["level"] == -1:
        if (ground["grid"] != -1 or ground["level"] != -1 or ground["items"] or
                ground["truncated"] or nearby["markers"] or nearby["truncated"]):
            raise LootFailure("absent ground scope cannot contain loot or truncation claims")
        return
    previous_id = None
    for index, item in enumerate(ground["items"]):
        if (not isinstance(item, dict) or set(item) != {"id", "summary"} or
                not isinstance(item["id"], list) or len(item["id"]) != 2 or
                not _integer(item["id"][0], 0, 4294967294) or
                not _integer(item["id"][1], 1, 4294967295) or
                not _valid_slot(item["summary"], index) or item["summary"]["support"] != 1 or
                item["summary"]["status_kind"] == 0 or
                (previous_id is not None and tuple(item["id"]) <= previous_id)):
            raise LootFailure("malformed exact ground-item identities or typed summaries")
        previous_id = tuple(item["id"])
    previous_grid = -1
    origin_row, origin_column = divmod(ground["grid"], columns)
    for marker in nearby["markers"]:
        if (not isinstance(marker, dict) or set(marker) != {"grid", "level", "medical"} or
                not _integer(marker["grid"], 0, columns * rows - 1) or
                not _integer(marker["level"], 0, 1) or marker["level"] != ground["level"] or
                type(marker["medical"]) is not bool or marker["grid"] <= previous_grid):
            raise LootFailure("nearby marker must be a canonical, sorted same-floor location, never an item ID")
        row, column = divmod(marker["grid"], columns)
        if abs(row - origin_row) > RADIUS or abs(column - origin_column) > RADIUS:
            raise LootFailure("nearby marker crosses radius12 or wraps a world row")
        previous_grid = marker["grid"]


def _checked_states(states, expected_world=None):
    if not isinstance(states, list) or len(states) != 2:
        raise LootFailure("loot view certification requires exactly two replicas")
    assignments = []
    for state in states:
        if not isinstance(state, dict) or type(state.get("ready")) is not bool:
            raise LootFailure("malformed replica readiness")
        if not state["ready"]:
            raise _RetryView("replica is not ready")
        if (type(state.get("resynchronizing")) is not bool or
                not _integer(state.get("outstanding"), 0, U64_MAX) or
                any(not _integer(state.get(field), 1, U64_MAX) for field in ("session", "world", "revision")) or
                any(not _integer(state.get(field), 1, 2000) for field in ("columns", "rows"))):
            raise LootFailure("malformed current replica scope or dimensions")
        if state["resynchronizing"] or state["outstanding"]:
            raise _RetryView("replica is resynchronizing or has an outstanding gameplay command")
        assigned = state.get("assigned")
        if (not isinstance(assigned, list) or not 1 <= len(assigned) <= 256 or
                any(not _identity(identity) for identity in assigned) or
                assigned != [list(identity) for identity in sorted({tuple(identity) for identity in assigned})]):
            raise LootFailure("malformed or repeated owner assignments")
        assignments.append(assigned)
    scope = (states[0]["session"], states[0]["world"])
    if ((expected_world is not None and scope != expected_world) or
            (states[1]["session"], states[1]["world"]) != scope or
            (states[0]["columns"], states[0]["rows"]) != (states[1]["columns"], states[1]["rows"])):
        raise LootFailure("loot inspection changed world identity or replicas disagree on dimensions")
    caches = _inventories(states)
    for state, assigned, cache in zip(states, assignments, caches):
        if set(cache) != {tuple(identity) for identity in assigned}:
            raise _RetryView("current owner inventory cache is incomplete")
        for inventory in cache.values():
            if (any(not _integer(inventory.get(field), 1, U64_MAX) for field in ("session", "world")) or
                    not _identity(inventory.get("actor"))):
                raise LootFailure("malformed private inventory identity")
            _ground(inventory, state["columns"], state["rows"])
    return scope, caches


def _public_actor(state, identity):
    actors = state.get("actors")
    if not isinstance(actors, list):
        raise LootFailure("missing public actor list")
    matches = [actor for actor in actors if isinstance(actor, dict) and
               (actor.get("slot"), actor.get("inc")) == identity]
    if len(matches) != 1:
        raise LootFailure("loot view lost or duplicated exact public actor")
    actor = matches[0]
    if (not _integer(actor.get("slot"), 0, 255) or not _integer(actor.get("inc"), 1, 4294967295) or
            any(actor.get(field) is not True for field in ("pose", "active", "in_sector")) or
            not _integer(actor.get("grid"), 0, state["columns"] * state["rows"] - 1) or
            not _integer(actor.get("level"), 0, 1)):
        raise LootFailure("loot view selected actor has no canonical active public pose")
    return actor["grid"], actor["level"]


def _validate_view(reply, request, sender, before, after, before_cache, after_cache, world):
    if (not isinstance(reply, dict) or reply.get("kind") != "loot_ui" or
            not _integer(reply.get("request"), 1, 10000) or reply["request"] != request or
            type(reply.get("ready")) is not bool):
        raise LootFailure("malformed or mismatched LOOT_UI response")
    if not reply["ready"]:
        raise _RetryView("loot model is stale, camera changed, or not initialized")
    if (set(reply) != {"request", "kind", "ready", "actor", "session", "world", "baseline",
                      "inventory_revision", "origin_grid", "origin_level", "viewport", "available",
                      "count", "truncated", "markers", "hovered"} or
            not _identity(reply.get("actor")) or
            any(not _integer(reply.get(field), 1, U64_MAX)
                for field in ("session", "world", "baseline", "inventory_revision")) or
            not _integer(reply.get("origin_grid"), 0, before[sender]["columns"] * before[sender]["rows"] - 1) or
            not _integer(reply.get("origin_level"), 0, 1) or
            not _integer(reply.get("available"), 0, MAX_MARKERS) or
            not _integer(reply.get("count"), 0, MAX_MARKERS) or
            type(reply.get("truncated")) is not bool or
            not isinstance(reply.get("markers"), list) or len(reply["markers"]) != reply["count"] or
            reply["count"] > reply["available"] or
            not _integer(reply.get("hovered"), -1, reply["count"] - 1)):
        raise LootFailure("malformed loot model scope, count, availability, truncation or hover index")
    identity = tuple(reply["actor"])
    owner = before_cache[sender].get(identity)
    if owner is None or identity not in after_cache[sender]:
        raise LootFailure("loot model selected a foreign or unassigned actor")
    if (reply["session"], reply["world"]) != world:
        raise LootFailure("loot model has foreign world scope")
    if (before[sender]["assigned"] != after[sender]["assigned"] or owner != after_cache[sender][identity] or
            (reply["baseline"], reply["inventory_revision"]) != (owner["baseline"], owner["revision"])):
        raise _RetryView("loot model changed private lineage during read-only correlation")
    origin = owner["ground"]["grid"], owner["ground"]["level"]
    if origin[0] < 0 or (reply["origin_grid"], reply["origin_level"]) != origin:
        raise _RetryView("loot model has stale or unavailable ground origin")
    if any(_public_actor(state, identity) != origin for state in (*before, *after)):
        raise _RetryView("public actor pose changed or replicas have not caught up to ground origin")
    authorized = owner["nearby_loot"]
    if (reply["available"] != len(authorized["markers"]) or
            reply["truncated"] != authorized["truncated"]):
        raise LootFailure("loot model availability/truncation disagrees with exact owner revision")
    viewport = reply["viewport"]
    if (not isinstance(viewport, list) or len(viewport) != 4 or
            any(not _integer(value, 0, 32767) for value in viewport) or
            viewport[0] >= viewport[2] or viewport[1] >= viewport[3]):
        raise LootFailure("loot model viewport is malformed")
    previous = -1
    for marker in reply["markers"]:
        if (not isinstance(marker, dict) or set(marker) != {"grid", "level", "medical", "x", "y"} or
                not _integer(marker["grid"], 0, before[sender]["columns"] * before[sender]["rows"] - 1) or
                not _integer(marker["level"], 0, 1) or type(marker["medical"]) is not bool or
                not _integer(marker["x"], viewport[0], viewport[2] - 1) or
                not _integer(marker["y"], viewport[1], viewport[3] - 1)):
            raise LootFailure("projected marker has malformed geometry/type or claims item identity")
        location = {field: marker[field] for field in ("grid", "level", "medical")}
        try:
            index = authorized["markers"].index(location)
        except ValueError as error:
            raise LootFailure("projected marker is not in the exact owner-authorized location list") from error
        if index <= previous:
            raise LootFailure("projected markers are not a strict authority-order subsequence")
        previous = index
    return {"sender": sender + 1, "actor": list(identity), "session": world[0], "world": world[1],
            "baseline": owner["baseline"], "inventory_revision": owner["revision"],
            "origin_grid": origin[0], "origin_level": origin[1], "viewport": copy.deepcopy(viewport),
            "available": reply["available"], "count": reply["count"], "truncated": reply["truncated"],
            "authorized_markers": copy.deepcopy(authorized["markers"]),
            "projected_markers": copy.deepcopy(reply["markers"]), "hovered_index": reply["hovered"],
            "nonempty_marker_view_verified": reply["count"] > 0}


def certify_two_client_loot_views(clients, server, timeout_seconds):
    """Read STATE/LOOT_UI only; retry changing snapshots, never send gameplay."""
    try:
        if len(clients) != 2 or type(timeout_seconds) not in (int, float) or not 0 < timeout_seconds <= 300:
            raise LootFailure("loot inspection requires two clients and a positive deadline at most300seconds")
        deadline = time.monotonic() + timeout_seconds
        probes = [_Probe(client) for client in clients]
        guard = _guard([server, *clients])
        world = None
        last_retry = "no coherent loot view"
        for attempt in range(1, MAX_ATTEMPTS + 1):
            if time.monotonic() >= deadline:
                break
            guard()
            try:
                before = _states(probes, deadline, guard)
                scope, before_cache = _checked_states(before, world)
                world = scope
                requests = [probe.send("LOOT_UI") for probe in probes]
                replies = [probe.receive(request, "loot_ui", deadline, guard)
                           for probe, request in zip(probes, requests)]
                after = _states(probes, deadline, guard)
                _, after_cache = _checked_states(after, world)
                views = [_validate_view(reply, request, sender, before, after, before_cache, after_cache, world)
                         for sender, (reply, request) in enumerate(zip(replies, requests))]
                if [state.get("receipt") for state in before] != [state.get("receipt") for state in after]:
                    raise _RetryView("terminal receipt changed during read-only inspection")
                guard()
                nonempty = sum(view["nonempty_marker_view_verified"] for view in views)
                print(f"installed loot views: two current owner scopes verified; {nonempty}/2 nonempty projected views; "
                      "no pixel, mouse or pickup claim", flush=True)
                return {"session": world[0], "world": world[1], "views": views, "attempts": attempt,
                        "scope_and_projection_subset_verified": True,
                        "nonempty_marker_view_verified": nonempty == 2,
                        "clients_with_nonempty_marker_view": nonempty,
                        "no_gameplay_submission": True, "paint_verified": False,
                        "actual_mouse_interaction_verified": False, "pickup_verified": False}
            except _RetryView as error:
                last_retry = str(error)
                time.sleep(min(0.1, max(0.0, deadline - time.monotonic())))
        raise LootFailure(f"loot view did not correlate within bounded read-only attempts: {last_retry}")
    except LootFailure:
        raise
    except (MovementFailure, KeyError, TypeError, ValueError, IndexError, OSError) as error:
        raise LootFailure(f"loot view inspection failed: {error}") from error
