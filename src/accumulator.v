// ============================================================================
// accumulator.v
//
// PLAIN LANGUAGE:
//   The accumulator (A) is the CPU's single working register: an 8-bit box
//   that holds the number we are currently working on. Every calculation is
//   "A = A (something) other value", so results "accumulate" here.
//
//   Next to it live two 1-bit notes about the last result, called FLAGS:
//     Z (zero)  = 1 if the value just stored in A was 0
//     C (carry) = the ALU's 9th bit from the last ADD / SUB / ROL
//   The jump instructions JZ and JC look at these notes to make decisions.
//   That is how a CPU does "if": compute something, then jump on a flag.
// ============================================================================
`default_nettype none

module accumulator (
    input  wire       clk,
    input  wire       rst,       // 1 = clear A and both flags
    input  wire       we,        // 1 = store din into A (Z updates too)
    input  wire       c_we,      // 1 = also store carry_in into C
    input  wire [7:0] din,       // new value, from the ALU
    input  wire       carry_in,  // carry, from the ALU
    output reg  [7:0] a,
    output reg        z,
    output reg        c
);

    always @(posedge clk) begin
        if (rst) begin
            a <= 8'h00;
            z <= 1'b0;
            c <= 1'b0;
        end else begin
            if (we) begin
                a <= din;
                z <= (din == 8'h00);
            end
            if (c_we)
                c <= carry_in;
        end
    end

endmodule
