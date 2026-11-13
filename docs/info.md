## How it works

An 8-bit accumulator CPU with 16 bytes of memory shared by program and data.
Every instruction is one byte: a 4-bit opcode and a 4-bit operand. Each
instruction takes two clock cycles (fetch, then execute). The full instruction
set is in `docs/ISA.md`.

Memory is empty at power-up, so a program is first written in through the pins
(load mode). The demo program is a toy stream cipher: for every input byte it
updates a key (rotate left, add a constant) and outputs the byte XORed with the
key. Feeding the output back through a freshly loaded chip returns the original
bytes. The cipher is deliberately weak; it is a learning project.

## How to test

1. Hold `rst_n` low for a few clocks, then release it.
2. Load the 16-byte program: set LOAD (`uio[4]`) high. For each address 0-15,
   put the address on `uio[3:0]`, the byte on `ui_in`, and hold WRITE
   (`uio[5]`) high for one clock.
3. Set LOAD and WRITE low. The CPU starts at address 0.
4. For each byte: put it on `ui_in`, raise READY (`uio[6]`), wait for VALID
   (`uio[7]`) to pulse, read the result from `uo_out`, then lower READY for a
   few clocks.

The bytes for the demo program come from `python tools/asm.py programs/cipher.asm out.hex`.

## External hardware

None. A microcontroller or the Tiny Tapeout demo board's RP2040 can drive the pins.
