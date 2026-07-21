# Runtime diagnostic reports

The game can write a bounded JSON snapshot of the new engine runtime for bug
reports, launcher integration, and mod/package diagnosis. Reporting is opt-in;
an installation with no report setting performs no serialization or file I/O.

Add a path to `Ja2.ini`:

```ini
[Ja2 Settings]
ENGINE_REPORT_PATH = engine-runtime-report.json
ENGINE_REPORT_ON_STARTUP = true
ENGINE_REPORT_ON_SHUTDOWN = true
```

The equivalent command-line option is:

```text
JA2_ENGLISH.exe --engine-report=engine-runtime-report.json
```

`--engine-report path.json` is also accepted. `--no-engine-report` explicitly
disables an INI setting for that launch. A report path supplied on the command
line overrides the INI path. Paths are written through the game's existing
writable VFS profile; use a simple filename unless the chosen subdirectory
already exists.

With both phase switches enabled, startup writes a baseline after all package
bootstrap phases complete and the engine enters its running state. A graceful
shutdown overwrites the same path immediately before package teardown, leaving
the more useful final counters and retained faults. Disable either phase in the
INI when only one capture point is wanted. A report failure is logged but never
blocks startup or shutdown.

The schema includes:

- runtime lifecycle, completed frame/tick boundaries, and compatibility
  fingerprint;
- package identities, versions, activation order, declared content counts,
  dependencies, capabilities, callback health, and owned resource totals;
- registered engine services and sealed configuration values;
- aggregate frame/cache statistics and the bounded runtime fault journal; and
- a top-level health result for automation.

The report deliberately excludes translation text, definition payload bytes,
entity records, audio asset paths, and individual frame samples. Output is
valid UTF-8 JSON, deterministic for the same snapshot, transactionally written,
and limited to 4 MiB by default.
