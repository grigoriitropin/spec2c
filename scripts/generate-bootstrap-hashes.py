#!/usr/bin/env python3
"""Generate compiled SHA256 hashes of all bootstrap source files."""
import os, subprocess, sys

SRC_DIR = sys.argv[1] if len(sys.argv) > 1 else 'src'
WHITELIST = os.path.join(SRC_DIR, 'bootstrap-c-whitelist.txt')
OUTPUT = os.path.join(SRC_DIR, 'bootstrap-compiled-limit-hash-data/bootstrap-file-sha-hashes-generated.h')

def read_whitelist():
    names = []
    with open(WHITELIST) as f:
        for line in f:
            line = line.strip()
            if line and line[0] != '#':
                names.append(line)
    return names

def find_file(basename):
    for root, dirs, files in os.walk(SRC_DIR):
        if basename in files:
            return os.path.join(root, basename)
    return None

def sha256_hex(path):
    r = subprocess.run(['sha256sum', path], capture_output=True, text=True)
    if r.returncode == 0:
        return r.stdout.split()[0]
    # fallback: use openssl
    r = subprocess.run(['openssl', 'dgst', '-sha256', path], capture_output=True, text=True)
    if r.returncode == 0:
        return r.stdout.split()[-1]
    return None

names = read_whitelist()
hashes = []
for name in names:
    path = find_file(name)
    if path:
        h = sha256_hex(path)
        if h:
            hashes.append((name, h))
            continue
    print(f'WARNING: cannot hash {name}', file=sys.stderr)
    hashes.append((name, '0' * 64))

with open(OUTPUT, 'w') as f:
    f.write('// AUTO-GENERATED — SHA256 hashes of bootstrap files, compiled into binary\n')
    f.write(f'#define BOOTSTRAP_HASH_COUNT {len(hashes)}\n')
    f.write('static const char *hash_file_names[] = {\n')
    for name, _ in hashes:
        f.write(f'    "{name}",\n')
    f.write('};\n')
    f.write('static const char *hash_sha256_values[] = {\n')
    for _, h in hashes:
        f.write(f'    "{h}",\n')
    f.write('};\n')
    f.write('static const int hash_max_lines[] = {\n')
    # Keep line limits too for the freeze check
    limits = {}
    limits_path = os.path.join(SRC_DIR, 'bootstrap-c-freeze-limits.txt')
    if os.path.exists(limits_path):
        with open(limits_path) as lf:
            for line in lf:
                line = line.strip()
                if line and line[0] != '#':
                    p = line.split()
                    if len(p) >= 2:
                        limits[p[0]] = p[1]
    for name, _ in hashes:
        f.write(f'    {limits.get(name, "0")},\n')
    f.write('};\n')

print(f'Generated {len(hashes)} SHA256 hashes to {OUTPUT}')
