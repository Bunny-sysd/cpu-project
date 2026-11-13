// ============================================================================
// control_unit.v
//
// PLAIN LANGUAGE:
//   All the other modules are dumb parts: a counter, a calculator, some
//   boxes. The control unit is the conductor. It looks at the opcode and
//   switches on exactly the right parts at the right moment.
//
//   Every instruction takes two clock ticks:
//
//     FETCH   - "get the instruction": memory shows the byte at address PC,
//               the instruction register grabs it, PC moves on by one.
//     EXECUTE - "do it": memory now shows the byte at the instruction's
//               operand address, and the control unit raises the signals
//               that instruction needs (write A? write memory? jump?).
//
//   Then back to FETCH, forever. Two special cases:
//     HLT  -> go to the HALT state and stay there until reset.
//     WAIT -> stay in EXECUTE until the wrapper says a new byte has arrived.
//
//   The big "case (opcode)" below is the DECODER: one entry per instruction
//   in docs/ISA.md. If you add an instruction, this is where it goes.
// ============================================================================
`default_nettype none

module control_unit (
    input  wire       clk,
    input  wire       rst,            // 1 = go back to FETCH
    input  wire [3:0] opcode,         // from the instruction register
    input  wire       z,              // zero flag
    input  wire       c,              // carry flag
    input  wire       ready_pending,  // 1 = a new input byte is waiting

    output wire       fetch,          // 1 during FETCH
    output reg  [2:0] alu_op,         // which calculation (see alu.v)
    output reg  [1:0] b_sel,          // ALU's b input: 0 memory, 1 operand, 2 input pins
    output reg        acc_we,         // store ALU result in A (and update Z)
    output reg        c_we,           // update the carry flag
    output reg        mem_we,         // write A into memory
    output reg        out_we,         // copy A to the output pins
    output reg        pc_load,        // jump
    output reg        wait_ack,       // "I used the waiting byte"
    output wire       halted          // 1 = stopped by HLT
);

    // ---- opcodes: must match docs/ISA.md and tools/asm.py ----
    localparam OP_NOP  = 4'h0;
    localparam OP_LDA  = 4'h1;
    localparam OP_LDI  = 4'h2;
    localparam OP_STA  = 4'h3;
    localparam OP_ADD  = 4'h4;
    localparam OP_SUB  = 4'h5;
    localparam OP_XOR  = 4'h6;
    localparam OP_ROL  = 4'h7;
    localparam OP_IN   = 4'h8;
    localparam OP_OUT  = 4'h9;
    localparam OP_JMP  = 4'hA;
    localparam OP_JZ   = 4'hB;
    localparam OP_JC   = 4'hC;
    localparam OP_WAIT = 4'hD;
    localparam OP_HLT  = 4'hF;

    // ---- ALU operations: must match alu.v ----
    localparam ALU_PASS = 3'd0;
    localparam ALU_ADD  = 3'd1;
    localparam ALU_SUB  = 3'd2;
    localparam ALU_XOR  = 3'd3;
    localparam ALU_ROL  = 3'd4;

    // ---- where the ALU's b input comes from ----
    localparam B_MEM = 2'd0;   // the byte memory is showing
    localparam B_IMM = 2'd1;   // the instruction's own operand (LDI)
    localparam B_PIN = 2'd2;   // the input pins (IN)

    // ---- the three states ----
    localparam S_FETCH = 2'd0;
    localparam S_EXEC  = 2'd1;
    localparam S_HALT  = 2'd2;

    reg [1:0] state;
    reg [1:0] next_state;

    assign fetch  = (state == S_FETCH);
    assign halted = (state == S_HALT);

    // The state register: the only part of this module that remembers anything.
    always @(posedge clk) begin
        if (rst)
            state <= S_FETCH;
        else
            state <= next_state;
    end

    // Everything below is combinational: "given the state and the opcode,
    // which switches are on right now, and which state comes next?"
    always @(*) begin
        // Defaults: everything off. Each instruction only turns on what it needs.
        alu_op     = ALU_PASS;
        b_sel      = B_MEM;
        acc_we     = 1'b0;
        c_we       = 1'b0;
        mem_we     = 1'b0;
        out_we     = 1'b0;
        pc_load    = 1'b0;
        wait_ack   = 1'b0;
        next_state = S_FETCH;

        case (state)
            S_FETCH: next_state = S_EXEC;

            S_HALT:  next_state = S_HALT;

            S_EXEC: begin
                case (opcode)
                    OP_LDA: begin acc_we = 1'b1; end                    // A = mem
                    OP_LDI: begin acc_we = 1'b1; b_sel = B_IMM; end     // A = operand
                    OP_IN:  begin acc_we = 1'b1; b_sel = B_PIN; end     // A = pins
                    OP_STA: begin mem_we = 1'b1; end                    // mem = A
                    OP_ADD: begin acc_we = 1'b1; c_we = 1'b1; alu_op = ALU_ADD; end
                    OP_SUB: begin acc_we = 1'b1; c_we = 1'b1; alu_op = ALU_SUB; end
                    OP_XOR: begin acc_we = 1'b1; alu_op = ALU_XOR; end
                    OP_ROL: begin acc_we = 1'b1; c_we = 1'b1; alu_op = ALU_ROL; end
                    OP_OUT: begin out_we = 1'b1; end
                    OP_JMP: begin pc_load = 1'b1; end
                    OP_JZ:  begin pc_load = z; end                      // jump only if Z
                    OP_JC:  begin pc_load = c; end                      // jump only if C
                    OP_WAIT: begin
                        if (ready_pending)
                            wait_ack = 1'b1;        // byte is here: carry on
                        else
                            next_state = S_EXEC;    // not yet: stay right here
                    end
                    OP_HLT: next_state = S_HALT;
                    default: ;                      // NOP and unused opcodes
                endcase
            end

            default: next_state = S_FETCH;
        endcase
    end

endmodule
