"""Read-only installed clock/leader evidence. Never sends time/gameplay commands."""

import time

from full_engine_coop_installed_movement import MovementFailure, _Probe, _guard


FIELDS = ("session", "revision", "world_seconds", "phase", "compression_mode", "paused",
          "pause_locked", "compression_active", "time_interrupted", "leader_revision",
          "leader_ready", "ready_peers", "leader", "time_control_revision", "arrival")
# Ended contracts awaiting a decision; always sent by the driver, optional in
# recorded fixtures that predate it.
OPTIONAL_FIELDS = ("contracts_due", "campaign_complete")
BOOLEANS = ("paused", "pause_locked", "compression_active", "time_interrupted", "leader_ready", "local_leader")
ARRIVAL_FLAGS = ("final_destination", "native_auto", "native_enter", "native_retreat", "native_placement")
EMPTY_ARRIVAL = {**dict.fromkeys(("decision", "kind", "stage", "x", "y", "z", "encounter", "pending", "involved", "uninvolved"), 0),
                 **dict.fromkeys(ARRIVAL_FLAGS, False)}


def validate_arrival(arrival):
    if not isinstance(arrival, dict) or set(arrival) != set(EMPTY_ARRIVAL):
        raise MovementFailure("incomplete arrival observation")
    bounds = {"decision": (0, 2**64 - 1), "kind": (0, 3), "stage": (0, 5), "x": (0, 16), "y": (0, 16),
              "z": (0, 3), "encounter": (0, 255), "pending": (0, 256), "involved": (0, 256), "uninvolved": (0, 256)}
    if (any(type(arrival[key]) is not int or not low <= arrival[key] <= high for key, (low, high) in bounds.items()) or
            any(type(arrival[key]) is not bool for key in ARRIVAL_FLAGS)):
        raise MovementFailure("invalid arrival field type or bounds")
    if not arrival["decision"]:
        if arrival != EMPTY_ARRIVAL:
            raise MovementFailure("no-decision observation retained stale arrival fields")
        return False
    if (not all(arrival[key] for key in ("kind", "stage", "x", "y", "pending")) or
            (arrival["final_destination"] and arrival["kind"] != 1) or
            (arrival["kind"] != 3 and (arrival["encounter"] or arrival["stage"] in (2, 3)))):
        raise MovementFailure("invalid native arrival context")
    if arrival["stage"] == 2:
        if (not arrival["involved"] or arrival["involved"] + arrival["uninvolved"] > 256 or
                (arrival["native_placement"] and not arrival["native_enter"])):
            raise MovementFailure("invalid prepared battle observation")
    elif arrival["involved"] or arrival["uninvolved"] or any(arrival[key] for key in ARRIVAL_FLAGS[1:]):
        raise MovementFailure("unprepared arrival advertises native battle choices")
    return True


