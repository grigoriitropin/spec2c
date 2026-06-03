#!/usr/bin/env python3
"""Generate detect-hardcoded-filesystem-paths.ipm — DFA for path patterns"""
import json

# Path patterns to detect (each byte as integer)
patterns = {
    "home": [47, 104, 111, 109, 101, 47],     # /home/
    "usr":  [47, 117, 115, 114, 47],           # /usr/
    "tmp":  [47, 116, 109, 112, 47],            # /tmp/
    "etc":  [47, 101, 116, 99, 47],             # /etc/
    "var":  [47, 118, 97, 114, 47],             # /var/
    "opt":  [47, 111, 112, 116, 47],            # /opt/
    "tild": [126, 47],                           # ~/
}

instructions = []

instructions.append({
    "instruction_type": "read_file_content",
    "file_path": "path",
    "result_variable": "content"
})

instructions.append({
    "instruction_type": "variable_declaration",
    "variable_name": "found",
    "variable_type": "i32",
    "assignment_operation": "literal",
    "source_target": 0
})

# Declare state variable for each DFA state
states = {}
for name, chars in patterns.items():
    for i in range(len(chars)):
        key = f"s_{name}_{i}"
        states[key] = 0
        instructions.append({
            "instruction_type": "variable_declaration",
            "variable_name": key,
            "variable_type": "i32",
            "assignment_operation": "literal",
            "source_target": 0
        })

body = []

# For each pattern, build a state machine
for name, chars in patterns.items():
    for i, byte_val in enumerate(chars):
        curr_state = f"s_{name}_{i}"
        is_last = (i == len(chars) - 1)

        body.append({
            "instruction_type": "conditional_branch",
            "condition_type": "numeric_compare",
            "operator": "==",
            "lhs": {"value": curr_state},
            "rhs": {"value": "1"},
            "branch_on_success": [{
                "instruction_type": "conditional_branch",
                "condition_type": "numeric_compare",
                "operator": "==",
                "lhs": {"value": "byte"},
                "rhs": {"value": str(byte_val)},
                "branch_on_success": [{
                    "instruction_type": "alu_operation",
                    "operator": "+",
                    "target": "found" if is_last else f"s_{name}_{i + 1}",
                    "lhs": {"value": "0"},
                    "rhs": {"value": "1"}
                }],
                "branch_on_failure": [{
                    "instruction_type": "alu_operation",
                    "operator": "-",
                    "target": curr_state,
                    "lhs": {"value": curr_state},
                    "rhs": {"value": curr_state}
                }]
            }]
        })

    # Entry: if byte matches first char → set state to 1
    first_state = f"s_{name}_0"
    first_byte = chars[0]
    body.append({
        "instruction_type": "conditional_branch",
        "condition_type": "numeric_compare",
        "operator": "==",
        "lhs": {"value": "byte"},
        "rhs": {"value": str(first_byte)},
        "branch_on_success": [{
            "instruction_type": "alu_operation",
            "operator": "+",
            "target": first_state,
            "lhs": {"value": "0"},
            "rhs": {"value": "1"}
        }]
    })

iter_inst = {
    "instruction_type": "iterate_over_bytes",
    "collection": "content",
    "item_variable": "byte",
    "index_variable": "i",
    "body": body
}

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

return_ok = {
    "instruction_type": "return_statement",
    "return_payload": {"value": "0"}
}

instructions.append({
    "instruction_type": "conditional_branch",
    "condition_operation": "is_not_null",
    "condition_target": "content",
    "branch_on_success": [iter_inst, check_found, return_ok],
    "branch_on_failure": [return_ok]
})

spec = {
    "package_name": "detect-hardcoded-filesystem-paths",
    "build_type": "library",
    "module_name": "detect-hardcoded-filesystem-paths",
    "description": "Pure IPM hardcoded path detector via DFA",
    "extern_imports": [
        {"name": "read_entire_file_into_string", "return_type": "str",
         "arguments": [{"name": "path", "type": "str"}]}
    ],
    "function_definitions": {
        "detect-hardcoded-filesystem-paths": {
            "return_type": "i32",
            "parameter_definitions": [
                {"parameter_name": "path", "parameter_type": "str"}
            ],
            "execution_instructions": instructions
        }
    }
}

with open("modules/rules/detect-hardcoded-filesystem-paths.ipm", "w") as f:
    json.dump(spec, f, separators=(",", ":"), indent=None)
    f.write("\n")

total_states = sum(len(c) for c in patterns.values())
total_patterns = len(patterns)
print(f"OK: {len(instructions)} top-level, {len(body)} loop body, {total_states} DFA states, {total_patterns} patterns")
