// ============================================================================
// instruction_register.v
//
// PLAIN LANGUAGE:
//   Memory can only show one byte at a time. During an instruction we need
//   memory twice: once to read the instruction itself, and once to read or
//   write the data it talks about. So we need somewhere to REMEMBER the
//   instruction while memory is busy with the data. That is the instruction
//   register (IR): an 8-bit box that holds the instruction being executed.
//
//   The byte is split into two halves:
//     opcode  = top 4 bits    -> WHAT to do   (goes to the control unit)
//     operand = bottom 4 bits -> WHERE / WHAT with (an address or a constant)
// ============================================================================
`default_nettype none

module instruction_register (
    input  wire       clk,
    input  wire       rst,      // 1 = clear to 0x00, which is NOP
    input  wire       we,       // 1 = capture the byte on din
    input  wire [7:0] din,      // byte coming out of memory
    output wire [3:0] opcode,   // top half
    output wire [3:0] operand   // bottom half
);

    reg [7:0] ir;

    always @(posedge clk) begin
        if (rst)
            ir <= 8'h00;
        else if (we)
            ir <= din;
    end

    assign opcode  = ir[7:4];
    assign operand = ir[3:0];

endmodule
