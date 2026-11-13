// ============================================================================
// memory.v
//
// PLAIN LANGUAGE:
//   16 boxes, each holding one byte, numbered 0..15. The program AND its data
//   live in these same 16 boxes.
//
//   Reading: put a box number on addr, and that box's contents appear on dout
//            immediately (no clock needed).
//   Writing: put the box number on addr, the new byte on din, set we = 1,
//            and the byte is stored on the next clock edge.
//
// WHY ONLY 16 BYTES:
//   On a chip like this there is no ready-made RAM block. Every single bit
//   is built from its own flip-flop, so this module alone is 128 flip-flops
//   plus the logic that selects one box out of 16. It is the biggest part of
//   the whole CPU. Doubling memory would roughly double that.
//
// NOTE: there is no reset here on purpose (it would cost extra cells). The
//   memory holds garbage at power-up until a program is loaded through the
//   load pins - see the top-level wrapper.
// ============================================================================
`default_nettype none

module memory (
    input  wire       clk,
    input  wire       we,     // 1 = write din into box number addr
    input  wire [3:0] addr,   // which box
    input  wire [7:0] din,    // byte to store
    output wire [7:0] dout    // contents of box number addr
);

    reg [7:0] mem [0:15];

    always @(posedge clk) begin
        if (we)
            mem[addr] <= din;
    end

    assign dout = mem[addr];

endmodule
