// ============================================================================
// program_counter.v
//
// PLAIN LANGUAGE:
//   The program counter (PC) is a 4-bit number that says "which memory address
//   holds the next instruction". It is the CPU's bookmark.
//
//   On each clock edge it does ONE of these, in priority order:
//     1. rst  = 1  -> go back to address 0 (start of the program)
//     2. load = 1  -> jump: take the address given on load_addr (JMP/JZ/JC)
//     3. inc  = 1  -> move to the next address (normal case)
//     4. otherwise -> stay where it is
//
//   4 bits means it counts 0..15 and then wraps to 0, which matches our
//   16 bytes of memory exactly.
// ============================================================================
`default_nettype none

module program_counter (
    input  wire       clk,
    input  wire       rst,        // 1 = go to address 0
    input  wire       inc,        // 1 = step to the next address
    input  wire       load,       // 1 = jump to load_addr
    input  wire [3:0] load_addr,  // where to jump to
    output reg  [3:0] addr        // the current bookmark
);

    always @(posedge clk) begin
        if (rst)
            addr <= 4'd0;
        else if (load)
            addr <= load_addr;
        else if (inc)
            addr <= addr + 4'd1;
    end

endmodule