def validate(status):
    if not isinstance(status, dict) or status.get("kind") != "campaign" or type(status.get("ready")) is not bool:
        raise MovementFailure("malformed campaign status response")
    if type(status.get("request")) is not int or status["request"] <= 0:
        raise MovementFailure("campaign status request identity is invalid")
    if not status["ready"]:
        if set(status) != {"request", "kind", "ready"}:
            raise MovementFailure("unavailable campaign response leaked stale status")
        return False
    if set(status) - set(OPTIONAL_FIELDS) != {*FIELDS, "request", "kind", "ready", "local_leader"}:
        raise MovementFailure("campaign status fields are incomplete or unexpected")
    bounds = {"session": (1, 2**64 - 1), "revision": (1, 2**64 - 1), "world_seconds": (0, 2**32 - 1),
              "phase": (1, 4), "compression_mode": (-1, 5), "leader_revision": (1, 2**64 - 1),
              "ready_peers": (1, 4), "time_control_revision": (1, 2**64 - 1)}
    if "campaign_complete" in status and type(status["campaign_complete"]) is not bool:
        raise MovementFailure("invalid campaign-complete flag")
    if "contracts_due" in status and (type(status["contracts_due"]) is not int or not 0 <= status["contracts_due"] <= 255):
        raise MovementFailure("campaign status contract count is invalid")
    if any(type(status[key]) is not int or not low <= status[key] <= high for key, (low, high) in bounds.items()):
        raise MovementFailure("campaign status integer is invalid")
    if any(type(status[key]) is not bool for key in BOOLEANS):
        raise MovementFailure("campaign status flag is invalid")
    leader = status["leader"]
    if (not isinstance(leader, list) or len(leader) != 16 or not any(leader) or
            any(type(byte) is not int or not 0 <= byte <= 255 for byte in leader) or
            (status["local_leader"] and not status["leader_ready"]) or
            (status["compression_active"] and (status["paused"] or status["compression_mode"] == 0))):
        raise MovementFailure("campaign status leadership or native pause flags are invalid")
    if validate_arrival(status["arrival"]) and (status["phase"] not in (1, 2) or not status["paused"] or status["compression_active"]):
        raise MovementFailure("arrival hold is inconsistent with the server clock/phase")
    return True


def projection(status):
    return {key: status[key] for key in FIELDS}


def require_progress(current, previous):
    if (current["session"] != previous["session"] or current["leader"] != previous["leader"] or
            current["leader_revision"] != previous["leader_revision"] or
            current["time_control_revision"] < previous["time_control_revision"] or
            current["revision"] < previous["revision"] or current["world_seconds"] < previous["world_seconds"] or
            (current["revision"] == previous["revision"] and projection(current) != projection(previous)) or
            (current["time_control_revision"] == previous["time_control_revision"] and current["arrival"] != previous["arrival"])):
        raise MovementFailure("campaign clock/leader lineage changed or regressed")


def observations(probes, deadline, guard):
    requests = [probe.send("CAMPAIGN") for probe in probes]
    return [probe.receive(request, "campaign", deadline, guard) for probe, request in zip(probes, requests)]


def wait_shared(clients, server, timeout, previous=None):
    if len(clients) != 2:
        raise MovementFailure("campaign status check requires exactly two clients")
    deadline = time.monotonic() + timeout
    probes, guard = [_Probe(client) for client in clients], _guard([server, *clients])
    last = list(previous) if previous else [None, None]
    while time.monotonic() < deadline:
        current = observations(probes, deadline, guard)
        ready = [validate(status) for status in current]
        for index, status in enumerate(current):
            if not ready[index]:
                continue
            if last[index] is not None:
                require_progress(status, last[index])
            last[index] = status
        if all(ready) and projection(current[0]) == projection(current[1]):
            if current[0]["leader_ready"] and current[0]["ready_peers"] == 2:
                if sum(status["local_leader"] for status in current) != 1:
                    raise MovementFailure("two ready clients do not identify exactly one time leader")
                return current
        time.sleep(0.05)
    raise MovementFailure("clients did not converge on the same server clock and time leader")


def wait_leader_offline(observer, server, previous, timeout):
    deadline = time.monotonic() + timeout
    probe, guard = _Probe(observer), _guard([server, observer])
    last = previous
    while time.monotonic() < deadline:
        status = observations([probe], deadline, guard)[0]
        if validate(status):
            require_progress(status, last)
            last = status
            if status["local_leader"]:
                raise MovementFailure("remaining client silently took over time leadership")
            if not status["leader_ready"] and status["ready_peers"] == 1:
                return status
        time.sleep(0.05)
    raise MovementFailure("remaining client did not observe its time leader going offline")


def verify_cleared(clients, timeout):
    deadline = time.monotonic() + timeout
    current = observations([_Probe(client) for client in clients], deadline, _guard(clients))
    if any(validate(status) for status in current):
        raise MovementFailure("disconnected client still exposes a live campaign clock or leader")
    return current
