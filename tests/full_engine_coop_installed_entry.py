#!/usr/bin/env python3
"""Opt-in installed native battle-action fixtures, not a two-client playtest."""
import argparse
from pathlib import Path
import subprocess
import tempfile

from full_engine_coop_installed_process_smoke import (
    ProcessSupervisor, SmokeFailure, fingerprint_installed_inputs,
    free_loopback_port, selected_configuration, installed_content_roots,
    log_text, make_private_directory, make_run_root, process_environment,
    server_command,
)

SCENARIOS = ("spread", "forced", "ambush-spread", "failed-load", "retreat")


def parse_arguments(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--executable", type=Path, required=True)
    parser.add_argument("--installed-data-root", type=Path, required=True)
    parser.add_argument("--timeout-seconds", type=float, default=180)
    parser.add_argument("--scenario", choices=SCENARIOS,
                        help="run one native scenario (default: all scenarios)")
    arguments = parser.parse_args(argv)
    if not 1 <= arguments.timeout_seconds <= 600:
        parser.error("timeout must be between 1 and 600 seconds")
    return arguments


def main() -> int:
    arguments = parse_arguments()
    executable = arguments.executable.resolve(strict=True)
    installed_root = arguments.installed_data_root.resolve(strict=True)
    ja2_ini, configuration = selected_configuration(installed_root)
    roots = installed_content_roots(installed_root)
    before = fingerprint_installed_inputs([ja2_ini, configuration], roots)
    run_root = Path(tempfile.mkdtemp(prefix="ja2-coop-native-entry-"))
    print(f"Installed native entry logs: {run_root}", flush=True)
    supervisor = ProcessSupervisor()
    try:
        for mode in (arguments.scenario,) if arguments.scenario else SCENARIOS:
            work = run_root / mode
            state = run_root / f"{mode}-state"
            make_run_root(work, ja2_ini, configuration, roots)
            make_private_directory(state)
            environment = process_environment()
            environment["JA2_COOP_ENTRY_TEST_MODE"] = mode
            process = supervisor.start(mode, server_command(
                executable, state, free_loopback_port(), "new"), work, environment)
            try:
                code = process.process.wait(timeout=arguments.timeout_seconds)
            except subprocess.TimeoutExpired as error:
                raise SmokeFailure(f"{mode}: fixture timed out; see {process.log_path}") from error
            text = log_text(process)
            # A successful fixture deliberately stops in combat. The normal
            # shutdown refuses an ineligible tactical checkpoint (exit 2).
            expected_code = 0 if mode == "retreat" else 2
            if (code != expected_code or "[co-op entry test] PASS:" not in text or
                    "[co-op entry test] FAIL:" in text or
                    (mode != "retreat" and "[dedicated] final co-op checkpoint failed: checkpoint not eligible" not in text) or
                    "[SGPGameLoop]" in text or "[CallGameLoop]" in text or
                    "[dedicated] native dialog requires handling:" in text or
                    "[dedicated] co-op runtime failed:" in text):
                raise SmokeFailure(f"{mode}: native entry failed (exit {code}); see {process.log_path}")
            print(f"Installed native entry passed: {mode}", flush=True)
    finally:
        supervisor.cleanup()
        after = fingerprint_installed_inputs([ja2_ini, configuration], roots)
        if before != after:
            raise SmokeFailure("installed content changed during entry fixtures")
    print("Selected native battle actions verified; seeded fixtures, no travel/peer/physical-input acceptance.", flush=True)
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (SmokeFailure, OSError) as error:
        print(f"Installed native entry failed: {error}", flush=True)
        raise SystemExit(1)
