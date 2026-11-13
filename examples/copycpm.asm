; CP/M-86 1.1 sequential FCB copy.
; Usage: XCOPY INPUT.DAT OUTPUT.DAT with a new destination.
; Copies complete 128-byte records. DOS pads a partial final record.
bits 16
cpu 8086
org 100h

; The default FCBs overlap. Copy just the drive and 8.3 name into
; private FCBs before the OS initializes their remaining fields.
    push ds
    pop es
    cld
    mov si, 5ch
    mov di, input_fcb
    mov cx, 12
    rep movsb
    mov si, 6ch
    mov di, output_fcb
    mov cx, 12
    rep movsb

    jmp short validate_names
invalid_names:
    jmp failure
validate_names:
    cmp byte [input_fcb+1], ' '
    je invalid_names
    cmp byte [output_fcb+1], ' '
    je invalid_names

    mov al, '?'
    mov di, input_fcb+1
    mov cx, 11
    repne scasb
    je invalid_names
    mov di, output_fcb+1
    mov cx, 11
    repne scasb
    je invalid_names

; Normalize an omitted drive to the current drive before comparing.
    mov cl, 19h ; get current drive (zero-based)
    push es
    int 0E0h
    pop es
    inc al
    cmp byte [input_fcb], 0
    jne check_output_drive
    mov [input_fcb], al
check_output_drive:
    cmp byte [output_fcb], 0
    jne check_names
    mov [output_fcb], al
check_names:
    push ds
    pop es
    cld
    mov si, input_fcb
    mov di, output_fcb
    mov cx, 12
    repe cmpsb
    je invalid_names

; Check for an existing destination before create. In particular, DOS
; create can truncate it. Require an exact filename and a fresh output.
    mov dx, output_fcb
    mov cl, 0Fh ; open destination FCB
    push es
    int 0E0h
    pop es
    cmp al, 0FFh
    je open_input
    mov dx, output_fcb
    mov cl, 10h ; close existing destination FCB
    push es
    int 0E0h
    pop es
    jmp failure

open_input:
    mov dx, input_fcb
    mov cl, 0Fh ; open input FCB
    push es
    int 0E0h
    pop es
    cmp al, 0FFh
    je failure

    mov dx, output_fcb
    mov cl, 16h ; create output FCB
    push es
    int 0E0h
    pop es
    cmp al, 0FFh
    je close_input_on_failure

    mov dx, buffer
    mov cl, 1Ah ; set DMA address
    push es
    int 0E0h
    pop es

read_record:
    mov dx, input_fcb
    mov cl, 14h ; read sequential 128-byte record
    push es
    int 0E0h
    pop es
    cmp al, 1
    je finish
    or al, al
    jnz close_both_on_failure

write_record:
    mov dx, output_fcb
    mov cl, 15h ; write sequential 128-byte record
    push es
    int 0E0h
    pop es
    or al, al
    jnz close_both_on_failure
    jmp read_record

finish:
    mov dx, output_fcb
    mov cl, 10h ; close output and commit its directory entry
    push es
    int 0E0h
    pop es
    cmp al, 0FFh
    je close_input_on_failure
    mov dx, input_fcb
    mov cl, 10h ; close input
    push es
    int 0E0h
    pop es
    cmp al, 0FFh
    je failure
    mov dx, success_message
    jmp print

close_both_on_failure:
    mov dx, output_fcb
    mov cl, 10h ; close partial output
    push es
    int 0E0h
    pop es
close_input_on_failure:
    mov dx, input_fcb
    mov cl, 10h ; close input
    push es
    int 0E0h
    pop es
failure:
    mov dx, failure_message
print:
    mov cl, 09h ; print dollar-terminated status
    push es
    int 0E0h
    pop es
    mov cl, 0                  ; terminate CP/M-86 program
    mov dl, 0
    int 0E0h

success_message: db 'COPY OK', 13, 10, '$'
failure_message: db 'COPY FAILED', 13, 10, '$'
input_fcb:       times 36 db 0
output_fcb:      times 36 db 0
align 2, db 0
buffer:          times 128 db 0
