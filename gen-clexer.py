#!/usr/bin/env python3
"""Generate C-lexer DFA for function count + length checks"""
import json

# ── Helpers ──
def cond(op, lhs, rhs, on_true=None, on_false=None):
    """Generate numeric_compare conditional_branch"""
    r = {
        "instruction_type": "conditional_branch",
        "condition_type": "numeric_compare",
        "operator": op,
        "lhs": {"value": lhs},
        "rhs": {"value": rhs}
    }
    if on_true: r["branch_on_success"] = on_true
    if on_false: r["branch_on_failure"] = on_false
    return r

def alu(op, target, lhs, rhs):
    return {
        "instruction_type": "alu_operation",
        "operator": op, "target": target,
        "lhs": {"value": lhs}, "rhs": {"value": rhs}
    }

def set_var(v, val):
    """Set variable to literal integer value"""
    return alu("+", v, "0", str(val))

def bump(v):
    """v++"""
    return alu("+", v, v, "1")

def decr(v):
    """v--"""
    return alu("-", v, v, "1")

def reset(v):
    """v = v - v (set to 0)"""
    return alu("-", v, v, v)

def byte_is(ch, on_true=None, on_false=None):
    """Check if byte == character code"""
    return cond("==", "byte", str(ord(ch)), on_true=on_true, on_false=on_false)

# ── Body builder ──
body = []

# Track prev byte for comment detection
# prev_slash: 1 if previous byte was '/'
# skip_asterisk: 1 if prev byte was '*' (for block comment end detection)

# ── State variables ──
# in_str: 1 inside strings, in_chr: 1 inside chars
# line_cm: 1 inside // comment, block_cm: 1 inside /* */
# esc_next: 1 skip next char (backslash escape)
# prev_sl: 1 previous byte was '/'
# prev_as: 1 previous byte was '*' (for */ end detection)

# ── Handle escape_skip ──
body.append(cond("==", "esc_next", "1",
    on_true=[reset("esc_next"), reset("prev_sl"), reset("prev_as")]
))
# ── State: in string ──
body.append(cond("==", "in_str", "1", on_true=[
    cond("==", "byte", "92",  # backslash
        on_true=[set_var("esc_next", 1)],
        on_false=[cond("==", "byte", "34",  # quote
            on_true=[reset("in_str")]
        )]
    ),
    reset("prev_sl"), reset("prev_as")
]))
# ── State: in char ──
body.append(cond("==", "in_chr", "1", on_true=[
    cond("==", "byte", "92",  # backslash
        on_true=[set_var("esc_next", 1)],
        on_false=[cond("==", "byte", "39",  # single quote
            on_true=[reset("in_chr")]
        )]
    ),
    reset("prev_sl"), reset("prev_as")
]))
# ── State: line comment ──
body.append(cond("==", "line_cm", "1", on_true=[
    byte_is('\n', on_true=[reset("line_cm")]),
    reset("prev_sl"), reset("prev_as")
]))
# ── State: block comment ──
body.append(cond("==", "block_cm", "1", on_true=[
    cond("==", "prev_as", "1", on_true=[
        byte_is('/', on_true=[reset("block_cm"), reset("prev_as")]),
        reset("prev_as")
    ]),
    byte_is('*', on_true=[set_var("prev_as", 1)]),
]))
# ── State: NORMAL (none of the above) ──
# We use a helper: in_any_special = in_str OR in_chr OR line_cm OR block_cm
# Check each: if in_str == 1 → already handled above
# So: if (in_str == 0 && in_chr == 0 && line_cm == 0 && block_cm == 0) → NORMAL

# Use nested conditionals to check NORMAL state
# Simplified: check in_str == 0 first, then in_chr == 0, etc.

