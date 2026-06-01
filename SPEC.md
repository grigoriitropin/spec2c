# .ipm Language Specification

Declarative, JSON-based language for describing C code generators.
8 instruction types. Flat dispatch. All logic in `string_substitute` + `append_to_buffer`.

## Design Principles

- **LLM-first**: Verbose, unambiguous, exhaustive enums. JSON-structured errors.
- **No parser**: `.ipm` is valid JSON. cJSON handles parsing.
- **Compile-to-C, don't interpret**: AST → C source → GCC binary. Zero runtime interpreter.
- **Core language frozen**: 8 types. Expand only through hardening sessions.

## Instruction Types

### access_json_field
Read a named field from a cJSON object into a typed variable.
```json
{
  "instruction_type": "access_json_field",
  "variable_name": "pkg_name",
  "variable_type": "string",
  "source_object": "parsed_spec",
  "field_name": "package_name"
}
```

### function_invocation
Call a builtin or AST-defined function with named arguments.
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

### conditional_branch
Branch on condition. Three operations: `key_exists`, `is_not_null`, `string_equals`.
```json
{
  "instruction_type": "conditional_branch",
  "condition_operation": "string_equals",
  "condition_target": "inst_type",
  "condition_value": "variable_declaration",
  "branch_on_success": [...],
  "branch_on_failure": [...]
}
```

### return_statement
Return from function with optional value.
```json
{
  "instruction_type": "return_statement",
  "return_payload": {
    "execution_status": "success",
    "value": "generation_status"
  }
}
```

### iterate_over_collection
For-loop over a JSON array.
```json
{
  "instruction_type": "iterate_over_collection",
  "collection_variable": "params",
  "item_variable": "par",
  "body_instructions": [...]
}
```

### iterate_over_object_keys
Foreach over JSON object keys and values.
```json
{
  "instruction_type": "iterate_over_object_keys",
  "collection_variable": "templates",
  "key_variable": "template_name",
  "value_variable": "template_entry",
  "body_instructions": [...]
}
```

### variable_declaration
Declare a typed variable with function-call initialization.
```json
{
  "instruction_type": "variable_declaration",
  "variable_name": "x",
  "variable_type": "int",
  "assignment_operation": "atoi",
  "source_target": "some_var"
}
```

### database_execute_parameterized
Stub. Reserved for future DB integration. Generates comment only.

## Built-in Functions

Available to all AST functions (defined in `src/ipm_builtins.c`):

| Function | Purpose |
|----------|---------|
| `read_file_to_string` | Read file to heap string |
| `write_string_to_file` | Write string to file |
| `parse_json_string` | Parse JSON string to cJSON object |
| `create_hash_table` | Create substitution table |
| `hash_table_insert` | Insert key-value pair |
| `hash_table_lookup` | Lookup value by key |
| `string_substitute` | Replace `{{KEY}}` patterns in template |
| `create_string_buffer` | Create append-only string buffer |
| `append_to_buffer` | Append string to buffer |
| `write_buffer_to_file` | Flush buffer to file |
| `free_string_buffer` | Free buffer |
| `vartype_to_c` | Map IPM type to C type |
| `json_die` | Structured JSON error + exit |
| `compile_body` | Compile array of instructions (AST function) |

## Type System

| IPM Type | C Type |
|----------|--------|
| `void` | `void` |
| `string` | `char *` |
| `int` | `int` |
| `json_object` | `cJSON *` |
| `json_array` | `cJSON *` |
| `subst_table` | `subst_table *` |
| `string_buffer` | `string_buffer *` |

## License

Apache-2.0. Authored by Vehir (autonomous agent).
