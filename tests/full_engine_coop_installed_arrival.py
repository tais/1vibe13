#!/usr/bin/env python3
"""Opt-in two-client observation of a seeded native pending battle, not travel/control acceptance."""
import argparse
import json
from pathlib import Path
import tempfile
import time

from full_engine_coop_installed_campaign_status import wait_shared, verify_cleared
from full_engine_coop_installed_movement import MovementFailure, _Probe, _guard
from full_engine_coop_installed_process_smoke import (
    ProcessSupervisor, SmokeFailure, client_command, fingerprint_installed_inputs,
    free_loopback_port, installed_content_roots, log_text, make_private_directory,
    make_run_root, process_environment, require_reconnecting, selected_configuration,
    server_command, wait_for_marker, READY_MARKER,
)


def parse_arguments(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--server-executable", type=Path, required=True,
                        help="Opt-in full_engine_coop_installed_entry fixture, never the shipping app")
    parser.add_argument("--client-executable", type=Path, required=True)
    parser.add_argument("--installed-data-root", type=Path, required=True)
    parser.add_argument("--timeout-seconds", type=float, default=180)
    args = parser.parse_args(argv)
    if not 1 <= args.timeout_seconds <= 600:
        parser.error("timeout must be between 1 and 600 seconds")
    return args


def wait_for_capture(client, response, session, decision, timeout):
    if (set(response) != {"request", "kind", "queued", "session", "decision", "screenshot"} or
            response["kind"] != "campaign_capture" or response["queued"] is not True or
            type(response["request"]) is not int or response["request"] <= 0 or
            type(response["session"]) is not int or response["session"] != session or
            type(response["decision"]) is not int or response["decision"] != decision or
            response["screenshot"] != "Screenshots/SCREEN00000.png"):
        raise SmokeFailure("arrival screenshot was not scoped to the exact current decision")
    image = client.log_path.parent / response["screenshot"]
    # Existence precedes completion: wait until the native writer finishes PNG encoding.
    wait_for_marker(client, "[video] Saved screenshot: Screenshots/SCREEN00000.png", timeout)
    if not image.is_file() or image.is_symlink() or image.parent.is_symlink() or not image.stat().st_size:
        raise SmokeFailure("native arrival screenshot missing or redirected")
    return str(image)


def main():
    args = parse_arguments()
    server_exe = args.server_executable.resolve(strict=True)
    client_exe = args.client_executable.resolve(strict=True)
    installed = args.installed_data_root.resolve(strict=True)
    ja2_ini, configuration = selected_configuration(installed)
    roots = installed_content_roots(installed)
    before = fingerprint_installed_inputs([ja2_ini, configuration], roots)
    run = Path(tempfile.mkdtemp(prefix="ja2-coop-arrival-observation-"))
    print(f"Installed arrival observation logs: {run}", flush=True)
    supervisor = ProcessSupervisor()
    try:
        for name in ("server", "client-a", "client-b"):
            make_run_root(run / name, ja2_ini, configuration, roots)
            make_private_directory(run / f"{name}-state")
        port = free_loopback_port()
        environment = process_environment()
        server_environment = dict(environment, JA2_COOP_ENTRY_TEST_MODE="observe")
        server = supervisor.start("server", server_command(server_exe, run / "server-state", port, "new"),
                                  run / "server", server_environment)
        wait_for_marker(server, "[co-op entry test] observing pending battle id=1", args.timeout_seconds)
        wait_for_marker(server, "[co-op entry test] pending native context stable", args.timeout_seconds)
        clients = [supervisor.start(name, client_command(client_exe, run / f"{name}-state", port),
                                   run / name, environment, driven=True) for name in ("client-a", "client-b")]
        for client in clients:
            wait_for_marker(client, READY_MARKER, args.timeout_seconds)
        shared = wait_shared(clients, server, args.timeout_seconds)
        arrival = shared[0]["arrival"]
        if (arrival["decision"] != 1 or arrival["kind"] != 3 or arrival["stage"] != 2 or
                (arrival["x"], arrival["y"], arrival["z"]) != (10, 1, 0) or arrival["involved"] != 2 or
                arrival["pending"] != 1 or not arrival["native_enter"] or not arrival["native_placement"]):
            raise SmokeFailure("clients did not observe the exact seeded native battle preparation")
        images = []
        for client in clients:
            probe = _Probe(client)
            request = probe.send("CAMPAIGN_CAPTURE", shared[0]["session"], arrival["decision"])
            deadline = time.monotonic() + args.timeout_seconds
            response = probe.receive(request, "campaign_capture", deadline, _guard([server, *clients]))
            images.append(wait_for_capture(client, response, shared[0]["session"], arrival["decision"],
                                           max(1, deadline - time.monotonic())))
        print("Both clients received the same native pending battle and captured their actual waiting screens.", flush=True)
        supervisor.stop(clients[1], 0, 30)
        clients[1] = supervisor.start("client-b-reconnected", client_command(client_exe, run / "client-b-state", port),
                                      run / "client-b", environment, driven=True)
        wait_for_marker(clients[1], READY_MARKER, args.timeout_seconds)
        reconnected = wait_shared(clients, server, args.timeout_seconds, previous=shared)
        if reconnected[0]["arrival"] != arrival or reconnected[0]["world_seconds"] != shared[0]["world_seconds"]:
            raise SmokeFailure("reconnect changed/rerolled the native decision or advanced its held clock")
        if "[dedicated] native dialog requires handling:" in log_text(server):
            raise SmokeFailure("observation fixture unexpectedly queued a native GUI continuation")
        print("Reconnect received the same decision; the native clock stayed held.", flush=True)
        before_stop = {client: log_text(client) for client in clients}
        supervisor.stop(server, 2, 30)  # A pending native decision cannot be cold-checkpointed.
        require_reconnecting(tuple(clients), before_stop, 30)
        cleared = verify_cleared(clients, 10)
        for client in clients:
            supervisor.stop(client, 0, 30)
        with (run / "arrival-evidence.json").open("x", encoding="utf-8") as output:
            json.dump({"shared": shared, "reconnected": reconnected, "disconnected": cleared,
                       "screenshots": images, "seeded_native_fixture": True, "travel_verified": False,
                       "battle_controls_verified": False, "physical_input_verified": False,
                       "paint_verified": False}, output, indent=2, allow_nan=False)
        print("PASS: installed native arrival observation, reconnect and disconnect clearing; screenshots require visual review.", flush=True)
    finally:
        supervisor.cleanup()
        if fingerprint_installed_inputs([ja2_ini, configuration], roots) != before:
            raise SmokeFailure("installed inputs changed during arrival observation fixture")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (SmokeFailure, MovementFailure, OSError) as error:
        print(f"Installed arrival observation failed: {error}", flush=True)
        raise SystemExit(1)