# Enter string?
normal = []
normal.append(byte_is('"', on_true=[set_var("in_str", 1), reset("prev_sl")]))
normal.append(cond("==", "byte", "39", on_true=[set_var("in_chr", 1), reset("prev_sl")]))  # '
# Comment starts: prev_sl == 1 and byte in ['/', '*']
normal.append(cond("==", "prev_sl", "1", on_true=[
    byte_is('/', on_true=[set_var("line_cm", 1), reset("prev_sl")]),
    byte_is('*', on_true=[set_var("block_cm", 1), reset("prev_sl")])
]))
# Track if byte is '/'
normal.append(byte_is('/', on_true=[set_var("prev_sl", 1)],
    on_false=[reset("prev_sl")]))

# Nesting logic (in NORMAL state only)
# If byte == '{': nesting_level++
#   AND if nesting_level was 0 → function_count++
normal.append(byte_is('{', on_true=[
    cond("==", "nesting", "0", on_true=[bump("func_count")]),
    bump("nesting")
]))
# If byte == '}': nesting_level--
#   AND if nesting_level becomes 0 after decrement → func_lines = 0
normal.append(byte_is('}', on_true=[
    decr("nesting"),
    cond("==", "nesting", "0", on_true=[reset("func_lines")])
]))

# Line counting (inside function: nesting_level > 0)
normal.append(cond(">", "nesting", "0", on_true=[
    byte_is('\n', on_true=[
        bump("func_lines"),
        cond(">", "func_lines", "50", on_true=[set_var("found", 1)])
    ])
]))

# Wrap NORMAL in: if in_str == 0 && in_chr == 0 && line_cm == 0 && block_cm == 0
normal_wrap = cond("==", "in_str", "0", on_true=[
    cond("==", "in_chr", "0", on_true=[
        cond("==", "line_cm", "0", on_true=[
            cond("==", "block_cm", "0", on_true=normal)
        ])
    ])
])
body.append(normal_wrap)

# ── Build spec ──

instructions = []

# Read file
instructions.append({
    "instruction_type": "read_file_content",
    "file_path": "path",
    "result_variable": "content"
})

# State variables
vars_init = [
    ("found", 0), ("func_count", 0), ("func_lines", 0),
    ("nesting", 0), ("in_str", 0), ("in_chr", 0),
    ("line_cm", 0), ("block_cm", 0), ("esc_next", 0),
    ("prev_sl", 0), ("prev_as", 0)
]
for vn, vv in vars_init:
    instructions.append({
        "instruction_type": "variable_declaration",
        "variable_name": vn, "variable_type": "i32",
        "assignment_operation": "literal", "source_target": vv
    })

# Iterate
iter_inst = {
    "instruction_type": "iterate_over_bytes",
    "collection": "content",
    "item_variable": "byte",
    "index_variable": "i",
    "body": body
}

# Post-loop checks
check_func_count = cond(">", "func_count", "10",
    on_true=[set_var("found", 1)])
check_found = cond("!=", "found", "0",
    on_true=[{"instruction_type": "return_statement", "return_payload": {"value": "1"}}])
return_ok = {"instruction_type": "return_statement", "return_payload": {"value": "0"}}

# NULL check wrapper
instructions.append({
    "instruction_type": "conditional_branch",
    "condition_operation": "is_not_null",
    "condition_target": "content",
    "branch_on_success": [iter_inst, check_func_count, check_found, return_ok],
    "branch_on_failure": [return_ok]
})

spec = {
    "package_name": "scan-and-count-c-function-blocks",
    "build_type": "library",
    "module_name": "scan-and-count-c-function-blocks",
    "description": "C lexer DFA — counts function blocks and checks line limits",
    "extern_imports": [
        {"name": "read_entire_file_into_string", "return_type": "str",
         "arguments": [{"name": "path", "type": "str"}]}
    ],
    "function_definitions": {
        "scan-and-count-c-function-blocks": {
            "return_type": "i32",
            "parameter_definitions": [
                {"parameter_name": "path", "parameter_type": "str"}
            ],
            "execution_instructions": instructions
        }
    }
}

with open("modules/rules/scan-and-count-c-function-blocks.ipm", "w") as f:
    json.dump(spec, f, separators=(",", ":"), indent=None)
    f.write("\n")

print(f"OK: {len(instructions)} top-level, {len(body)} body, {len(vars_init)} vars")
