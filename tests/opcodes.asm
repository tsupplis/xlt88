        org 100h
        nop
        rst 0

        ; Verify the emitted bytes for hardware instructions on both
        ; targets. The running program exits before reaching this block.
        di
        in 60h
        out 61h
        ei
        hlt
        end
