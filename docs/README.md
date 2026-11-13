# Cipher CPU

A tiny 8-bit accumulator CPU for Tiny Tapeout. It runs a 16-byte program; the
demo program is a toy stream cipher: a byte goes in, the encrypted byte comes out.

The instruction set is in [ISA.md](ISA.md).

## Run the simulation

```
python tools/run_sim.py
gtkwave build/tb.vcd
```

`run_sim.py` assembles the programs, compiles the Verilog with Icarus Verilog,
runs the testbench and prints `ALL TESTS PASSED` if everything works.

## Where things are

| Path                          | What it is                                        |
|-------------------------------|---------------------------------------------------|
| `src/program_counter.v`       | the bookmark: address of the next instruction     |
| `src/instruction_register.v`  | holds the instruction being executed              |
| `src/memory.v`                | 16 bytes, program and data together               |
| `src/alu.v`                   | the calculator: add, subtract, xor, rotate        |
| `src/accumulator.v`           | the A register and the Z / C flags                |
| `src/control_unit.v`          | fetch/execute state machine and instruction decoder |
| `src/tt_um_bunny_sysd_cipher_cpu.v`      | top level: wires the parts to the chip's pins     |
| `test/tb_local.v`             | local testbench (Icarus only, makes the VCD)      |
| `test/tb.v`, `test/test.py`   | Tiny Tapeout testbench (cocotb), run by GitHub    |
| `tools/asm.py`                | assembler: `.asm` text -> 16 hex bytes            |
| `tools/run_sim.py`            | one command to build and test everything          |
| `programs/cipher.asm`         | the cipher                                        |
| `programs/count.asm`          | test program for the remaining instructions       |

## Pins

| Pin           | Direction | Meaning                                              |
|---------------|-----------|------------------------------------------------------|
| `ui_in[7:0]`  | in        | byte to load (load mode) or byte to encrypt (running) |
| `uo_out[7:0]` | out       | result byte, set by `OUT`                            |
| `uio[3:0]`    | in        | load mode: memory address to write                   |
| `uio[4]`      | in        | LOAD: 1 = CPU frozen at address 0, memory writable   |
| `uio[5]`      | in        | WRITE: 1 = store `ui_in` at that address             |
| `uio[6]`      | in        | READY: raise when a new byte is on `ui_in`           |
| `uio[7]`      | out       | VALID: one-clock pulse when `uo_out` has a new result |

## ALU exercise

`src/alu.v` works as shipped. To learn it properly: delete the body of its
`case` statement, write it yourself, and run `python tools/run_sim.py` until
the tests pass again.

## Tiny Tapeout build

The repo is laid out like the Tiny Tapeout GF template (`info.yaml`,
`.github/workflows/`, `test/test.py`). Every push to GitHub runs their free
build under the repo's **Actions** tab: it runs the tests, turns the Verilog
into a chip layout, and reports how many cells the design uses. Nothing is
submitted or paid for unless you do that yourself on tinytapeout.com.
