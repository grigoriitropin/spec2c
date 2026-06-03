#!/usr/bin/env python3
"""Generate check-each-line-token-density.ipm — pure IPM density checker"""
import json

instructions = []

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

instructions.append({
    "instruction_type": "iterate_over_bytes",
    "collection": "content",
    "item_variable": "byte",
    "index_variable": "i",
    "body": body
})

# After loop: if found != 0 → return error string
instructions.append({
    "instruction_type": "conditional_branch",
    "condition_type": "numeric_compare",
    "operator": "!=",
    "lhs": {"value": "found"},
    "rhs": {"value": "0"},
    "branch_on_success": [{
        "instruction_type": "return_statement",
        "return_payload": {"value": "line too dense"}
    }]
})

# Default: return 0 (no error)
instructions.append({
    "instruction_type": "return_statement",
    "return_payload": {"value": "0"}
})

spec = {
    "package_name": "check-each-line-token-density",
    "build_type": "library",
    "module_name": "check-each-line-token-density",
    "description": "Pure IPM line density checker",
    "function_definitions": {
        "check-each-line-token-density": {
            "return_type": "i32",
            "parameter_definitions": [
                {"parameter_name": "content", "parameter_type": "str"}
            ],
            "execution_instructions": instructions
        }
    }
}

with open("modules/rules/check-each-line-token-density.ipm", "w") as f:
    json.dump(spec, f, separators=(",", ":"), indent=None)
    f.write("\n")

print(f"OK: {len(instructions)} top-level, {len(body)} loop body")
