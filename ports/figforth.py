"""Explicit console-only port of the original FIG-Forth example to CP/M BDOS.

The original remains unchanged. Disk and printer BIOS requests fail closed.
The resulting 8080 source is translated normally; no emulator/runtime is added.
"""
from pathlib import Path
import re
import sys


def port_source(original):
    def replace(text, pattern, replacement):
        result, count = re.subn(pattern, replacement, text, flags=re.M | re.S)
        if count != 1:
            raise ValueError('FIG-Forth source pattern changed: '+pattern)
        return result

    text = replace(original, r'^IOS\s+LHLD\s+1\b.*?^\s*PCHL[^\n]*', '''IOS:
 mvi c,9
 lxi d,porterr
 call 5
 jmp 0
porterr: db 'Unsupported disk/printer request in console-only Forth',13,10,'$'
''')
    text = replace(text, r'^CSTAT\s+PUSH\s+B\b.*?(?=^CPOUT\s)', '''; Explicit native BDOS console port, preserving Forth's working registers.
CSTAT: push b
 push h
 push d
 mvi c,11
 call 5
 pop d
 pop h
 pop b
 ret
CIN: push b
 push h
 push d
cinwait: mvi c,6
 mvi e,255
 call 5
 ora a
 jz cinwait
 pop d
 pop h
 pop b
 ret
COUT: push b
 push h
 push d
 mov e,c
 mvi c,6
 call 5
 pop d
 pop h
 pop b
 ret
POUT: jmp IOS
;
''')
    return '; Explicit console-only FIG-Forth port; see ports/figforth.py\n'+text


if __name__ == '__main__':
    original = Path(__file__).resolve().parents[1]/'examples'/'forth.asm'
    Path(sys.argv[1]).write_text(port_source(original.read_text()))
