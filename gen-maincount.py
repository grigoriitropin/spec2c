#!/usr/bin/env python3
"""Generate find-every-main-function-block.ipm — strstr-based main() detector"""
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

# Call match_pattern_against_text_string(content, "main(")
instructions.append({
    "instruction_type": "function_invocation",
    "invocation_name": "match_pattern_against_text_string",
    "invocation_arguments": [
        {"kind": "var", "value": "content"},
        {"kind": "str", "value": "main("}
    ],
    "result_assignment_variable": "found",
    "result_type": "i32"
})

# Check found != 0 → return 1 else 0
instructions.append({
    "instruction_type": "conditional_branch",
    "condition_type": "numeric_compare",
    "operator": "!=", "lhs": {"value": "found"}, "rhs": {"value": "0"},
    "branch_on_success": [{"instruction_type": "return_statement", "return_payload": {"value": "1"}}]
})
instructions.append({"instruction_type": "return_statement", "return_payload": {"value": "0"}})

spec = {
    "package_name": "find-every-main-function-block",
    "build_type": "library",
    "module_name": "find-every-main-function-block",
    "description": "Pattern-based main() detector",
    "extern_imports": [
        {"name": "read_entire_file_into_string", "return_type": "str",
         "arguments": [{"name": "path", "type": "str"}]},
        {"name": "match_pattern_against_text_string", "return_type": "i32",
         "arguments": [{"name": "text", "type": "str"}, {"name": "pattern", "type": "str"}]}
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
print("OK")
