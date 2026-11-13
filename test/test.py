# Tiny Tapeout test (cocotb). Same checks as tb_local.v, written in Python.
# GitHub runs this on every push, and again on the synthesized gates.

import os
import sys

import cocotb
from cocotb.clock import Clock
from cocotb.triggers import ClockCycles, FallingEdge

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..")
sys.path.insert(0, os.path.join(ROOT, "tools"))
from asm import assemble  # noqa: E402  (our own assembler)

# uio_in bits
LOAD = 0x10
WRITE = 0x20
READY = 0x40


async def reset(dut):
    clock = Clock(dut.clk, 1, unit="us")   # 1 MHz
    cocotb.start_soon(clock.start())
    dut.ena.value = 1
    dut.ui_in.value = 0
    dut.uio_in.value = 0
    dut.rst_n.value = 0
    await ClockCycles(dut.clk, 5)
    dut.rst_n.value = 1


async def load_program(dut, name):
    """Assemble programs/<name>.asm and write its 16 bytes through the load pins."""
    with open(os.path.join(ROOT, "programs", name + ".asm"), encoding="utf-8") as f:
        memory, _ = assemble(f.read())
    for addr, byte in enumerate(memory):
        await FallingEdge(dut.clk)
        dut.ui_in.value = byte
        dut.uio_in.value = LOAD | WRITE | addr
    await FallingEdge(dut.clk)
    dut.ui_in.value = 0
    dut.uio_in.value = 0          # LOAD low: the CPU starts running


async def wait_valid(dut, timeout=200):
    """Wait for the VALID pulse and return the output byte."""
    for _ in range(timeout):
        await FallingEdge(dut.clk)
        if (int(dut.uio_out.value) >> 7) & 1:
            return int(dut.uo_out.value)
    raise AssertionError("timed out waiting for VALID")


async def send_byte(dut, value):
    """Hand one byte to the chip the way a real sender would."""
    await FallingEdge(dut.clk)
    dut.ui_in.value = value
    dut.uio_in.value = READY
    result = await wait_valid(dut)
    dut.uio_in.value = 0
    await ClockCycles(dut.clk, 4)
    return result


def reference_cipher(data, key=0xA7, step=0x5B):
    """The same cipher as programs/cipher.asm, in Python."""
    out = []
    for byte in data:
        key = (((key << 1) | (key >> 7)) + step) & 0xFF   # ROL, then ADD step
        out.append(byte ^ key)
    return out


@cocotb.test()
async def test_cipher_roundtrip(dut):
    plain = list(b"HELLO")

    await reset(dut)
    await load_program(dut, "cipher")
    encrypted = [await send_byte(dut, b) for b in plain]
    dut._log.info("encrypted: " + " ".join(f"{b:02x}" for b in encrypted))
    assert encrypted == reference_cipher(plain)
    assert int(dut.uio_oe.value) == 0x80

    await load_program(dut, "cipher")      # reloading resets the key
    decrypted = [await send_byte(dut, b) for b in encrypted]
    assert decrypted == plain


@cocotb.test()
async def test_count_program(dut):
    """LDI, SUB, JZ, JC and HLT: expect 03 02 01 E0, then nothing more."""
    await reset(dut)
    await load_program(dut, "count")
    outputs = [await wait_valid(dut) for _ in range(4)]
    assert outputs == [0x03, 0x02, 0x01, 0xE0]

    # After HLT the CPU must stay quiet.
    for _ in range(60):
        await FallingEdge(dut.clk)
        assert (int(dut.uio_out.value) >> 7) & 1 == 0, "CPU kept running after HLT"
