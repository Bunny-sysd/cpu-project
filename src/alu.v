// ============================================================================
// alu.v  -  Arithmetic Logic Unit
//
// PLAIN LANGUAGE:
//   The ALU is the calculator. It has no memory and no clock: values go in,
//   the answer comes straight out (this is called "combinational" logic).
//
//   Inputs:
//     a  = the accumulator's current value
//     b  = the other number (from memory, from the instruction, or from the
//          input pins - the top-level wrapper picks which)
//     op = which calculation to do
//
//   Outputs:
//     y     = the answer (this becomes the new accumulator value)
//     carry = the "9th bit":
//               ADD -> 1 if the sum did not fit in 8 bits   (200 + 100)
//               SUB -> 1 if we had to borrow, i.e. a < b    (5 - 9)
//               ROL -> the bit that wrapped round from bit 7 to bit 0
//
//   op codes (must match control_unit.v):
//     0 PASS  y = b            (used by LDA, LDI, IN: "just load b into A")
//     1 ADD   y = a + b
//     2 SUB   y = a - b
//     3 XOR   y = a ^ b
//     4 ROL   y = a rotated left by one bit
//
// EXERCISE FOR AARON (after the chip works):
//   Delete everything inside the "case" below, write it yourself, and run
//   `python tools/run_sim.py`. The testbench tells you if you got it right.
// ============================================================================
`default_nettype none

module alu (
    input  wire [2:0] op,
    input  wire [7:0] a,
    input  wire [7:0] b,
    output reg  [7:0] y,
    output reg        carry
);

    localparam ALU_PASS = 3'd0;
    localparam ALU_ADD  = 3'd1;
    localparam ALU_SUB  = 3'd2;
    localparam ALU_XOR  = 3'd3;
    localparam ALU_ROL  = 3'd4;

    always @(*) begin
        // Defaults first, so every output always has a value
        // (otherwise the synthesizer would create unwanted latches).
        y     = b;
        carry = 1'b0;

        case (op)
            // Do the sum in 9 bits; the extra top bit IS the carry.
            ALU_ADD: {carry, y} = {1'b0, a} + {1'b0, b};

            // Same trick: if a < b the 9-bit result goes "negative" and the
            // top bit becomes 1. That is the borrow.
            ALU_SUB: {carry, y} = {1'b0, a} - {1'b0, b};

            ALU_XOR: y = a ^ b;

            // Rotate: bits 6..0 move up one place, old bit 7 drops into bit 0.
            ALU_ROL: begin
                y     = {a[6:0], a[7]};
                carry = a[7];
            end

            default: y = b;   // ALU_PASS and unused codes
        endcase
    end

endmodule
