"""Bounded movement certification through the installed test client's stdin seam."""

from __future__ import annotations

import json
import time


PREFIX = "[co-op test] "
CELL_Q8 = 10 * 256
MAX_FRESH_SNAPSHOT_ATTEMPTS = 8
FRESH_SNAPSHOT_RETRY_DELAYS = (0.01, 0.03, 0.05, 0.09)
STABLE_FIELDS = (
    "slot", "inc", "grid", "level", "direction", "stance", "ap", "life",
    "world_x", "world_y",
)
POSITION_FIELDS = ("slot", "inc", "grid", "level", "world_x", "world_y")
PHASE_FIELDS = ("turn_serial", "team", "turn_based", "in_combat", "commands_blocked")
NEIGHBORS = ((-1, 0), (0, 1), (1, 0), (0, -1),
             (-1, 1), (1, 1), (1, -1), (-1, -1))


class MovementFailure(RuntimeError):
    pass


class _Probe:
    def __init__(self, managed) -> None:
        self.managed = managed
        self.request = 0
        self.offset = managed.log_path.stat().st_size
        self.partial = b""

    def send(self, verb: str, *arguments: int) -> int:
        self.request += 1
        if self.request > 10000:
            raise MovementFailure("movement probe exceeded its request budget")
        command = " ".join(map(str, (verb, self.request, *arguments))) + "\n"
        try:
            self.managed.process.stdin.write(command.encode("ascii"))
            self.managed.process.stdin.flush()
        except (AttributeError, BrokenPipeError, OSError) as error:
            raise MovementFailure(
                f"{self.managed.name} cannot receive movement probes: {error}") from error
        return self.request

    def receive(self, request: int, kind: str, deadline: float, guard) -> dict:
        while time.monotonic() < deadline:
            guard()
            with self.managed.log_path.open("rb") as source:
                source.seek(self.offset)
                chunk = source.read(1024 * 1024)
                self.offset = source.tell()
            lines = (self.partial + chunk).split(b"\n")
            self.partial = lines.pop()
            if len(self.partial) > 2 * 1024 * 1024:
                raise MovementFailure("movement probe response exceeded its size bound")
            answer = None
            for raw in lines:
                line = raw.decode("utf-8", errors="replace")
                if ("[co-op client] runtime failed:" in line or
                        "[co-op client] tactical presentation rejected" in line):
                    raise MovementFailure(f"{self.managed.name}: {line}")
                if not line.startswith(PREFIX):
                    continue
                try:
                    response = json.loads(line[len(PREFIX):])
                except (ValueError, TypeError) as error:
                    raise MovementFailure(
                        f"{self.managed.name} emitted malformed probe JSON") from error
                if response.get("request") == request:
                    kinds = kind if isinstance(kind, tuple) else (kind,)
                    if response.get("kind") not in kinds or answer is not None:
                        raise MovementFailure("unexpected or repeated movement probe response")
                    answer = response
            if answer is not None:
                return answer
            time.sleep(0.02)
        raise MovementFailure(
            f"{self.managed.name} timed out answering {kind} request {request}")


def _guard(processes):
    def check() -> None:
        for managed in processes:
            code = managed.process.poll()
            if code is not None:
                raise MovementFailure(
                    f"{managed.name} exited during movement certification: {code}")
    return check


def _states(probes, deadline: float, guard) -> list[dict]:
    requests = [probe.send("STATE") for probe in probes]
    return [probe.receive(request, "state", deadline, guard)
            for probe, request in zip(probes, requests)]


def _actor(state: dict, identity: tuple[int, int]) -> dict:
    matches = [actor for actor in state.get("actors", [])
               if (actor["slot"], actor["inc"]) == identity]
    if len(matches) != 1 or not matches[0].get("pose"):
        raise MovementFailure(f"replica lost rendered actor {identity}")
    return matches[0]


def _projection(actor: dict) -> dict:
    return {field: actor[field] for field in STABLE_FIELDS}


