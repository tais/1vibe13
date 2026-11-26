#!/usr/bin/env python3
"""Phase 8.2 diagnostic: native save and load of a live battle.

Two clients join the untouched starter battle. At the first ordinary player
turn of turn-based combat (no pending combat action, no interrupt) the test
host digests the native battle, saves it through the native dedicated saver,
loads it back through the native loader, and reports every digest line that
differs. Diagnostic only: it qualifies no production behavior; it shows what
mid-battle persistence would lose.
"""
import argparse
import json
from pathlib import Path
import re
import tempfile
import time

from full_engine_coop_installed_process_smoke import (
    TACTICAL_PRESENTATION_READY_MARKER,
    READY_MARKER, TACTICAL_SERVER_MARKER, ProcessSupervisor, SmokeFailure, client_command, free_loopback_port,
    installed_content_roots, log_text, make_private_directory, make_run_root, process_environment,
    selected_configuration, server_command, wait_for_marker, wait_for_markers,
)

ROOT = Path(__file__).resolve().parents[1]
RESULT = re.compile(r"\[co-op entry test\] battle save diagnostic: saved=(\d) loaded=(\d)")
COUNT = re.compile(r"\[co-op entry test\] battle save diagnostic (save|load): (\d+) differing lines")
DIFF = re.compile(r"\[co-op entry test\] battle save diagnostic (save|load): - (.*) \| \+ (.*)")


def parse_arguments(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--fixture-executable", type=Path, default=ROOT / "build/full_engine_coop_installed_entry")
    parser.add_argument("--client-executable", type=Path, default=ROOT / "build/full_engine_coop_installed_client")
    parser.add_argument("--installed-data-root", type=Path, default=ROOT.parent / "JA2")
    parser.add_argument("--timeout-seconds", type=float, default=600)
    parser.add_argument("--after-halt", action="store_true",
                        help="save and load only once a player merc is halted mid-stride by a move")
    args = parser.parse_args(argv)
    if not 1 <= args.timeout_seconds <= 1800:
        parser.error("timeout must be 1..1800 seconds")
    return args


def parse_report(text):
    result = RESULT.search(text)
    return {"saved": bool(result and result.group(1) == "1"), "loaded": bool(result and result.group(2) == "1"),
            "counts": {kind: int(count) for kind, count in COUNT.findall(text)},
            "differences": [{"phase": kind, "before": a, "after": b} for kind, a, b in DIFF.findall(text)]}


def main(argv=None):
    args = parse_arguments(argv)
    fixture, client_exe = (path.resolve(strict=True) for path in (args.fixture_executable, args.client_executable))
    installed = args.installed_data_root.resolve(strict=True)
    ja2_ini, configuration = selected_configuration(installed)
    roots = installed_content_roots(installed)
    run = Path(tempfile.mkdtemp(prefix="ja2-coop-battle-save-"))
    print(f"Battle save diagnostic logs: {run}", flush=True)
    for name in ("server", "client-a", "client-b"):
        make_run_root(run / name, ja2_ini, configuration, roots)
        make_private_directory(run / f"{name}-state")
    supervisor = ProcessSupervisor()
    environment = process_environment()
    environment.pop("JA2_COOP_ENTRY_TEST_MODE", None)
    report = {}
    try:
        port = free_loopback_port()
        server = supervisor.start("server", server_command(fixture, run / "server-state", port, "new"), run / "server",
                                  dict(environment, JA2_COOP_ENTRY_TEST_MODE="starter-victory",
                                       JA2_COOP_ENTRY_BATTLE_SAVE_DIAGNOSTIC="after-halt" if args.after_halt
                                       else "1"))
        wait_for_marker(server, f"[dedicated] co-op campaign created; admission listening on 127.0.0.1:{port}",
                        args.timeout_seconds)
        clients = [supervisor.start(name, client_command(client_exe, run / f"{name}-state", port), run / name,
                                    environment, driven=True) for name in ("client-a", "client-b")]
        wait_for_markers([(clients[0], READY_MARKER), (clients[1], READY_MARKER), (server, TACTICAL_SERVER_MARKER)],
                         args.timeout_seconds)
        if args.after_halt:
            # Moves toward the enemy until one is halted mid-stride (a sighting or
            # an interrupt); the host then saves and loads and exits.
            from full_engine_coop_installed_movement import _states
            from full_engine_coop_installed_battle_resume import apply_one_move, wait_for_player_turn
            wait_for_markers([(clients[0], TACTICAL_PRESENTATION_READY_MARKER),
                              (clients[1], TACTICAL_PRESENTATION_READY_MARKER)], args.timeout_seconds)
            probes, guard, states = wait_for_player_turn(clients, server, args.timeout_seconds)
            for _ in range(6):
                if server.process.poll() is not None:
                    break
                try:
                    report["moved"] = apply_one_move(probes, guard, _states(probes, time.monotonic() + 60, guard), 60)
                except Exception:
                    break  # the host finished and closed the session
                time.sleep(5)
        deadline = time.monotonic() + args.timeout_seconds
        while time.monotonic() < deadline and server.process.poll() is None:
            time.sleep(1)
        report = dict(report, **parse_report(log_text(server)))
        print(json.dumps(report, indent=1)[:4000], flush=True)
        return 0 if report.get("saved") and report.get("loaded") and not any(report["counts"].values()) else 1
    finally:
        supervisor.cleanup()
        with (run / "battle-save-diagnostic.json").open("w", encoding="utf-8") as output:
            json.dump(report, output, indent=2, default=str)
            output.write("\n")


if __name__ == "__main__":
    raise SystemExit(main())
