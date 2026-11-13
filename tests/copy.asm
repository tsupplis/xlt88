        org 100h
        lxi h, buffer
        mov a, l
        ani 1
        jnz error
        lxi h, packed
        inx h
        mov a, m
        cpi 34h
        jnz error

        lxi h, 5ch
        lxi d, input
        call name
        lxi h, 6ch
        lxi d, output
        call name

        lxi d, input
        mvi c, 15
        call bdos
        cpi 0ffh
        jz error

        lxi d, output
        mvi c, 22
        call bdos
        cpi 0ffh
        jz error

        lxi d, buffer
        mvi c, 26
        call bdos
loop:   lxi d, input
        mvi c, 20
        call bdos
        cpi 1
        jz done
        ora a
        jnz error
        lxi d, output
        mvi c, 21
        call bdos
        ora a
        jnz error
        jmp loop

done:   lxi d, input
        mvi c, 16
        call bdos
        cpi 0ffh
        jz error
        lxi d, output
        mvi c, 16
        call bdos
        cpi 0ffh
        jz error
        lxi d, success
        jmp print
error:  lxi d, failure
print:  mvi c, 9
        call bdos
        jmp warm

name:   mvi b, 12
next:   mov a, m
        stax d
        inx h
        inx d
        dcr b
        jnz next
        ret

success: db 'COPY OK',13,10,'$'
failure: db 'COPY FAILED',13,10,'$'
input:  ds 36
output: ds 36
packed: db 0
        dw 1234h
        align 2
buffer: ds 128
bdos    equ service
service equ 5
warm    equ 0
        end
