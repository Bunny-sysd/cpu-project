# ISA - cipher CPU

Status: approved by Aaron on 2026-11-13 and implemented in `src/`. Opcode 0xE is
still free. If you change anything here, `src/control_unit.v` and `tools/asm.py`
must change to match.

## The machine in one paragraph

An 8-bit accumulator CPU. There is one working register, **A**. Almost every
instruction either loads A, changes A, or saves A. Memory is 16 bytes, and the
program and its data share those same 16 bytes.

| Thing            | Size   | Meaning                                         |
|------------------|--------|-------------------------------------------------|
| A (accumulator)  | 8 bits | the one register all the maths happens in       |
| PC               | 4 bits | address of the next instruction (0-15)          |
| Z flag           | 1 bit  | 1 when the last result in A was zero            |
| C flag           | 1 bit  | carry out of ADD, borrow of SUB, bit out of ROL |
| Memory           | 16 x 8 | program and data together                       |

## Instruction format

Every instruction is exactly one byte:

```
 bit  7 6 5 4   3 2 1 0
     [ opcode ] [operand]
```

The operand is a memory address (0-15) or, for LDI, a small constant (0-15).
Instructions that need no operand ignore it (write it as 0).

## Instructions

| Opcode | Mnemonic   | What it does                                   | Flags |
|--------|------------|------------------------------------------------|-------|
| 0x0    | `NOP`      | nothing                                        | -     |
| 0x1    | `LDA addr` | A = mem[addr]                                  | Z     |
| 0x2    | `LDI n`    | A = n (0-15, upper 4 bits become 0)            | Z     |
| 0x3    | `STA addr` | mem[addr] = A                                  | -     |
| 0x4    | `ADD addr` | A = A + mem[addr]                              | Z, C  |
| 0x5    | `SUB addr` | A = A - mem[addr]                              | Z, C  |
| 0x6    | `XOR addr` | A = A xor mem[addr]                            | Z     |
| 0x7    | `ROL`      | rotate A left by 1 (bit 7 wraps round to bit 0) | Z, C  |
| 0x8    | `IN`       | A = input pins (`ui_in`)                       | Z     |
| 0x9    | `OUT`      | output pins (`uo_out`) = A                     | -     |
| 0xA    | `JMP addr` | PC = addr                                      | -     |
| 0xB    | `JZ addr`  | PC = addr if Z is 1                            | -     |
| 0xC    | `JC addr`  | PC = addr if C is 1                            | -     |
| 0xD    | `WAIT`     | pause until a new byte arrives (READY pin goes 0 -> 1) | -     |
| 0xE    | (unused)   | acts as NOP                                    | -     |
| 0xF    | `HLT`      | stop until reset                               | -     |

Why ROL instead of a plain shift: a shift throws a bit away every time, a
rotate keeps all 8 bits. For a cipher key that matters - a shifted key would
turn into all zeros after 8 steps.

## Loading a program (not an instruction, but part of the design)

A real chip powers up with empty memory, so the wrapper has a load mode:

- `uio_in[4]` = LOAD. While it is 1 the CPU is frozen with PC = 0.
- `uio_in[3:0]` = address to write, `ui_in[7:0]` = byte to write.
- `uio_in[5]` = WRITE. The byte is stored on a clock edge while WRITE is 1.
- Drop LOAD to 0 and the CPU starts running from address 0.

## Handshake pins (how the outside world feeds bytes in)

- `uio_in[6]` = READY. The sender puts a byte on `ui_in`, then raises READY.
  `WAIT` lets the program continue once per 0 -> 1 change of READY, so holding
  READY high does not encrypt the same byte twice.
- `uio_out[7]` = VALID. Pulses high for one clock when `OUT` runs, telling the
  receiver "a fresh result is on `uo_out` now".

## The demo program: toy stream cipher

Each trip round the loop makes a new key byte, waits for one byte on the input
pins, XORs the two, and shows the result on the output pins. Exactly one key
step per byte, so sender and receiver cannot drift apart.

```
addr  code        meaning
0     LDA key     ; A = current key
1     ROL         ; stir it
2     ADD step    ; stir it more
3     STA key     ; save the new key
4     WAIT        ; pause here until the sender raises READY
5     IN          ; A = byte from input pins
6     XOR key     ; encrypt (or decrypt - XOR undoes itself)
7     OUT         ; show the result (VALID pulses)
8     JMP 0       ; again
...
14    step: 0x5B  ; any odd number
15    key:  0xA7  ; the secret starting key
```

11 of the 16 bytes used. This cipher is weak on purpose: breaking it is the
follow-up exercise.

## Decisions so far

- ROL (rotate) rather than a plain shift - agreed by Aaron.
- Realistic input timing with `WAIT` + READY pin - chosen by Aaron.

## Open question for Aaron

- 0xE is still free. Any instruction of your own you want there?
