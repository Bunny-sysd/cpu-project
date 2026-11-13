; cipher.asm - toy stream cipher (the chip's main job)
;
; For every byte that arrives: stir the key, XOR the byte with it, output it.
; Running the same program on the encrypted bytes gives the original back,
; because XOR-ing with the same key twice cancels out.

loop:   LDA key     ; A = current key
        ROL         ; stir it: rotate left one bit
        ADD step    ; stir it more: add a constant
        STA key     ; save the new key for next time
        WAIT        ; pause until the sender raises READY
        IN          ; A = byte on the input pins
        XOR key     ; encrypt / decrypt
        OUT         ; put the result on the output pins (VALID pulses)
        JMP loop    ; next byte

        .org 14
step:   .byte 0x5B  ; any odd number
key:    .byte 0xA7  ; the secret starting key
