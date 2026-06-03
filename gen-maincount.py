#!/usr/bin/env python3
"""Generate detect-main-function-in-source.ipm — DFA for main() detection"""
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

# DFA states for "main("  with optional space: m(109)→a(97)→i(105)→n(110)→[(40) or (32)→(40)]
for s in range(5):
    instructions.append({
        "instruction_type": "variable_declaration",
        "variable_name": f"ms_{s}", "variable_type": "i32",
        "assignment_operation": "literal", "source_target": 0
    })

body = []

# Pattern bytes: m=109, a=97, i=105, n=110, ( =40, space=32
patterns = [109, 97, 105, 110, 40]  # m, a, i, n, (
for idx, bv in enumerate(patterns[:-1]):  # first 4: m, a, i, n
    body.append({
        "instruction_type": "conditional_branch",
        "condition_type": "numeric_compare",
        "operator": "==", "lhs": {"value": f"ms_{idx}"}, "rhs": {"value": "1"},
        "branch_on_success": [{
            "instruction_type": "conditional_branch",
            "condition_type": "numeric_compare",
            "operator": "==", "lhs": {"value": "byte"}, "rhs": {"value": str(patterns[idx + 1])},
            "branch_on_success": [{
                "instruction_type": "alu_operation",
                "operator": "+", "target": f"ms_{idx + 1}",
                "lhs": {"value": "0"}, "rhs": {"value": "1"}
            }],
            "branch_on_failure": [{
                "instruction_type": "alu_operation",
                "operator": "-", "target": f"ms_{idx}",
                "lhs": {"value": f"ms_{idx}"}, "rhs": {"value": f"ms_{idx}"}
            }]
        }]
    })

# After state 4 (found "main"): check for '(' or space-then-'('
body.append({
    "instruction_type": "conditional_branch",
    "condition_type": "numeric_compare",
    "operator": "==", "lhs": {"value": "ms_4"}, "rhs": {"value": "1"},
    "branch_on_success": [
        # If byte == '(' → match!
        {"instruction_type": "conditional_branch",
         "condition_type": "numeric_compare",
         "operator": "==", "lhs": {"value": "byte"}, "rhs": {"value": "40"},
         "branch_on_success": [
             {"instruction_type": "alu_operation", "operator": "+", "target": "found", "lhs": {"value": "0"}, "rhs": {"value": "1"}},
             {"instruction_type": "alu_operation", "operator": "-", "target": "ms_4", "lhs": {"value": "ms_4"}, "rhs": {"value": "ms_4"}}
         ],
         "branch_on_failure": [
             # If byte == ' ' → stay in state 4 (waiting for '(')
             {"instruction_type": "conditional_branch",
              "condition_type": "numeric_compare",
              "operator": "==", "lhs": {"value": "byte"}, "rhs": {"value": "32"},
              "branch_on_failure": [
                  # Any other char → reset
                  {"instruction_type": "alu_operation", "operator": "-", "target": "ms_4", "lhs": {"value": "ms_4"}, "rhs": {"value": "ms_4"}}
              ]
             }
         ]
        }
    ]
})

# Entry: if byte == 'm' → ms_0 = 1
body.append({
    "instruction_type": "conditional_branch",
    "condition_type": "numeric_compare",
    "operator": "==", "lhs": {"value": "byte"}, "rhs": {"value": "109"},
    "branch_on_success": [{
        "instruction_type": "alu_operation",
        "operator": "+", "target": "ms_0",
        "lhs": {"value": "0"}, "rhs": {"value": "1"}
    }]
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
    "package_name": "detect-main-function-in-source-code",
    "build_type": "library",
    "module_name": "detect-main-function-in-source-code",
    "description": "DFA scanner for int main( pattern",
    "extern_imports": [
        {"name": "read_entire_file_into_string", "return_type": "str",
         "arguments": [{"name": "path", "type": "str"}]}
    ],
    "function_definitions": {
        "detect-main-function-in-source-code": {
            "return_type": "i32",
            "parameter_definitions": [{"parameter_name": "path", "parameter_type": "str"}],
            "execution_instructions": instructions
        }
    }
}

with open("modules/rules/find-every-main-function-block.ipm", "w") as f:
    json.dump(spec, f, separators=(",", ":"), indent=None)
    f.write("\n")
print(f"OK: {len(instructions)} top-level, {len(body)} body, {len(spec['function_definitions']['detect-main-function-in-source-code']['execution_instructions'])} exec")
