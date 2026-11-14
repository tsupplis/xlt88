#!/bin/sh
set -eu

root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT
bin2cmd=${BIN2CMD:-bin2cmd}
emu2=${EMU2:-emu2}
export EMU2_DOSVER=1.0 EMU2_CPM_VER=2.2

cd "$tmp"
cp "$root/examples/hello.asm" source.80
ln source.80 hardlink.asm
ln -s source.80 symlink.asm
for alias in ./source.80 hardlink.asm symlink.asm; do
    if "$root/xlt88" source.80 "$alias" 2> alias.err; then
        exit 1
    fi
    grep -F 'input and output must differ' alias.err
    cmp "$root/examples/hello.asm" source.80
done

for target in msdos cpm86; do
    "$root/xlt88" --target="$target" --dialect=intel "$root/examples/hello.asm" hello.asm
    nasm -f bin hello.asm -o hello.com
    program=hello.com
    if [ "$target" = cpm86 ]; then
        "$bin2cmd" hello.com hello.cmd
        program=hello.cmd
        grep -A2 'push es.*console output' hello.asm | grep -F 'pop es'
    else
        grep -A1 'mov ah, 02h.*console output' hello.asm | grep -F 'int 21h'
        grep -F 'int 20h' hello.asm
    fi
    "$emu2" "$program" > hello.out
    grep -F 'Hello, world from Assembler!' hello.out

    cat > api.80 <<'ASM'
        org 100h
        lxi d, buffer
        mvi c, 26
        call 5
        mvi c, 25
        call 5
        cmp l
        jnz failed
        lxi d, okay
        jmp print
failed: lxi d, error
print:  mvi c, 9
        call 5
        jmp 0
buffer: ds 128
okay:   db 'API OK$'
error:  db 'API FAILED$'
        end
ASM
    if [ "$target" = cpm86 ]; then
        sed '2i\
        mvi c, 12\
        call 5\
        cpi 22h\
        jnz failed\
        mov a, l\
        cpi 22h\
        jnz failed
' api.80 > version.80
        mv version.80 api.80
    fi
    "$root/xlt88" --target="$target" api.80 api.asm
    nasm -Werror -f bin api.asm -o api.com
    program=api.com
    if [ "$target" = cpm86 ]; then
        "$bin2cmd" api.com api.cmd
        program=api.cmd
    fi
    "$emu2" "$program" > api.out
    grep -F 'API OK' api.out

    "$root/xlt88" --target="$target" "$root/tests/andflags.asm" andflags.asm
    nasm -f bin andflags.asm -o and-flags.com
    if [ "$target" = cpm86 ]; then
        "$bin2cmd" and-flags.com and-flags.cmd
        "$emu2" and-flags.cmd > and-flags.out
    else
        "$emu2" and-flags.com > and-flags.out
    fi
    grep -F 'AND FLAGS OK' and-flags.out

    "$root/xlt88" --target="$target" "$root/examples/tst8080.asm" diagnostic.asm
    nasm -Werror -f bin diagnostic.asm -o diagnostic.com
    program=diagnostic.com
    if [ "$target" = cpm86 ]; then
        "$bin2cmd" diagnostic.com diagnostic.cmd
        program=diagnostic.cmd
        grep -A1 'mov ss, si' diagnostic.asm | grep -E 'mov[[:space:]]+sp,'
    fi
    "$emu2" "$program" > diagnostic.out
    grep -F 'CPU IS OPERATIONAL' diagnostic.out
    if grep -F 'CPU HAS FAILED' diagnostic.out; then
        exit 1
    fi

    "$root/xlt88" --target="$target" "$root/tests/opcodes.asm" remaining.asm
    nasm -Werror -f bin remaining.asm -o remaining.com
    program=remaining.com
    if [ "$target" = cpm86 ]; then
        # NOP, then MOV CL,0 / MOV DL,0 / INT E0h for RST 0.
        printf '\220\261\000\262\000\315\340' > remaining.expected
        "$bin2cmd" remaining.com remaining.cmd
        program=remaining.cmd
    else
        # NOP, then INT 20h for RST 0.
        printf '\220\315\040' > remaining.expected
    fi
    # CLI, IN AL,60h, OUT 61h,AL, STI, HLT.
    printf '\372\344\140\346\141\373\364' >> remaining.expected
    cmp remaining.expected remaining.com
    "$emu2" "$program" > remaining.out

    for vector in 1 2 3 4 5 6 7; do
        printf '        rst %s\n' "$vector" > restart.80
        if "$root/xlt88" --target="$target" restart.80 restart.asm 2> restart.err; then
            exit 1
        fi
        grep -Fq "unsupported 8080 restart vector: $vector" restart.err
    done

    "$root/xlt88" --target="$target" "$root/tests/copy.asm" copy.asm
    nasm -Werror -f bin copy.asm -o copy.com

    cat > ambiguous.80 <<'ASM'
        mvi c, 2
