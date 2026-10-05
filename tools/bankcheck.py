#!/usr/bin/env python3
"""bankcheck.py: refuse plain references to banked functions.

    bankcheck.py FILE.s...

Given the assembly of every source file in the program (banked files after
bankfix.py), find the public functions placed in a bank (bank_* text
sections), then fail if any file refers to one other than through a banked
call (`.mmuslot5`/`.mmubank`). A plain JSR or JMP to a banked function from
anywhere but its own bank lands in whatever is mapped at the time, and so
does a call through its address. The compiler does not catch either: a
prototype without __banked silently makes callers use JSR.
"""

import re
import sys

LABEL = re.compile(r'^(`[^`]+`|[A-Za-z_.$?][\w.$?]*):')
SECTION = re.compile(r'^\s+\.section\s+([^,\s]+)\s*,\s*(\w+)')
TOKEN = re.compile(r'`[^`]+`|[A-Za-z_.$?][\w.$?]*')


def parse(path):
    """Return (publics, [(section, line)]) for one file."""
    publics = set()
    body = []
    section = None
    for line in open(path):
        line = line.rstrip('\n')
        m = SECTION.match(line)
        if m:
            section = m.group(1)
            continue
        s = line.split(';')[0]
        pm = re.match(r'^\s+\.public\s+(.*)$', s)
        if pm:
            publics.update(x.strip() for x in pm.group(1).split(','))
            continue
        body.append((section, s))
    return publics, body


def main():
    files = sys.argv[1:]
    parsed = {f: parse(f) for f in files}

    # Which bank each public label is placed in, if any.
    banked = {}
    for f, (publics, body) in parsed.items():
        for section, line in body:
            lm = LABEL.match(line)
            if lm and lm.group(1) in publics and section \
                    and section.startswith('bank_') \
                    and not section.endswith('_ro'):
                banked[lm.group(1)] = section

    errors = []
    for f, (publics, body) in parsed.items():
        for section, line in body:
            if LABEL.match(line):
                line = LABEL.sub('', line)
            if '.mmuslot5' in line or '.mmubank' in line:
                continue
            if re.match(r'^\s+\.(extern|public|section|rtmodel)', line):
                continue
            for t in TOKEN.findall(line):
                if t in banked and section != banked[t]:
                    errors.append(f'{f}: in {section}: {line.strip()} '
                                  f'-- {t} is in {banked[t]}')
    if errors:
        print('\n'.join(errors), file=sys.stderr)
        sys.exit(1)


if __name__ == '__main__':
    main()
