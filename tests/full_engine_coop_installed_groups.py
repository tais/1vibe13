"""Read-only live friendly-group evidence; no travel orders or native simulation."""
import time

from full_engine_coop_installed_movement import MovementFailure, _Probe, _guard, _states


def integer(value, low, high):
    return type(value) is int and low <= value <= high


def validate(value):
    if (not isinstance(value, dict) or value.get("kind") != "groups" or
            not integer(value.get("request"), 1, 2**64 - 1) or type(value.get("ready")) is not bool):
        raise MovementFailure("invalid group response")
    if not value["ready"]:
        if set(value) != {"request", "kind", "ready"}:
            raise MovementFailure("unready client exposed stale groups")
        return False
    if (set(value) != {"request", "kind", "ready", "session", "revision", "available", "groups"} or
            not integer(value["session"], 1, 2**64 - 1) or not integer(value["revision"], 1, 2**64 - 1) or
            type(value["available"]) is not bool or not isinstance(value["groups"], list) or len(value["groups"]) > 255 or
            (not value["available"] and value["groups"])):
        raise MovementFailure("invalid group snapshot header")
    previous_group, actor_slots = 0, set()
    bounds = {"slot": (1, 255), "inc": (1, 2**32 - 1), "x": (1, 16), "y": (1, 16), "z": (0, 3),
              "next_x": (0, 16), "next_y": (0, 16), "transport": (0, 255), "destination_x": (0, 16),
              "destination_y": (0, 16), "arrival_minutes": (0, 2**32 - 1), "traverse_minutes": (0, 2**32 - 1)}
    for group in value["groups"]:
        if (not isinstance(group, dict) or set(group) != {*bounds, "between", "vehicle", "members"} or
                any(not integer(group[key], low, high) for key, (low, high) in bounds.items()) or
                type(group["between"]) is not bool or type(group["vehicle"]) is not bool or
                not isinstance(group["members"], list) or not 1 <= len(group["members"]) <= 256 or
                group["slot"] <= previous_group or bool(group["destination_x"]) != bool(group["destination_y"])):
            raise MovementFailure("invalid friendly group record")
        previous_group = group["slot"]
        if group["between"]:
            if (not group["next_x"] or not group["next_y"] or
                    abs(group["next_x"] - group["x"]) + abs(group["next_y"] - group["y"]) != 1):
                raise MovementFailure("invalid native next-leg destination")
        elif any(group[key] for key in ("next_x", "next_y", "arrival_minutes", "traverse_minutes")):
            raise MovementFailure("idle group retained stale transit data")
        previous_member = -1
        for member in group["members"]:
            if (not isinstance(member, dict) or set(member) != {"slot", "inc", "profile", "assignment"} or
                    not integer(member["slot"], 0, 65534) or not integer(member["inc"], 1, 2**32 - 1) or
                    not integer(member["profile"], 0, 65535) or not integer(member["assignment"], -128, 127) or
                    member["slot"] <= previous_member or member["slot"] in actor_slots):
                raise MovementFailure("invalid or duplicated group member")
            previous_member = member["slot"]
            actor_slots.add(member["slot"])
            if len(actor_slots) > 256:
                raise MovementFailure("friendly member capacity exceeded")
    return value["available"]


def projection(value):
    return {key: value[key] for key in ("session", "revision", "available", "groups")}


def observations(probes, deadline, guard):
    requests = [probe.send("GROUPS") for probe in probes]
    return [probe.receive(request, "groups", deadline, guard) for probe, request in zip(probes, requests)]


def verify_tactical_binding(value, states):
    members = {(member["slot"], member["inc"]): member["profile"]
               for group in value["groups"] for member in group["members"]}
    assigned = set()
    for state in states:
        if state.get("ready") is not True or state.get("session") != value["session"]:
            raise MovementFailure("group observation does not match a ready tactical session")
        visible = {(actor["slot"], actor["inc"]): actor["profile"] for actor in state["actors"] if actor["team"] == 0}
        if visible != members:
            raise MovementFailure("live group identities/profiles disagree with the friendly tactical roster")
        owned = {tuple(actor) for actor in state["assigned"]}
        if assigned & owned or not owned.issubset(members):
            raise MovementFailure("tactical ownership contradicts the grouped mercenaries")
        assigned.update(owned)
    if assigned != set(members):
        raise MovementFailure("starter group membership does not cover the two clients' exact assigned actors")


def wait_shared(clients, server, timeout, previous=None):
    if len(clients) != 2:
        raise MovementFailure("group agreement requires two clients")
    deadline = time.monotonic() + timeout
    probes, guard = [_Probe(client) for client in clients], _guard([server, *clients])
    last = list(previous) if previous else [None, None]
    while time.monotonic() < deadline:
        current = observations(probes, deadline, guard)
        available = [validate(value) for value in current]
        for index, value in enumerate(current):
            if not value["ready"]:
                continue
            old = last[index]
            if old and (value["session"] != old["session"] or value["revision"] < old["revision"] or
                        (value["revision"] == old["revision"] and projection(value) != projection(old))):
                raise MovementFailure("group observation lineage regressed or contradicted itself")
            last[index] = value
        if all(available) and projection(current[0]) == projection(current[1]):
            # This installed fixture starts with four mercenaries. Do not let
            # an empty but well-formed replacement count as live roster proof.
            if sum(len(group["members"]) for group in current[0]["groups"]) != 4:
                raise MovementFailure("starter campaign did not expose all four grouped mercenaries")
            verify_tactical_binding(current[0], _states(probes, deadline, guard))
            return current
        time.sleep(0.05)
    raise MovementFailure("clients did not converge on the live friendly groups")


def verify_cleared(clients, timeout):
    values = observations([_Probe(client) for client in clients], time.monotonic() + timeout, _guard(clients))
    for value in values:
        validate(value)
        if value["ready"]:
            raise MovementFailure("disconnected client retained a campaign group observation")
    return values
