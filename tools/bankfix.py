#!/usr/bin/env python3
"""bankfix.py: finish placing a banked source file's code into its bank.

    bankfix.py IN.s OUT.s

(It does the same for the near code that goes above $8000, whose section
is `hicode`: the "bank" is then that section.)

A banked file names its bank with `#pragma clang section text="bank_x"
rodata="bank_x_ro"`, but cc6502's cross-call optimisation lifts repeated
instruction sequences into subroutines that it always puts in the `code`
section, and switch jump tables go in `switch`. Left there they would take
room in the PRG. This moves every `code` and `switch` fragment that only
banked code uses into the bank (text to bank_x, tables to bank_x_ro), and
whatever a near function in the file uses to hicode/hicode_ro.

Near functions in a banked file are the ones it puts in hicode on purpose
(function pointer targets, and anything called while the bank is not
mapped). A near function that refers directly to something in the bank
would run with the bank unmapped, so that is an error.
"""

import re
import sys

LABEL = re.compile(r'^(`[^`]+`|[A-Za-z_.$?][\w.$?]*):')
SECTION = re.compile(r'^\s+\.section\s+([^,\s]+)\s*,\s*(\w+)(.*)$')
TOKEN = re.compile(r'`[^`]+`|[A-Za-z_.$?][\w.$?]*')
NEAR = 'hicode'


def main():
    src, dst = sys.argv[1], sys.argv[2]
    lines = open(src).read().split('\n')

    # Split into chunks, each starting at a .section directive.
    chunks = []
    cur = {'section': None, 'kind': None, 'lines': [], 'labels': []}
    for line in lines:
        m = SECTION.match(line)
        if m:
            chunks.append(cur)
            cur = {'section': m.group(1), 'kind': m.group(2),
                   'rest': m.group(3), 'lines': [line], 'labels': []}
            continue
        cur['lines'].append(line)
        lm = LABEL.match(line)
        if lm:
            cur['labels'].append(lm.group(1))
    chunks.append(cur)

    texts = {c['section'] for c in chunks
             if c['section'] and c['kind'] == 'text'} - {'code'}
    banks = {t for t in texts if t.startswith('bank_')}
    if len(banks) > 1 or texts - banks - {NEAR}:
        sys.exit(f'{src}: expected one bank_* section and/or {NEAR}, '
                 f'found {sorted(texts)}')
    bank = banks.pop() if banks else NEAR
    bank_ro = bank + '_ro'

    owner = {}
    for i, c in enumerate(chunks):
        for l in c['labels']:
            owner[l] = i

    def refs(c):
        out = set()
        for line in c['lines']:
            if LABEL.match(line) or line.lstrip().startswith(';'):
                body = LABEL.sub('', line)
            else:
                body = line
            body = body.split(';')[0]
            if '.mmuslot5' in body or '.mmubank' in body:
                continue        # a banked call, which maps the bank itself
            for t in TOKEN.findall(body):
                if t in owner:
                    out.add(owner[t])
        return out

    movable = {'code', 'switch'}

    def is_lifted(c):
        return c['labels'] and c['labels'][0].startswith('`?')

    # Near roots: the file's functions in hicode (or, if it has any left in
    # the default section, those).
    near = set()
    work = [i for i, c in enumerate(chunks)
            if c['section'] in ('code', NEAR) and not is_lifted(c)]
    while work:
        i = work.pop()
        if i in near:
            continue
        near.add(i)
        for j in refs(chunks[i]):
            if chunks[j]['section'] in movable:
                work.append(j)
            elif bank != NEAR and chunks[j]['section'] in (bank, bank_ro):
                c = chunks[i]
                sys.exit(f'{src}: near code {c["labels"][:1]} refers to '
                         f'{chunks[j]["labels"][:1]} in {chunks[j]["section"]}')

    for i, c in enumerate(chunks):
        if c['section'] in movable:
            home = NEAR if i in near else bank
            target = home if c['section'] == 'code' else home + '_ro'
            kind = 'text' if c['section'] == 'code' else 'rodata'
            c['lines'][0] = f'            .section {target},{kind}{c["rest"]}'

    # cc6502 interleaves the C source as comments, which as6502 cannot
    # always read back (escaped quotes), so they are left out.
    out = []
    for c in chunks:
        out.extend(l for l in c['lines'] if not l.lstrip().startswith(';'))
    open(dst, 'w').write('\n'.join(out) + '\n')


if __name__ == '__main__':
    main()
