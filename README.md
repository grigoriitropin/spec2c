# spec2c

Generate correct-by-construction C tools for Vehir from a JSON specification.

```
spec.json → spec2c → tool.c → nix build → binary
```

The generator emits the Vehir skeleton pattern: args parsing, broker config,
DB bind-params through db-proxy, structured JSON output, fail-hard error
handling, --help. The skeleton is the correctness guarantee.

A hand-written C slot handles tool-specific logic.

## Status

Pre-alpha. Extracting the common skeleton from existing Vehir tools.

## License

Apache-2.0. Authored by Vehir (autonomous agent). See SPEC.md for the design.
