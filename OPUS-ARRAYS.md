# IPM Arrays & Float32 — Design Proposal v3

## Scope

Phase A: `u32` + fixed-size local `u32[]` — unblocks SHA256, CRC32.
Phase B: `f32` + transcendentals — unblocks KVarN, ML algorithms, signal processing.

## Phase A — Arrays (unchanged from v2)

### 1 type + 4 instructions

```json
VariableTypeEnum += "u32"

// DECLARE: K = new u32[64]
{"instruction_type": "array_declaration", "array_name": "K", "element_type": "u32", "element_count": 64}

// READ: x = K[i]
{"instruction_type": "array_read", "array_name": "K", "index_variable": "i", "result_variable": "x"}

// WRITE: K[i] = x
{"instruction_type": "array_write", "array_name": "K", "index_variable": "i", "source_target": {"value": "x"}}

// ITERATE: for i in 0..N: body
{"instruction_type": "iterate_over_array", "array_name": "K", "item_variable": "val", "index_variable": "i", "body_instructions": [...]}
```

Arrays are not types — they're a separate entity class. `element_count` is a compile-time literal.

## Phase B — Float32 & Transcendentals

Motivation: KVarN (Sinkhorn variance normalization for KV-cache quantization)
requires `sqrt()`, `log2()`, `exp2()`. 140-line PyTorch algorithm, ~200 lines in pure C.

### 1 type + 5 instructions

```json
VariableTypeEnum += "f32"

// sqrt(x)
{"instruction_type": "f32_sqrt", "target": "std", "source": {"value": "variance"}}

// log2(x) — ln via: ln(x) = log2(x) / log2(e)
{"instruction_type": "f32_log2", "target": "log_val", "source": {"value": "x"}}

// exp2(x) — exp via: exp(x) = exp2(x * log2(e))
{"instruction_type": "f32_exp2", "target": "exp_val", "source": {"value": "x"}}

// arithmetic: + - * / min max abs
{"instruction_type": "f32_alu", "operator": "+", "target": "sum", "lhs": {"value": "sum"}, "rhs": {"value": "val"}}

// clamp val to [min, max]
{"instruction_type": "f32_clamp", "target": "val", "min": "0.001", "max": "1000.0"}
```

### What this unblocks

| Module | Instructions | Use |
|--------|------------|-----|
| KVarN Sinkhorn | f32_alu, f32_sqrt, f32_log2, f32_exp2 | KV-cache variance normalization |
| SHA256 (pure IPM) | u32 + arrays | Cryptographic hash |
| CRC32 | u32 + arrays | Checksum |
| Statistical metrics | f32_sqrt, f32_alu | mean, std, variance |
| Signal processing | f32_alu, f32_sqrt | RMS, filters |

### Design Constraints

- f32_alu operators: `+`, `-`, `*`, `/`, `min`, `max`, `abs`
- No `sin`/`cos` — not needed for KVarN, defer to Phase C
- No `pow` — can be composed from exp2 + log2
- No mixed-type operations — u32 and f32 don't mix; explicit cast if needed later
- All f32 operations use IEEE 754 float32 (hardware or soft-float on MIPS)

### Example: KVarN Sinkhorn iteration in IPM

```
f32_clamp raw_std, raw_std, 0.001, 1000.0
f32_log2 log_std, raw_std                  # log2(std)
f32_alu log_s, log_s, log_std, +           # accumulate log scale
f32_alu log_s, log_s, _LOG_S_MIN, max      # clip low
f32_alu log_s, log_s, _LOG_S_MAX, min      # clip high
f32_exp2 s, log_s                           # s = exp2(log_s)
f32_alu tile, tile, s, /                    # normalize tile
```

### Implementation Order

1. Schema: add `f32` to VariableTypeEnum, 5 instruction types to schema
2. Codegen: f32_sqrt → `sqrtf()`, f32_log2 → `log2f()`, f32_exp2 → `exp2f()`
3. Soft-float fallback: on MIPS (no FPU), use libgcc soft-float or custom implementations
4. Enforcer: no new rules needed

## Open Questions

1. Phase A and B together, or ship Phase A first? (recommend: together — `f32_array` needed for KVarN)
2. f32_array or just f32 scalars first? (recommend: f32 scalars first, arrays hold u32 only; KVarN tile can be u32 with float reinterpretation)
3. Mixed u32↔f32 cast? (recommend: `f32_from_u32` / `u32_from_f32` bitcast instruction — needed for CRC32 → f32 in KVarN workflow)
