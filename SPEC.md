# The IPM language — specification

IPM is the input language of [`spec2c`](README.md): a declarative, JSON-based
abstract syntax tree (AST) that describes a C code generator. It has no surface
syntax and no parser of its own — an `.ipm` file *is* JSON — and it is never
interpreted: the AST is compiled to C source, which is then compiled to a binary.

## The contract

- **Closed, default-deny schema.** The set of instruction kinds is frozen. The loader
  validates every node against the schema and **rejects** any unknown instruction type
  or unknown key — "valid JSON" is not enough, the closed schema is the load-bearing
  half. Adding a new instruction kind is an operator-signed event (see §9 of
  [`SOUL.md`](SOUL.md)), not a casual edit.
- **Compile, don't interpret.** AST → C source → binary. There is zero runtime
  interpreter to attack or to drift.
- **LLM-first.** Verbose, unambiguous, exhaustive enums; errors are structured JSON
  with the function name, instruction index, and a fix hint, so a model (or a human)
  always learns *why* a build failed.
- **Small frozen core.** The eight instruction kinds below are the current frozen set.
  The language grows only by proof-of-necessity and under an operator signature, never
  by accretion.

## Instruction kinds

### `access_json_field`
Read a named field from a JSON object into a typed variable.
```json
{
  "instruction_type": "access_json_field",
  "variable_name": "pkg_name",
  "variable_type": "string",
  "source_object": "parsed_spec",
  "field_name": "package_name"
}
```

### `function_invocation`
Call a built-in or AST-defined function with named arguments.
```json
{
  "instruction_type": "function_invocation",
  "invocation_name": "hash_table_insert",
  "invocation_arguments": {
    "table_handle": "subst_table",
    "entry_key": "\"package_name\"",
    "entry_value": "pkg_name"
  },
  "result_assignment_variable": null,
  "result_type": "void"
}
```

### `conditional_branch`
Branch on a condition. Operations: `key_exists`, `is_not_null`, `string_equals`.
```json
{
  "instruction_type": "conditional_branch",
  "condition_operation": "string_equals",
  "condition_target": "inst_type",
  "condition_value": "variable_declaration",
  "branch_on_success": [],
  "branch_on_failure": []
}
```

### `return_statement`
Return from a function with an optional value.
```json
{
  "instruction_type": "return_statement",
  "return_payload": { "execution_status": "success", "value": "generation_status" }
}
```

### `iterate_over_collection`
For-loop over a JSON array.
```json
{
  "instruction_type": "iterate_over_collection",
  "collection_variable": "params",
  "item_variable": "par",
  "body_instructions": []
}
```

### `iterate_over_object_keys`
Foreach over the keys and values of a JSON object.
```json
{
  "instruction_type": "iterate_over_object_keys",
  "collection_variable": "templates",
  "key_variable": "template_name",
  "value_variable": "template_entry",
  "body_instructions": []
}
```

### `variable_declaration`
Declare a typed variable with a function-call initialization.
```json
{
  "instruction_type": "variable_declaration",
  "variable_name": "count",
  "variable_type": "int",
  "assignment_operation": "atoi",
  "source_target": "count_text"
}
```

### `database_execute_parameterized`
Reserved stub for future database integration; currently generates a comment only.

## Type system

The current frozen surface types map to C as follows:

| IPM type | C type |
|----------|--------|
| `void` | `void` |
| `string` | `char *` |
| `int` | `int` |
| `json_object` | `cJSON *` |
| `json_array` | `cJSON *` |
| `subst_table` | substitution-table handle |
| `string_buffer` | append-only buffer handle |

**Roadmap note (research-honest).** This is the *current* type surface. The roadmap
(Phase 1, "Strict Type Guard") replaces the loose `int` with strict, fixed-width
`u8/u32/u64` plus a bitwise/modular ALU, as a prerequisite for sovereign memory
(arenas and bounds-checked slices) in Phase 2. A minimal `u32` + fixed-size array
extension is the next planned, operator-signed node-kind addition (needed for pure-IPM
hashing such as SHA-256/CRC-32). The tables above will change when those land; they
describe what exists today, not the destination.

## Built-in functions

A small set of runtime primitives (file I/O, JSON parsing, a substitution hash table,
and an append-only string buffer) is available to AST functions. These primitives are
part of the **frozen runtime** under the project's support-code modules; they contain
no compiler logic — all code-generation logic lives in the IPM AST. They are
themselves subject to the same default-deny capability and freeze rules as everything
else.

## License & authorship

Apache-2.0. Authored by Vehir (autonomous agent) under [`SOUL.md`](SOUL.md);
delegated-operator legal layer, see §11.
