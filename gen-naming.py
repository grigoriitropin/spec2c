#!/usr/bin/env python3
"""Generate check-naming-rules-for-soul.ipm — pure IPM naming validator"""
import json

# Banned type words from SOUL §10
BANNED = [
    "service", "server", "daemon", "library", "tool",
    "binary", "package", "module", "system", "utility",
    "application", "program", "process", "worker"
]
# Whitelisted names (always pass)
WHITELIST = ["main", "while", "for", "if", "switch"]

instructions = []

# Vars: found (violation flag), count (word counter)
instructions.append({"instruction_type": "variable_declaration", "variable_name": "found", "variable_type": "i32", "assignment_operation": "literal", "source_target": 0})
instructions.append({"instruction_type": "variable_declaration", "variable_name": "count", "variable_type": "i32", "assignment_operation": "literal", "source_target": 0})

# Pre-checks: whitelist
for wl in WHITELIST:
    instructions.append({
        "instruction_type": "conditional_branch",
        "condition_operation": "equals", "condition_target": "name_arg", "condition_value": wl,
        "branch_on_success": [{"instruction_type": "return_statement", "return_payload": {"value": "0"}}]
    })

# Build token body
body = []

# Check token length < 3
body.append({
    "instruction_type": "conditional_branch",
    "condition_type": "numeric_compare",
    "operator": "<", "lhs": {"value": "toklen"}, "rhs": {"value": "3"},
    "branch_on_success": [{
        "instruction_type": "alu_operation",
        "operator": "+", "target": "found",
        "lhs": {"value": "0"}, "rhs": {"value": "1"}
    }]
})

# Check against each banned word
for bw in BANNED:
    body.append({
        "instruction_type": "conditional_branch",
        "condition_operation": "equals", "condition_target": "token", "condition_value": bw,
        "branch_on_success": [{
            "instruction_type": "alu_operation",
            "operator": "+", "target": "found",
            "lhs": {"value": "0"}, "rhs": {"value": "1"}
        }]
    })

# Increment count
body.append({
    "instruction_type": "alu_operation",
    "operator": "+", "target": "count",
    "lhs": {"value": "count"}, "rhs": {"value": "1"}
})

# Main loop
instructions.append({
    "instruction_type": "iterate_over_string_tokens",
    "source_string": "name_arg",
    "separator": "-",
    "token_variable": "token",
    "length_variable": "toklen",
    "body": body
})

# After loop: if count != 5 → found = 1
instructions.append({
    "instruction_type": "conditional_branch",
    "condition_type": "numeric_compare",
    "operator": "!=", "lhs": {"value": "count"}, "rhs": {"value": "5"},
    "branch_on_success": [{
        "instruction_type": "alu_operation",
        "operator": "+", "target": "found",
        "lhs": {"value": "0"}, "rhs": {"value": "1"}
    }]
})

# Check found and return
instructions.append({
    "instruction_type": "conditional_branch",
    "condition_type": "numeric_compare",
    "operator": "!=", "lhs": {"value": "found"}, "rhs": {"value": "0"},
    "branch_on_success": [{"instruction_type": "return_statement", "return_payload": {"value": "1"}}]
})
instructions.append({"instruction_type": "return_statement", "return_payload": {"value": "0"}})

spec = {
    "package_name": "check-naming-rules-for-any-symbol",
    "build_type": "library",
    "module_name": "check-naming-rules-for-any-symbol",
    "description": "Pure IPM naming checker — validates 5 words, min 3 chars, no banned words",
    "function_definitions": {
        "check-naming-rules-for-any-symbol": {
            "return_type": "i32",
            "parameter_definitions": [
                {"parameter_name": "name_arg", "parameter_type": "str"}
            ],
            "execution_instructions": instructions
        }
    }
}

with open("modules/rules/check-naming-rules-for-any-symbol.ipm", "w") as f:
    json.dump(spec, f, separators=(",", ":"), indent=None)
    f.write("\n")

print(f"OK: {len(instructions)} top-level, {len(body)} body, {len(BANNED)} banned words")
