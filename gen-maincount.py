#!/usr/bin/env python3
"""Generate find-every-main-function-block.ipm — DFA for main() (single-state version)"""
import json

instructions = []

instructions.append({
    "instruction_type": "read_file_content",
    "file_path": "path",
    "result_variable": "content"
})

instructions.append({
    "instruction_type": "variable_declaration",
    "variable_name": "found", "variable_type": "i32",
    "assignment_operation": "literal", "source_target": 0
})
instructions.append({
    "instruction_type": "variable_declaration",
    "variable_name": "ms", "variable_type": "i32",
    "assignment_operation": "literal", "source_target": 0
})

body = []

# State machine: ms values: 0=idle, 1=saw'm', 2=saw'ma', 3=saw'mai', 4=saw'main', 5=saw'main(space)'
# On match: found=1, ms=0 (reset)
# byte codes: m=109, a=97, i=105, n=110, (=40, space=32

transitions = [
    (0, 109, 1),   # idle → saw 'm'
    (1, 97,  2),   # 'm' → saw 'ma', expecting 'a'
    (2, 105, 3),   # 'ma' → saw 'mai', expecting 'i'
    (3, 110, 4),   # 'mai' → saw 'main', expecting 'n'
]

for state, byte_val, next_state in transitions:
    body.append({
        "instruction_type": "conditional_branch",
        "condition_type": "numeric_compare",
        "operator": "==", "lhs": {"value": "ms"}, "rhs": {"value": str(state)},
        "branch_on_success": [{
            "instruction_type": "conditional_branch",
            "condition_type": "numeric_compare",
            "operator": "==", "lhs": {"value": "byte"}, "rhs": {"value": str(byte_val)},
            "branch_on_success": [{
                "instruction_type": "alu_operation",
                "operator": "+", "target": "ms",
                "lhs": {"value": "0"}, "rhs": {"value": str(next_state)}
            }]
        }]
    })

# State 4 (saw 'main'): looking for '(' or ' '
body.append({
    "instruction_type": "conditional_branch",
    "condition_type": "numeric_compare",
    "operator": "==", "lhs": {"value": "ms"}, "rhs": {"value": "4"},
    "branch_on_success": [
        # If byte == '(' → match!
        {"instruction_type": "conditional_branch",
         "condition_type": "numeric_compare",
         "operator": "==", "lhs": {"value": "byte"}, "rhs": {"value": "40"},
         "branch_on_success": [
             {"instruction_type": "alu_operation", "operator": "+", "target": "found", "lhs": {"value": "0"}, "rhs": {"value": "1"}},
             {"instruction_type": "alu_operation", "operator": "-", "target": "ms", "lhs": {"value": "ms"}, "rhs": {"value": "ms"}}
         ],
         "branch_on_failure": [
             # If byte == ' ' → stay
             {"instruction_type": "conditional_branch",
              "condition_type": "numeric_compare",
              "operator": "==", "lhs": {"value": "byte"}, "rhs": {"value": "32"},
              "branch_on_failure": [
                  # Any other char → reset
                  {"instruction_type": "alu_operation", "operator": "-", "target": "ms", "lhs": {"value": "ms"}, "rhs": {"value": "ms"}}
              ]}
         ]}
    ]
})

iter_inst = {
    "instruction_type": "iterate_over_bytes",
    "collection": "content",
    "item_variable": "byte", "index_variable": "i",
    "body": body
}

check_found = {
    "instruction_type": "conditional_branch",
    "condition_type": "numeric_compare",
    "operator": "!=", "lhs": {"value": "found"}, "rhs": {"value": "0"},
    "branch_on_success": [{"instruction_type": "return_statement", "return_payload": {"value": "1"}}]
}
return_ok = {"instruction_type": "return_statement", "return_payload": {"value": "0"}}

instructions.append({
    "instruction_type": "conditional_branch",
    "condition_operation": "is_not_null",
    "condition_target": "content",
    "branch_on_success": [iter_inst, check_found, return_ok],
    "branch_on_failure": [return_ok]
})

spec = {
    "package_name": "find-every-main-function-block",
    "build_type": "library",
    "module_name": "find-every-main-function-block",
    "description": "DFA scanner for int main( pattern",
    "extern_imports": [
        {"name": "read_entire_file_into_string", "return_type": "str",
         "arguments": [{"name": "path", "type": "str"}]}
    ],
    "function_definitions": {
        "find-every-main-function-block": {
            "return_type": "i32",
            "parameter_definitions": [{"parameter_name": "path", "parameter_type": "str"}],
            "execution_instructions": instructions
        }
    }
}

with open("modules/rules/find-every-main-function-block.ipm", "w") as f:
    json.dump(spec, f, separators=(",", ":"), indent=None)
    f.write("\n")
print(f"OK: {len(instructions)} top-level, {len(body)} body")
