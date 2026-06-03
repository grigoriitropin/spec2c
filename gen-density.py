#!/usr/bin/env python3
"""Generate check-each-line-token-density.ipm — pure IPM density checker"""
import json

instructions = []

# Read file content
instructions.append({
    "instruction_type": "read_file_content",
    "file_path": "path",
    "result_variable": "content"
})

# Declare persistent state (outside loop)
instructions.append({
    "instruction_type": "variable_declaration",
    "variable_name": "tokens",
    "variable_type": "i32",
    "assignment_operation": "literal",
    "source_target": 0
})
instructions.append({
    "instruction_type": "variable_declaration",
    "variable_name": "found",
    "variable_type": "i32",
    "assignment_operation": "literal",
    "source_target": 0
})

# Build loop body
body = []

# If byte == newline (10): tokens = 0
body.append({
    "instruction_type": "conditional_branch",
    "condition_type": "numeric_compare",
    "operator": "==",
    "lhs": {"value": "byte"},
    "rhs": {"value": "10"},
    "branch_on_success": [{
        "instruction_type": "alu_operation",
        "operator": "-",
        "target": "tokens",
        "lhs": {"value": "tokens"},
        "rhs": {"value": "tokens"}
    }]
})

# If byte in [';', '{', '?']: tokens++
for ch_code in [59, 123, 63]:
    body.append({
        "instruction_type": "conditional_branch",
        "condition_type": "numeric_compare",
        "operator": "==",
        "lhs": {"value": "byte"},
        "rhs": {"value": str(ch_code)},
        "branch_on_success": [{
            "instruction_type": "alu_operation",
            "operator": "+",
            "target": "tokens",
            "lhs": {"value": "tokens"},
            "rhs": {"value": "1"}
        }]
    })

# If tokens > 3: found = 1
body.append({
    "instruction_type": "conditional_branch",
    "condition_type": "numeric_compare",
    "operator": ">",
    "lhs": {"value": "tokens"},
    "rhs": {"value": "3"},
    "branch_on_success": [{
        "instruction_type": "alu_operation",
        "operator": "+",
        "target": "found",
        "lhs": {"value": "0"},
        "rhs": {"value": "1"}
    }]
})

# The iterate_over_bytes instruction
iter_inst = {
    "instruction_type": "iterate_over_bytes",
    "collection": "content",
    "item_variable": "byte",
    "index_variable": "i",
    "body": body
}

# Check after loop: if found != 0 → return 1
check_found = {
    "instruction_type": "conditional_branch",
    "condition_type": "numeric_compare",
    "operator": "!=",
    "lhs": {"value": "found"},
    "rhs": {"value": "0"},
    "branch_on_success": [{
        "instruction_type": "return_statement",
        "return_payload": {"value": "1"}
    }]
}

# Return 0 (no error)
return_ok = {
    "instruction_type": "return_statement",
    "return_payload": {"value": "0"}
}

# Wrap in NULL check
instructions.append({
    "instruction_type": "conditional_branch",
    "condition_operation": "is_not_null",
    "condition_target": "content",
    "branch_on_success": [iter_inst, check_found, return_ok],
    "branch_on_failure": [return_ok]
})

spec = {
    "package_name": "check-each-line-token-density",
    "build_type": "library",
    "module_name": "check-each-line-token-density",
    "description": "Pure IPM line density checker",
    "extern_imports": [
        {"name": "read_entire_file_into_string", "return_type": "str", "arguments": [{"name": "path", "type": "str"}]}
    ],
    "function_definitions": {
        "check-each-line-token-density": {
            "return_type": "i32",
            "parameter_definitions": [
                {"parameter_name": "path", "parameter_type": "str"}
            ],
            "execution_instructions": instructions
        }
    }
}

with open("modules/rules/check-each-line-token-density.ipm", "w") as f:
    json.dump(spec, f, separators=(",", ":"), indent=None)
    f.write("\n")

print(f"OK: {len(instructions)} top-level, {len(body)} loop body")
