"""Installed secured-sector hold and shared return through queued Screen input.

After a native victory the server keeps the sector loaded for peaceful play
instead of returning to the map immediately. Either player then arms (W) and
separately confirms (Enter) the shared return. This proves queued Screen input
and replicated state only; it does not prove physical OS input or painted text.
"""
import time

from full_engine_coop_installed_process_smoke import SmokeFailure, log_text

SECURED_MARKER = "[dedicated] co-op sector secured; peaceful tactical play ("
RETURNING_MARKER = "[dedicated] secured sector returning to the strategic map"
UI_KEYS = {"request", "kind", "ready", "session", "control_revision", "groups_revision",
           "sector", "armed", "enabled", "pending"}


def _secured_marker_sector(server):
    for line in log_text(server).splitlines():
        if line.startswith(SECURED_MARKER) and line.endswith(")"):
            value = line[len(SECURED_MARKER):-1]
            if value.isdigit() and int(value) > 0:
                return int(value)
    return None


def _ui(probe, deadline, guard):
    request = probe.send("RETURN_MAP_UI")
    answer = probe.receive(request, "return_map_ui", deadline, guard)
    if answer.get("ready") is True and set(answer) != UI_KEYS:
        raise SmokeFailure("return-to-map Screen observation has an unexpected shape")
    if answer.get("ready") is not True and set(answer) != {"request", "kind", "ready"}:
        raise SmokeFailure("unready return-to-map observation must carry no stale controls")
    return answer


def _input(probe, ui, key, deadline, guard):
    request = probe.send("RETURN_MAP_INPUT", ui["session"], ui["control_revision"], ui["groups_revision"],
                         ui["sector"], int(ui["armed"]), key)
    answer = probe.receive(request, "return_map_input", deadline, guard)
    if (answer.get("queued") is not True or answer.get("key") != key or
            answer.get("queued_events") != 2):
        raise SmokeFailure("Screen driver did not queue one exact return-to-map key pair")
    return answer


def return_from_secured_sector(probes, guard, server, timeout_seconds, hold_seconds=3.0, actor_index=1):
    """Observe the held secured sector on both clients, then return through one of them."""
    deadline = time.monotonic() + timeout_seconds
    sector = None
    while time.monotonic() < deadline and sector is None:
        guard()
        sector = _secured_marker_sector(server)
        if sector is None:
            time.sleep(0.05)
    if sector is None:
        raise SmokeFailure("native victory did not secure the sector for peaceful play")
    views = None
    while time.monotonic() < deadline:
        views = [_ui(probe, deadline, guard) for probe in probes]
        if all(view["ready"] and view["sector"] == sector and view["enabled"] and not view["pending"]
               and not view["armed"] for view in views):
            break
        time.sleep(0.05)
    else:
        raise SmokeFailure("both clients did not expose the same secured sector")
    if len({(view["session"], view["control_revision"]) for view in views}) != 1:
        raise SmokeFailure("clients disagree on the secured sector's control lineage")
    # The sector must stay loaded without any input: the old behavior unloaded
    # it immediately after victory.
    held_until = time.monotonic() + hold_seconds
    samples = 0
    while time.monotonic() < held_until:
        current = [_ui(probe, deadline, guard) for probe in probes]
        if any(not view["ready"] or view["sector"] != sector for view in current):
            raise SmokeFailure("secured sector did not stay loaded for peaceful play")
        if RETURNING_MARKER in log_text(server):
            raise SmokeFailure("server returned to the map without an explicit shared request")
        samples += 1
        time.sleep(0.2)
    actor = probes[actor_index]
    armed_from = _ui(actor, deadline, guard)
    _input(actor, armed_from, 1, deadline, guard)
    while time.monotonic() < deadline:
        armed = _ui(actor, deadline, guard)
        if armed["ready"] and armed["armed"]:
            break
        time.sleep(0.05)
    else:
        raise SmokeFailure("W did not arm the return-to-map confirmation")
    if RETURNING_MARKER in log_text(server):
        raise SmokeFailure("arming alone must not return to the map")
    _input(actor, armed, 2, deadline, guard)
    while time.monotonic() < deadline:
        guard()
        if RETURNING_MARKER in log_text(server):
            break
        time.sleep(0.05)
    else:
        raise SmokeFailure("confirmed return-to-map did not reach the server")
    return {"secured_sector": sector, "views": views, "hold_samples": samples,
            "hold_seconds": hold_seconds, "returning_client_index": actor_index,
            "armed_view": armed, "physical_input_verified": False, "paint_verified": False}


