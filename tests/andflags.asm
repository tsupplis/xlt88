        org 100h

        ; Only the original A contributes AC. The result is zero,
        ; with even parity and carry clear.
        mvi a, 08h
        mvi e, 00h
        stc
        ana e
        push psw
        pop b
        mov a, c
        cpi 56h
        jnz fail
        mov a, b
        ora a
        jnz fail
        mov a, e
        ora a
        jnz fail

        ; Only the memory operand contributes AC. DAA sees it.
        lxi h, mask
        mvi a, 01h
        ana m
        daa
        cpi 07h
        jnz fail

        ; Neither operand contributes AC. A prior AC must be cleared.
        mvi a, 0fh
        adi 1
        mvi a, 81h
        ani 81h
        push psw
        pop b
        mov a, c
        cpi 86h
        jnz fail
        mov a, b
        cpi 81h
        jnz fail

        ; Immediate operand supplies AC and clears carry.
        mvi a, 1
        stc
        ani 9
        daa
        cpi 7
        jnz fail

        ; ANA A reads A before updating it.
        mvi a, 08h
        ana a
        daa
        cpi 0eh
        jnz fail

        lxi d, success
        jmp print
fail:   lxi d, failure
print:  mvi c, 9
        call 5
        jmp 0

mask:   db 09h
success: db 'AND FLAGS OK',13,10,'$'
failure: db 'AND FLAGS FAILED',13,10,'$'
        end
