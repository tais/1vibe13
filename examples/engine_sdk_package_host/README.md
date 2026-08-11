# Minimal JA2Engine package host

This is a public, standalone C++17 example. It imports only
`JA2::EngineCore`, defines an application-owned package, declares a host
capability, and drives the complete transactional package lifecycle. It does
not require SDL, JA2 game data, or the repository source tree.

After installing or extracting a platform-matching SDK archive:

```sh
cmake -S . -B build -DCMAKE_PREFIX_PATH=/path/to/ja2-engine-sdk
cmake --build build --target run_ja2_engine_sdk_package_host_example
```

The package object deliberately outlives `EngineHost`; the host keeps a
non-owning reference while the package is registered. Real hosts should also
provide concrete `EngineServices` adapters for the facilities they use.
