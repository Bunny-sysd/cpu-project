// ============================================================================
// tt_um_bunny_sysd_cipher_cpu.v  -  top-level wrapper for Tiny Tapeout
//
// PLAIN LANGUAGE:
//   This file is the chip's outer shell. It does three jobs:
//     1. Wires the parts together (PC, IR, ALU, accumulator, memory, control).
//     2. Connects them to the chip's physical pins.
//     3. Adds the "load mode" so a program can be written into memory.
//
// PIN MAP
//   ui_in[7:0]   input byte. In load mode: the byte to store.
//                            When running: the byte the IN instruction reads.
//   uo_out[7:0]  output byte, set by the OUT instruction.
//   uio_in[3:0]  load mode: which memory address to write.
//   uio_in[4]    LOAD.  1 = freeze the CPU at address 0 and allow writing.
//   uio_in[5]    WRITE. 1 (with LOAD) = store ui_in at that address.
//   uio_in[6]    READY. Sender raises this when a new byte is on ui_in.
//   uio_out[7]   VALID. High for one clock when OUT has put a new result out.
//
// HOW THE SENDER USES IT
//   Put a byte on ui_in, raise READY, keep both steady until VALID pulses,
//   read uo_out, lower READY for a few clocks. Repeat.
//
// NOTE: LOAD, WRITE, the address and ui_in must change in step with clk
//   during loading (the testbench changes them on the falling edge).
// ============================================================================
`default_nettype none

module tt_um_bunny_sysd_cipher_cpu (
    input  wire [7:0] ui_in,    // dedicated inputs
    output wire [7:0] uo_out,   // dedicated outputs
    input  wire [7:0] uio_in,   // bidirectional pins: input path
    output wire [7:0] uio_out,  // bidirectional pins: output path
    output wire [7:0] uio_oe,   // bidirectional pins: 1 = output, 0 = input
    input  wire       ena,      // always 1 when the design is powered
    input  wire       clk,
    input  wire       rst_n     // reset, active LOW (0 = reset)
);

    // ---------------- pins -> friendly names ----------------
    wire       rst       = ~rst_n;        // inside the chip we use 1 = reset
    wire [3:0] load_addr = uio_in[3:0];
    wire       load      = uio_in[4];
    wire       load_we   = uio_in[5];
    wire       ready_pin = uio_in[6];

    // The CPU is held in reset while a program is being loaded.
    wire cpu_rst = rst | load;

    // ---------------- wires between the parts ----------------
    wire [3:0] pc_addr;
    wire [3:0] opcode;
    wire [3:0] operand;
    wire [7:0] a;
    wire       z;
    wire       c;
    wire [7:0] mem_dout;
    wire [7:0] alu_y;
    wire       alu_carry;

    wire       fetch;
    wire [2:0] alu_op;
    wire [1:0] b_sel;
    wire       acc_we;
    wire       c_we;
    wire       mem_we;
    wire       out_we;
    wire       pc_load;
    wire       wait_ack;
    wire       halted;

    // ---------------- READY handshake ----------------
    // READY comes from the outside world and can change at any moment, even
    // right on a clock edge, which can confuse a flip-flop. Passing it through
    // two flip-flops in a row first (a "synchronizer") makes it safe. The
    // third one remembers the previous value so we can spot a 0 -> 1 change.
    reg [2:0] ready_sync;
    always @(posedge clk) begin
        if (rst)
            ready_sync <= 3'b000;
        else
            ready_sync <= {ready_sync[1:0], ready_pin};
    end
    wire ready_rise = ready_sync[1] & ~ready_sync[2];

    // "A byte is waiting" note. Set when READY rises, cleared when WAIT uses it.
    reg ready_pending;
    always @(posedge clk) begin
        if (cpu_rst)
            ready_pending <= 1'b0;
        else if (ready_rise)
            ready_pending <= 1'b1;
        else if (wait_ack)
            ready_pending <= 1'b0;
    end

    // ---------------- who is using memory right now? ----------------
    //   load mode : the outside world writes a program in
    //   FETCH     : read the instruction at address PC
    //   EXECUTE   : read / write the data at the instruction's operand address
    wire [3:0] mem_addr = load  ? load_addr :
                          fetch ? pc_addr   :
                                  operand;
    wire [7:0] mem_din  = load ? ui_in   : a;
    wire       mem_wr   = load ? load_we : mem_we;

    // ---------------- what goes into the ALU's b input? ----------------
    wire [7:0] alu_b = (b_sel == 2'd1) ? {4'b0000, operand} :  // LDI constant
                       (b_sel == 2'd2) ? ui_in              :  // IN
                                         mem_dout;             // everything else

    // ---------------- the parts ----------------
    program_counter u_pc (
        .clk       (clk),
        .rst       (cpu_rst),
        .inc       (fetch),      // step forward every FETCH
        .load      (pc_load),
        .load_addr (operand),
        .addr      (pc_addr)
    );

    instruction_register u_ir (
        .clk     (clk),
        .rst     (cpu_rst),
        .we      (fetch),        // grab the instruction every FETCH
        .din     (mem_dout),
        .opcode  (opcode),
        .operand (operand)
    );

    control_unit u_ctrl (
        .clk           (clk),
        .rst           (cpu_rst),
        .opcode        (opcode),
        .z             (z),
        .c             (c),
        .ready_pending (ready_pending),
        .fetch         (fetch),
        .alu_op        (alu_op),
        .b_sel         (b_sel),
        .acc_we        (acc_we),
        .c_we          (c_we),
        .mem_we        (mem_we),
        .out_we        (out_we),
        .pc_load       (pc_load),
        .wait_ack      (wait_ack),
        .halted        (halted)
    );

    alu u_alu (
        .op    (alu_op),
        .a     (a),
        .b     (alu_b),
        .y     (alu_y),
        .carry (alu_carry)
    );

    accumulator u_acc (
        .clk      (clk),
        .rst      (cpu_rst),
        .we       (acc_we),
        .c_we     (c_we),
        .din      (alu_y),
        .carry_in (alu_carry),
        .a        (a),
        .z        (z),
        .c        (c)
    );

    memory u_mem (
        .clk  (clk),
        .we   (mem_wr),
        .addr (mem_addr),
        .din  (mem_din),
        .dout (mem_dout)
    );

    // ---------------- output pins ----------------
    reg [7:0] out_reg;   // holds the last value OUT sent
    reg       valid;     // one-clock pulse: "out_reg just changed"
    always @(posedge clk) begin
        if (cpu_rst) begin
            out_reg <= 8'h00;
            valid   <= 1'b0;
        end else begin
            valid <= out_we;
            if (out_we)
                out_reg <= a;
        end
    end

    assign uo_out  = out_reg;
    assign uio_out = {valid, 7'b0000000};
    assign uio_oe  = 8'b1000_0000;   // only bit 7 (VALID) is an output

    // Tell the tools these are unused on purpose (avoids warnings).
    wire _unused = &{ena, uio_in[7], halted, 1'b0};

endmodule
