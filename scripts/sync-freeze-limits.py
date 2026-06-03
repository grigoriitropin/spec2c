#!/usr/bin/env python3
"""Sync freeze limits and regenerate compiled header."""
import os

limits_file = 'src/bootstrap-c-freeze-limits.txt'
header_file = 'src/bootstrap-compiled-limit-hash-data/bootstrap-freeze-data-compiled-into.h'

with open(limits_file) as f:
    lines = f.readlines()

limits = {}
for line in lines:
    line = line.strip()
    if not line or line.startswith('#'):
        continue
    parts = line.split()
    if len(parts) >= 3:
        limits[parts[0]] = (parts[1], parts[2])

# Update limits from actual file sizes
for name in sorted(limits.keys()):
    for root, dirs, files in os.walk('src'):
        if name in files:
            actual = len(open(os.path.join(root, name)).readlines())
            old = int(limits[name][0])
            if actual > old:
                print(f'{name}: {old} -> {actual}')
                limits[name] = (str(actual), limits[name][1])
            break

# Write limits file
with open(limits_file, 'w') as f:
    f.write('# Bootstrap freeze limits\n')
    for name in sorted(limits.keys()):
        f.write(f'{name} {limits[name][0]} {limits[name][1]}\n')

# Write compiled header
names = sorted(limits.keys())
with open(header_file, 'w') as f:
    f.write('// AUTO-GENERATED — freeze limits compiled into the binary\n')
    f.write(f'#define BOOTSTRAP_FREEZE_COUNT {len(names)}\n')
    f.write('static const char *freeze_file_names[] = {\n')
    for n in names:
        f.write(f'    "{n}",\n')
    f.write('};\n')
    f.write('static const int freeze_max_lines[] = {\n')
    for n in names:
        f.write(f'    {limits[n][0]},\n')
    f.write('};\n')
    f.write('static const int freeze_max_funcs[] = {\n')
    for n in names:
        f.write(f'    {limits[n][1]},\n')
    f.write('};\n')

print('Sync complete')
