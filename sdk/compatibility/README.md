# JA2Engine compatibility kit

This installed source project is the executable baseline for the experimental
JA2Engine `0.2.x` line. It checks the machine-readable manifest, imports both
public CMake targets, compiles representative Core and JA2 adapter contracts,
and runs without game data or platform libraries.

```sh
cmake -S . -B build -DJA2Engine_DIR=/path/to/sdk/lib/cmake/JA2Engine
cmake --build build --target run_ja2_engine_sdk_compatibility_probe
```

Set `JA2_ENGINE_REQUIRED_COMPATIBILITY_LINE` when checking a copied kit against
another installed SDK. A mismatch fails at configure time. This is a source
contract probe, not a promise that C++ objects can be mixed across compilers,
standard libraries, runtime-library modes, architectures, or build types.
