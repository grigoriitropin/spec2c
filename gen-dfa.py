#!/usr/bin/env python3
"""Generate pure-IPM DFA for 9 banned patterns."""
import json

patterns = [
    ("goto ",   [103,111,116,111,32]),
    ("goto\t",  [103,111,116,111,9]),
    ("setjmp(", [115,101,116,106,109,112,40]),
    ("longjmp(",[108,111,110,103,106,109,112,40]),
    ("2>/dev/null",[50,62,47,100,101,118,47,110,117,108,108]),
    (">/dev/null", [62,47,100,101,118,47,110,117,108,108]),
    ("1>/dev/null",[49,62,47,100,101,118,47,110,117,108,108]),
    ("&>/dev/null",[38,62,47,100,101,118,47,110,117,108,108]),
    ("2>&1",     [50,62,38,49]),
]

body = []
for i in range(9):
    body.append(dict(instruction_type="variable_declaration",
        variable_name=f"s{i}", variable_type="u32",
        assignment_operation="literal", source_target=0))

for i, (name, bytes_seq) in enumerate(patterns):
    plen = len(bytes_seq)
    for pos in range(plen):
        bv = bytes_seq[pos]
        if pos == 0:
            body.append(dict(instruction_type="conditional_branch",
                condition_type="numeric_compare", operator="==",
                lhs=dict(kind="var", value=f"s{i}"),
                rhs=dict(kind="literal_int", value="0"),
                branch_on_success=[dict(
                    instruction_type="conditional_branch",
                    condition_type="numeric_compare", operator="==",
                    lhs=dict(kind="var", value="byte"),
                    rhs=dict(kind="literal_int", value=str(bv)),
                    branch_on_success=[dict(
                        instruction_type="alu_operation", operator="+",
                        lhs=dict(kind="var", value=f"s{i}"),
                        rhs=dict(kind="literal_int", value="1"),
                        target=f"s{i}")
                    ]
                )]
            ))
        else:
            body.append(dict(instruction_type="conditional_branch",
                condition_type="numeric_compare", operator="==",
                lhs=dict(kind="var", value=f"s{i}"),
                rhs=dict(kind="literal_int", value=str(pos)),
                branch_on_success=[dict(
                    instruction_type="conditional_branch",
                    condition_type="numeric_compare", operator="==",
                    lhs=dict(kind="var", value="byte"),
                    rhs=dict(kind="literal_int", value=str(bv)),
                    branch_on_success=[dict(
                        instruction_type="alu_operation", operator="+",
                        lhs=dict(kind="var", value=f"s{i}"),
                        rhs=dict(kind="literal_int", value="1"),
                        target=f"s{i}")
                    ],
                    branch_on_failure=[dict(
                        instruction_type="alu_operation", operator="+",
                        lhs=dict(kind="var", value=f"s{i}"),
                        rhs=dict(kind="literal_int", value="0"),
                        target=f"s{i}")
                    ]
                )]
            ))

    body.append(dict(instruction_type="conditional_branch",
        condition_type="numeric_compare", operator="==",
        lhs=dict(kind="var", value=f"s{i}"),
        rhs=dict(kind="literal_int", value=str(plen)),
        branch_on_success=[dict(
            instruction_type="function_invocation",
            invocation_name="report_error_and_exit",
            invocation_arguments=[
                dict(kind="var", value="_name"),
                dict(kind="str", value="uses banned pattern"),
                dict(kind="str", value="remove goto, setjmp, longjmp, or output-suppression")
            ]
        )]
    ))

d = dict(
    description="Pure IPM banned pattern DFA — 9 patterns via state machines",
    package_name="check-banned-patterns-pure-ipm",
    module_name="check-banned-patterns-pure-ipm",
    extern_imports=[dict(
        name="report_error_and_exit", return_type="void",
        arguments=[dict(name="file", type="str"), dict(name="msg", type="str"), dict(name="fix", type="str")]
    )],
    function_definitions=dict(
        main=dict(
            return_type="i32",
            execution_instructions=[
                dict(instruction_type="variable_declaration", variable_name="path", variable_type="str",
                     assignment_operation="literal", source_target="./test.txt"),
                dict(instruction_type="read_file_content", file_path="path", result_variable="content"),
                dict(instruction_type="iterate_over_bytes", collection="content",
                     item_variable="byte", index_variable="i", body=body),
                dict(instruction_type="return_statement", return_payload=dict(value="0"))
            ]
        )
    )
)

with open('check-banned-patterns-pure-ipm.ipm', 'w') as f:
    json.dump(d, f, separators=(',', ':'), indent=None)
    f.write('\n')

print(f'DFA: {len(open("check-banned-patterns-pure-ipm.ipm").read())} bytes')