def _movement_observations(states: list[dict], identity: tuple[int, int]) -> str:
    observations = []
    for state in states:
        actor = next((actor for actor in state.get("actors", [])
                      if (actor.get("slot"), actor.get("inc")) == identity), None)
        observations.append({
            "ready": state.get("ready"), "world": state.get("world"),
            "revision": state.get("revision"),
            "resynchronizing": state.get("resynchronizing"),
            "outstanding": state.get("outstanding"),
            "receipt": state.get("receipt"),
            "actor": {field: actor.get(field) for field in
                      ("slot", "inc", "grid", "world_x", "world_y", "ap")}
            if actor is not None else None,
        })
    return json.dumps(observations, separators=(",", ":"))


def _same_world(states: list[dict], expected: tuple[int, int]) -> bool:
    for state in states:
        if not state.get("ready") or state.get("resynchronizing"):
            return False
        if (state.get("session"), state.get("world")) != expected:
            raise MovementFailure("movement certification changed tactical world identity")
    return True


def _require_two_owners(states: list[dict]) -> None:
    assignments = [{tuple(identity) for identity in state.get("assigned", [])}
                   for state in states]
    if len(assignments) != 2 or not all(assignments):
        raise MovementFailure("movement certification requires two nonempty actor assignments")
    if assignments[0] & assignments[1]:
        raise MovementFailure("movement clients do not have disjoint actor ownership")


def _require_completed_evidence(expected: dict) -> None:
    actors = expected.get("actors", [])
    identities = {(actor["slot"], actor["inc"]) for actor in actors}
    if len(actors) != 2 or len(identities) != 2:
        raise MovementFailure("movement evidence must contain two different completed actors")


def _candidate_moves(state: dict) -> list[tuple[tuple[int, int], int]]:
    if state["commands_blocked"]:
        return []
    assigned = {tuple(identity) for identity in state["assigned"]}
    actors = sorted((actor for actor in state["actors"]
                     if (actor["slot"], actor["inc"]) in assigned and
                     actor["life"] > 0 and actor["ap"] > 0 and actor["pose"]),
                    key=lambda actor: (-actor["ap"], actor["slot"], actor["inc"]))
    candidates = []
    columns, rows = state["columns"], state["rows"]
    for actor in actors:
        row, column = divmod(actor["grid"], columns)
        occupied = {other["grid"] for other in state["actors"]
                    if other["level"] == actor["level"] and other["life"] > 0}
        for dr, dc in NEIGHBORS:
            r, c = row + dr, column + dc
            if 0 <= r < rows and 0 <= c < columns:
                target = r * columns + c
                if target not in occupied:
                    candidates.append(((actor["slot"], actor["inc"]), target))
        if len(candidates) >= 8:
            break
    return candidates[:8]


