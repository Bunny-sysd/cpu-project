#!/usr/bin/env python3
"""asm.py - assembler for the cipher CPU (see docs/ISA.md).

Turns a text program into the 16 bytes that go into the CPU's memory.

    python tools/asm.py programs/cipher.asm build/cipher.hex

Source syntax:
    ; anything after a semicolon is a comment
    loop:               a label = a name for "the address of the next thing"
    LDA key             mnemonic + operand (a label or a number)
    LDI 0x0F            numbers: 12, 0x0C or 0b1100
    .org 14             continue placing things from address 14
    key: .byte 0xA7     put a raw data byte here

Output: 16 lines, one hex byte per line (address 0 first). Unused addresses
are 00. This is the format Verilog's $readmemh reads.
"""
import sys

MEM_SIZE = 16

# mnemonic -> (opcode, takes_operand).  Must match src/control_unit.v.
OPCODES = {
    "NOP":  (0x0, False),
    "LDA":  (0x1, True),
    "LDI":  (0x2, True),
    "STA":  (0x3, True),
    "ADD":  (0x4, True),
    "SUB":  (0x5, True),
    "XOR":  (0x6, True),
    "ROL":  (0x7, False),
    "IN":   (0x8, False),
    "OUT":  (0x9, False),
    "JMP":  (0xA, True),
    "JZ":   (0xB, True),
    "JC":   (0xC, True),
    "WAIT": (0xD, False),
    "HLT":  (0xF, False),
}


class AsmError(Exception):
    pass


def parse_number(text):
    """'12', '0x0C' or '0b1100' -> int, or None if it is not a number."""
    try:
        return int(text, 0)
    except ValueError:
        return None


def assemble(source):
    """Return (list of 16 ints, listing lines)."""
    # ---- pass 1: work out the address of every item and every label ----
    items = []    # (address, line_number, mnemonic_or_directive, operand_text)
    labels = {}
    addr = 0
    for lineno, raw in enumerate(source.splitlines(), start=1):
        line = raw.split(";", 1)[0].strip()
        if not line:
            continue

        # A label is "name:" at the start of the line.
        if ":" in line:
            name, line = line.split(":", 1)
            name, line = name.strip(), line.strip()
            if not name.isidentifier():
                raise AsmError(f"line {lineno}: bad label name '{name}'")
            if name in labels:
                raise AsmError(f"line {lineno}: label '{name}' defined twice")
            labels[name] = addr
            if not line:
                continue

        parts = line.split()
        word = parts[0].upper()
        operand = parts[1] if len(parts) > 1 else None
        if len(parts) > 2:
            raise AsmError(f"line {lineno}: too many things on one line")

        if word == ".ORG":
            value = parse_number(operand or "")
            if value is None or not 0 <= value < MEM_SIZE:
                raise AsmError(f"line {lineno}: .org needs an address 0-{MEM_SIZE - 1}")
            addr = value
            continue

        if word != ".BYTE" and word not in OPCODES:
            raise AsmError(f"line {lineno}: unknown instruction '{parts[0]}'")
        if addr >= MEM_SIZE:
            raise AsmError(f"line {lineno}: program does not fit in {MEM_SIZE} bytes")
        items.append((addr, lineno, word, operand))
        addr += 1

    # ---- pass 2: now that all labels are known, build the bytes ----
    memory = [None] * MEM_SIZE
    listing = []
    for addr, lineno, word, operand in items:
        if memory[addr] is not None:
            raise AsmError(f"line {lineno}: address {addr} is used twice")

        if word == ".BYTE":
            value = parse_number(operand or "")
            if value is None or not 0 <= value <= 0xFF:
                raise AsmError(f"line {lineno}: .byte needs a number 0-255")
            byte = value
        else:
            opcode, takes_operand = OPCODES[word]
            if takes_operand:
                if operand is None:
                    raise AsmError(f"line {lineno}: {word} needs an operand")
                value = labels.get(operand, parse_number(operand))
                if value is None:
                    raise AsmError(f"line {lineno}: unknown label '{operand}'")
                if not 0 <= value <= 0xF:
                    raise AsmError(f"line {lineno}: operand {value} does not fit in 4 bits (0-15)")
            else:
                if operand is not None:
                    raise AsmError(f"line {lineno}: {word} takes no operand")
                value = 0
            byte = (opcode << 4) | value

        memory[addr] = byte
        text = word if operand is None else f"{word} {operand}"
        listing.append(f"  {addr:2d}: {byte:02X}   {text}")

    return [b if b is not None else 0 for b in memory], listing


def main(argv):
    if len(argv) != 3:
        print("usage: python tools/asm.py <program.asm> <output.hex>")
        return 2
    try:
        with open(argv[1], encoding="utf-8") as f:
            memory, listing = assemble(f.read())
    except (AsmError, OSError) as e:
        print(f"asm error: {e}")
        return 1
    with open(argv[2], "w", encoding="ascii") as f:
        for byte in memory:
            f.write(f"{byte:02X}\n")
    print(f"{argv[1]} -> {argv[2]}")
    print("\n".join(listing))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
