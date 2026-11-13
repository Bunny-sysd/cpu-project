; count.asm - test program for the instructions the cipher does not use
; (LDI, SUB, JZ, JC, HLT).
;
; Expected output bytes, in order: 03, 02, 01, E0. Then the CPU halts.

        LDI 3       ; A = 3
again:  OUT         ; show A            -> 03, 02, 01
        SUB one     ; A = A - 1
        JZ  done    ; reached zero? leave the loop
        JMP again
done:   LDA big     ; A = 0xF0
        ADD big     ; 0xF0 + 0xF0 = 0x1E0: A = 0xE0, carry = 1
        JC  good    ; carry set, so this jump must happen
        HLT         ; (only reached if JC is broken)
good:   OUT         ; show A            -> E0
        HLT

        .org 14
big:    .byte 0xF0
one:    .byte 1