def _wait_move(probes, guard, sender: int, command: int,
               identity: tuple[int, int], target: int,
               initial: list[dict], deadline: float) -> tuple[str, dict | None]:
    world = (initial[0]["session"], initial[0]["world"])
    before = [_actor(state, identity) for state in initial]
    combat_ap_cost = initial[sender]["turn_based"] and initial[sender]["in_combat"]
    if _projection(before[0]) != _projection(before[1]):
        raise MovementFailure("replicas disagreed before the movement command")
    row, column = divmod(target, initial[0]["columns"])
    center = (column * CELL_Q8 + CELL_Q8 // 2,
              row * CELL_Q8 + CELL_Q8 // 2)
    changed = [False] * len(probes)
    stable_since = None
    stable_value = None
    applied = False
    last_states = initial
    while time.monotonic() < deadline:
        try:
            states = _states(probes, deadline, guard)
        except MovementFailure as error:
            raise MovementFailure(
                f"{error}; last movement observations="
                f"{_movement_observations(last_states, identity)}") from error
        last_states = states
        if not _same_world(states, world):
            stable_since = None
            time.sleep(0.1)
            continue
        actors = [_actor(state, identity) for state in states]
        receipt = states[sender].get("receipt")
        if receipt and receipt["command"] == command:
            status, reason = receipt["status"], receipt["reason"]
            if status == 3 and reason == 0:
                applied = True
            elif status == 2 and reason in (5, 9, 12):
                if applied:
                    raise MovementFailure("move was rejected after an Applied receipt")
                if states[sender]["outstanding"] != 0:
                    raise MovementFailure("rejected move did not release its command lock")
                # Rejection is not permission to spend AP or move. Do not hide
                # an asynchronous side effect by immediately trying another tile.
                for actor, previous in zip(actors, before):
                    if ((actor["grid"], actor["world_x"], actor["world_y"]) !=
                            (previous["grid"], previous["world_x"], previous["world_y"]) or
                            actor["ap"] < previous["ap"]):
                        raise MovementFailure("rejected move changed position or spent AP")
                # RevisionMismatch and NotBaselineReady are both explicit,
                # non-executed refusals: retry only from another fresh view.
                return ("stale" if reason in (5, 9) else "rejected"), None
            elif status != 1:
                raise MovementFailure(
                    f"move {command} terminated with status={status}, reason={reason}")
        for index, (state, actor, previous) in enumerate(zip(states, actors, before)):
            if (state["revision"] > initial[index]["revision"] and
                    (actor["world_x"], actor["world_y"]) !=
                    (previous["world_x"], previous["world_y"])):
                changed[index] = True
        projections = [_projection(actor) for actor in actors]
        settled = (applied and states[sender]["outstanding"] == 0 and
                   all(changed) and projections[0] == projections[1] and
                   all(actor["grid"] == target and
                       (actor["world_x"], actor["world_y"]) == center and
                       actor["ap"] >= 0 and
                       (actor["ap"] < previous["ap"] if combat_ap_cost
                        else actor["ap"] >= previous["ap"])
                       for actor, previous in zip(actors, before)))
        if settled:
            if stable_since is None or stable_value != projections[0]:
                stable_since = time.monotonic()
                stable_value = projections[0]
            if time.monotonic() - stable_since >= 0.5:
                return "applied", stable_value
        else:
            stable_since = None
        time.sleep(0.1)
    raise MovementFailure(
        f"move {command} did not settle at grid {target} on both replicas; "
        f"Applied={applied}, coordinate changes={changed}; "
        f"last movement observations={_movement_observations(last_states, identity)}")


def certify_two_client_movement(clients, server, timeout_seconds: float) -> dict:
    """Move A, await settlement on A+B, then move B and await both again."""
    if len(clients) != 2:
        raise MovementFailure("movement certification requires exactly two clients")
    deadline = time.monotonic() + timeout_seconds
    probes = [_Probe(client) for client in clients]
    guard = _guard([server, *clients])
    initial = _states(probes, deadline, guard)
    world = (initial[0].get("session"), initial[0].get("world"))
    if not all(world) or not _same_world(initial, world):
        raise MovementFailure("two clients are not ready in the same tactical world")
    _require_two_owners(initial)
    expected = {"session": world[0], "world": world[1], "actors": [], "ap_checks": []}
    for sender in range(2):
        states = _states(probes, deadline, guard)
        if not _same_world(states, world) or states[sender]["outstanding"] != 0:
            raise MovementFailure("client is unavailable for its movement turn")
        _require_two_owners(states)
        candidates = _candidate_moves(states[sender])
        completed = False
        for identity, target in candidates:
            for retry in range(MAX_FRESH_SNAPSHOT_ATTEMPTS):
                states = _states(probes, deadline, guard)
                if not _same_world(states, world):
                    raise MovementFailure("client lost readiness before a move")
                request = probes[sender].send("MOVE", *identity, target)
                reply = probes[sender].receive(request, "move", deadline, guard)
                if not reply.get("submitted") or not reply.get("command"):
                    raise MovementFailure(f"movement controller rejected probe: {reply}")
                result, settled = _wait_move(
                    probes, guard, sender, reply["command"], identity, target,
                    states, min(deadline, time.monotonic() + 30.0))
                if result == "stale":
                    if retry == MAX_FRESH_SNAPSHOT_ATTEMPTS - 1:
                        raise MovementFailure(
                            f"movement exceeded {MAX_FRESH_SNAPSHOT_ATTEMPTS} fresh-snapshot attempts")
                    # Vary the delay to avoid repeatedly aligning with the
                    # server's snapshot/ack cycle on a local 16 Hz host.
                    time.sleep(FRESH_SNAPSHOT_RETRY_DELAYS[
                        retry % len(FRESH_SNAPSHOT_RETRY_DELAYS)])
                    continue
                if result == "applied":
                    if any((actor["slot"], actor["inc"]) == identity
                           for actor in expected["actors"]):
                        raise MovementFailure("both movement turns selected the same actor")
                    expected["actors"].append(settled)
                    ap_mode = ("combat" if states[sender]["turn_based"] and
                               states[sender]["in_combat"] else "realtime")
                    expected["ap_checks"].append({
                        "slot": identity[0], "inc": identity[1], "mode": ap_mode,
                        "turn_serial": states[sender]["turn_serial"],
                    })
                    print(f"installed movement: client {sender + 1}, actor "
                          f"{identity[0]}:{identity[1]}, target {target}, "
                          "Applied and settled on both replicas; "
                          f"AP check={ap_mode} "
                          f"({'cost verified' if ap_mode == 'combat' else 'agreement only; no combat AP certification'})",
                          flush=True)
                    completed = True
                break
            if completed:
                break
        if not completed:
            raise MovementFailure(
                f"client {sender + 1} had no accepted move among {len(candidates)} candidates")
    _require_completed_evidence(expected)
    guard()
    return expected


def _same_combat_ap_phase(state: dict, actor: dict, expected: dict) -> bool:
    check = next((check for check in expected.get("ap_checks", [])
                  if (check["slot"], check["inc"]) == (actor["slot"], actor["inc"])),
                 {"mode": "combat", "turn_serial": state["turn_serial"]})
    return (check["mode"] == "combat" and state["turn_based"] and state["in_combat"] and
            check["turn_serial"] == state["turn_serial"])


def _advanced_combat_phase(state: dict, actor: dict, expected: dict) -> bool:
    checks = [check for check in expected.get("ap_checks", [])
              if (check["slot"], check["inc"]) == (actor["slot"], actor["inc"])]
    # Missing historical evidence is not proof that combat advanced. Neither
    # a newer presentation revision nor an AP refresh alone proves a new turn.
    return (len(checks) == 1 and checks[0]["mode"] in ("combat", "realtime") and
            0 < checks[0]["turn_serial"] < state["turn_serial"] and
            (checks[0]["mode"] == "combat" or
             (state["turn_based"] and state["in_combat"])))


def verify_reconnected_movement(client, expected: dict,
                                timeout_seconds: float, *, observer_client=None) -> None:
    """Retain completed positions and verify current gameplay against live A.

    Native combat may damage or suppress actors while B reloads. Only explicit
    turn advancement plus a live observer permits those historical fields to
    change; the current replicas must still agree on every stable field.
    """
    _require_completed_evidence(expected)
    deadline = time.monotonic() + timeout_seconds
    clients = [client] if observer_client is None else [client, observer_client]
    probes = [_Probe(managed) for managed in clients]
    guard = _guard(clients)
    while time.monotonic() < deadline:
        states = _states(probes, deadline, guard)
        if _same_world(states, (expected["session"], expected["world"])):
            preserved = all(
                all(state[field] == states[0][field] for field in PHASE_FIELDS)
                for state in states[1:])
            for actor in expected["actors"]:
                current = [_projection(_actor(state, (actor["slot"], actor["inc"])))
                           for state in states]
                same_ap_phase = all(_same_combat_ap_phase(state, actor, expected)
                                    for state in states)
                if not same_ap_phase and observer_client is None:
                    raise MovementFailure(
                        "reconnect AP refresh requires comparison with a live observer client")
                advanced_combat = observer_client is not None and all(
                    _advanced_combat_phase(state, actor, expected) for state in states)
                if advanced_combat:
                    fields = POSITION_FIELDS
                else:
                    fields = STABLE_FIELDS if same_ap_phase else tuple(
                        field for field in STABLE_FIELDS if field != "ap")
                preserved = (preserved and all(value["ap"] >= 0 for value in current) and
                             all(value == current[0] for value in current[1:]) and
                             all(current[0][field] == actor[field] for field in fields))
            if preserved:
                return
        time.sleep(0.1)
    raise MovementFailure("reconnected client did not retain both completed movements")