def _parse_arguments(argv=None):
    import argparse
    from pathlib import Path
    root = Path(__file__).resolve().parents[1]
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--fixture-executable", type=Path, default=root / "build/full_engine_coop_installed_entry",
                        help="test-only host; seeds only enemy incapacitation after the shipping starter battle begins")
    parser.add_argument("--server-executable", type=Path,
                        default=root / "build/JA2_ENGLISH.app/Contents/MacOS/JA2_ENGLISH", help="shipping cold Resume host")
    parser.add_argument("--client-executable", type=Path, default=root / "build/full_engine_coop_installed_client")
    parser.add_argument("--installed-data-root", type=Path, default=root.parent / "JA2")
    parser.add_argument("--timeout-seconds", type=float, default=300)
    args = parser.parse_args(argv)
    if not 1 <= args.timeout_seconds <= 900:
        parser.error("timeout must be 1..900 seconds")
    return args


def main(argv=None):
    """Starter battle -> seeded native victory -> held secured sector with real movement ->
    shared Screen-input return -> post-combat checkpoint -> shipping cold Resume."""
    import json
    import tempfile
    from pathlib import Path
    from full_engine_coop_installed_campaign import paired_query, qualification_guard
    from full_engine_coop_installed_controlled_battle_return import write_trigger
    from full_engine_coop_installed_movement import _Probe, _states, certify_two_client_movement
    from full_engine_coop_installed_process_smoke import (
        READY_MARKER, TACTICAL_PRESENTATION_READY_MARKER, TACTICAL_SERVER_MARKER, ProcessSupervisor,
        client_command, free_loopback_port, installed_content_roots, make_private_directory, make_run_root,
        process_environment, selected_configuration, server_command, wait_for_marker, wait_for_markers,
    )
    args = _parse_arguments(argv)
    fixture, shipping, client_exe = (path.resolve(strict=True) for path in
                                      (args.fixture_executable, args.server_executable, args.client_executable))
    if fixture.name != "full_engine_coop_installed_entry" or shipping.name != "JA2_ENGLISH":
        raise SmokeFailure("peaceful sector requires the test-only starter host and a separate shipping Resume host")
    installed = args.installed_data_root.resolve(strict=True)
    ja2_ini, configuration = selected_configuration(installed)
    roots = installed_content_roots(installed)
    run = Path(tempfile.mkdtemp(prefix="ja2-coop-peaceful-sector-"))
    print(f"Installed peaceful-sector logs: {run}", flush=True)
    names = ("server", "client-a", "client-b", "server-resume")
    for name in names:
        make_run_root(run / name, ja2_ini, configuration, roots)
    for name in ("server-state", "client-a-state", "client-b-state"):
        make_private_directory(run / name)
    evidence = {"qualification_scope": "seeded_native_starter_victory_secured_sector", "seeded_native_fixture": True,
                "physical_input_verified": False, "paint_verified": False, "unseeded_combat_verified": False,
                "secured_sector_hold_verified": False, "peaceful_movement_verified": False,
                "shared_return_verified": False, "strategic_checkpoint_verified": False,
                "shipping_cold_resume_verified": False, "passed": False}
    supervisor = ProcessSupervisor()
    environment = process_environment()
    environment.pop("JA2_COOP_ENTRY_TEST_MODE", None)
    try:
        port = free_loopback_port()
        server = supervisor.start("server", server_command(fixture, run / "server-state", port, "new"), run / "server",
                                  dict(environment, JA2_COOP_ENTRY_TEST_MODE="starter-victory"))
        wait_for_marker(server, f"[dedicated] co-op campaign created; admission listening on 127.0.0.1:{port}", args.timeout_seconds)
        wait_for_marker(server, "[co-op controlled battle] armed; starter battle victory trigger required", args.timeout_seconds)
        clients = [supervisor.start(name, client_command(client_exe, run / f"{name}-state", port), run / name,
                                    environment, driven=True) for name in ("client-a", "client-b")]
        wait_for_markers([(clients[0], READY_MARKER), (clients[1], READY_MARKER), (server, TACTICAL_SERVER_MARKER),
                          (clients[0], TACTICAL_PRESENTATION_READY_MARKER),
                          (clients[1], TACTICAL_PRESENTATION_READY_MARKER)], args.timeout_seconds)
        guard = qualification_guard(server, clients)
        probes = [_Probe(client) for client in clients]
        deadline = time.monotonic() + args.timeout_seconds
        states = _states(probes, deadline, guard)
        evidence["trigger"] = write_trigger(run / "server", "starter-victory", states)
        wait_for_marker(server, "[co-op controlled battle] seeded outcome=starter-victory", args.timeout_seconds)
        evidence["peaceful_movement"] = certify_two_client_movement(clients, server, args.timeout_seconds)
        evidence["peaceful_movement_verified"] = True
        if RETURNING_MARKER in log_text(server):
            raise SmokeFailure("server left the secured sector during peaceful movement")
        print("Secured sector stayed loaded; both clients moved authoritatively in peaceful play.", flush=True)
        ready_before = [log_text(client).count(READY_MARKER) for client in clients]
        # The movement certifier used its own request streams; start fresh ones.
        probes = [_Probe(client) for client in clients]
        evidence["peaceful_return"] = return_from_secured_sector(probes, guard, server, args.timeout_seconds)
        evidence["secured_sector_hold_verified"] = evidence["shared_return_verified"] = True
        wait_for_marker(server, "[dedicated] post-combat strategic checkpoint committed; admission reopened", args.timeout_seconds)
        evidence["strategic_checkpoint_verified"] = True
        deadline = time.monotonic() + args.timeout_seconds
        while time.monotonic() < deadline and any(
                log_text(client).count(READY_MARKER) <= count for client, count in zip(clients, ready_before)):
            time.sleep(0.1)
        probes = [_Probe(client) for client in clients]
        campaigns = paired_query(probes, "CAMPAIGN", "campaign", time.monotonic() + args.timeout_seconds, guard)
        if any(campaign.get("phase") != 2 for campaign in campaigns):
            raise SmokeFailure("clients did not return to shared strategic play after the secured sector")
        evidence["returned_campaigns"] = campaigns
        supervisor.stop(server, expected_exit=0, timeout_seconds=120)
        resume_port = free_loopback_port()
        resumed = supervisor.start("server-resume", server_command(shipping, run / "server-state", resume_port, "resume"),
                                   run / "server-resume", environment)
        wait_for_marker(resumed, f"[dedicated] co-op campaign resumed; admission listening on 127.0.0.1:{resume_port}",
                        args.timeout_seconds)
        evidence["shipping_cold_resume_verified"] = True
        supervisor.stop(resumed, expected_exit=0, timeout_seconds=120)
        for client in clients:
            supervisor.stop(client, expected_exit=0, timeout_seconds=30)
        evidence["passed"] = True
        print("PASS: seeded starter victory held its sector for peaceful movement, then a confirmed shared return "
              "checkpointed and resumed cold on the shipping host.", flush=True)
        return 0
    except Exception as error:
        evidence["failure"] = str(error)
        print(f"Installed peaceful sector failed: {error}", flush=True)
        return 1
    finally:
        supervisor.cleanup()
        with (run / "peaceful-sector-evidence.json").open("w", encoding="utf-8") as output:
            json.dump(evidence, output, indent=2, default=str)
            output.write("\n")


if __name__ == "__main__":
    raise SystemExit(main())