entry:  call 5
        end
ASM
    if "$root/xlt88" --target="$target" ambiguous.80 ambiguous.asm 2> ambiguous.err; then
        exit 1
    fi
    grep -F 'xlt88:2: BDOS call needs' ambiguous.err

    cat > transfers.80 <<'ASM'
        jmp 0+5
        cz service
        call 10005h
service equ 0+5
        end
ASM
    if "$root/xlt88" --target="$target" transfers.80 transfers.asm 2> transfers.err; then
        exit 1
    fi
    grep -F 'xlt88:1: unsupported transfer target expression: 0+5' transfers.err
    grep -F 'xlt88:2: unsupported transfer target expression: service' transfers.err
    grep -F 'xlt88:3: transfer target exceeds a 16-bit address: 10005h' transfers.err

    cat > layout.80 <<'ASM'
packed: db 12h
        dw 3456h
        align 2
table:  dw 789ah
        end
ASM
    "$root/xlt88" --target="$target" layout.80 layout.asm
    nasm -f bin layout.asm -o layout.bin
    printf '\022\126\064\000\232\170' > expected.bin
    cmp expected.bin layout.bin

    cat > symbols.80 <<'ASM'
        jmp cpu
CPU:    db 'AND', (cPu AND 0ffh), (CPU / 100h)
        dw aLiAs
ALIAS   equ CPU
        end
ASM
    "$root/xlt88" --target="$target" symbols.80 symbols.asm
    nasm -Werror -f bin symbols.asm -o symbols.bin
    printf '\353\000AND\002\001\002\001' > expected.bin
    cmp expected.bin symbols.bin
done

test_copy()
{
    program=$1
    dd if=/dev/zero of=input.dat bs=128 count=130 2>/dev/null
    printf 'first record' | dd of=input.dat conv=notrunc 2>/dev/null
    printf 'last record' | dd of=input.dat bs=128 seek=129 conv=notrunc 2>/dev/null
    "$emu2" "$program" input.dat output.dat > copy.out
    grep -F 'COPY OK' copy.out
    cmp input.dat output.dat
    "$emu2" "$program" input.dat output.dat > copy.out
    grep -F 'COPY FAILED' copy.out
    cmp input.dat output.dat
    "$emu2" "$program" input.dat A:input.dat > copy.out
    grep -F 'COPY FAILED' copy.out
    cmp input.dat output.dat
    "$emu2" "$program" missing.dat absent.dat > copy.out
    grep -F 'COPY FAILED' copy.out
    test ! -e absent.dat
}

mkdir dos cpm86
(
    cd dos
    nasm -f bin "$root/examples/copy-dos.asm" -o xcopy.com
    test_copy xcopy.com
    printf 'partial record' > partial.dat
    "$emu2" xcopy.com partial.dat padded.dat > copy.out
    grep -F 'COPY OK' copy.out
    dd if=partial.dat of=padded.expected bs=128 conv=sync 2>/dev/null
    cmp padded.expected padded.dat
)
(
    cd cpm86
    nasm -f bin "$root/examples/copycpm.asm" -o xcopy.bin
    "$bin2cmd" xcopy.bin xcopy.cmd
    test_copy xcopy.cmd
)
