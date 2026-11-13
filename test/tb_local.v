// ============================================================================
// tb.v  -  testbench (simulation only, never becomes hardware)
//
// PLAIN LANGUAGE:
//   A testbench is a pretend world around the chip. It wiggles the chip's
//   input pins the way a real user would, watches the output pins, and
//   complains if they are wrong. Because it only runs in the simulator, it is
//   allowed to use things real hardware cannot (delays, $display, files).
//
//   Test 1: load cipher.asm, encrypt "HELLO", compare every byte against the
//           same cipher worked out here in the testbench.
//   Test 2: load the program again (this resets the key), feed the encrypted
//           bytes back in, and check "HELLO" comes out.
//   Test 3: load count.asm and check it outputs 03, 02, 01, E0 then halts.
//
// Run with:  python tools/run_sim.py      (then open build/tb.vcd in GTKWave)
// ============================================================================
`default_nettype none
`timescale 1ns / 1ps

module tb_local;

    // ---- the chip's pins ----
    reg  [7:0] ui_in  = 8'h00;
    wire [7:0] uo_out;
    reg  [7:0] uio_in = 8'h00;
    wire [7:0] uio_out;
    wire [7:0] uio_oe;
    reg        clk    = 1'b0;
    reg        rst_n  = 1'b0;

    wire valid = uio_out[7];

    tt_um_bunny_sysd_cipher_cpu dut (
        .ui_in   (ui_in),
        .uo_out  (uo_out),
        .uio_in  (uio_in),
        .uio_out (uio_out),
        .uio_oe  (uio_oe),
        .ena     (1'b1),
        .clk     (clk),
        .rst_n   (rst_n)
    );

    // 10 ns clock period
    always #5 clk = ~clk;

    // ---- programs, produced by tools/asm.py ----
    reg [7:0] prog_cipher [0:15];
    reg [7:0] prog_count  [0:15];

    reg [7:0] plain  [0:4];
    reg [7:0] cipher [0:4];

    integer   errors = 0;
    integer   i;
    integer   timeout;
    reg [7:0] got;
    reg [7:0] key;

    // Write 16 bytes into the chip's memory through the load pins.
    task load_program(input which);   // 0 = cipher, 1 = count
        integer n;
        begin
            for (n = 0; n < 16; n = n + 1) begin
                @(negedge clk);
                ui_in  = which ? prog_count[n] : prog_cipher[n];
                uio_in = {2'b00, 1'b1, 1'b1, n[3:0]};   // WRITE=1, LOAD=1, address
            end
            @(negedge clk);
            uio_in = 8'h00;                             // LOAD=0: CPU starts
            ui_in  = 8'h00;
        end
    endtask

    // Wait for the VALID pulse and grab the output byte.
    task wait_valid(output [7:0] value);
        begin
            timeout = 0;
            @(posedge clk);
            while (!valid && timeout < 200) begin
                @(posedge clk);
                timeout = timeout + 1;
            end
            if (!valid) begin
                $display("FAIL: timed out waiting for VALID");
                errors = errors + 1;
            end
            value = uo_out;
        end
    endtask

    // Send one byte the way a real sender would, and collect the result.
    task send_byte(input [7:0] value, output [7:0] result);
        begin
            @(negedge clk);
            ui_in     = value;
            uio_in[6] = 1'b1;          // READY up
            wait_valid(result);
            @(negedge clk);
            uio_in[6] = 1'b0;          // READY down
            repeat (4) @(negedge clk);
        end
    endtask

    task check(input [7:0] actual, input [7:0] expected, input [8*8-1:0] what);
        begin
            if (actual !== expected) begin
                $display("FAIL: %0s: got %02h, expected %02h", what, actual, expected);
                errors = errors + 1;
            end else begin
                $display("  ok: %0s = %02h", what, actual);
            end
        end
    endtask

    initial begin
        $dumpfile("build/tb.vcd");
        $dumpvars(0, tb_local);
        // Memory boxes are not dumped automatically, so add them by hand.
        for (i = 0; i < 16; i = i + 1)
            $dumpvars(0, dut.u_mem.mem[i]);

        $readmemh("build/cipher.hex", prog_cipher);
        $readmemh("build/count.hex",  prog_count);

        plain[0] = "H"; plain[1] = "E"; plain[2] = "L"; plain[3] = "L"; plain[4] = "O";

        // ---- reset ----
        repeat (3) @(negedge clk);
        rst_n = 1'b1;

        // ---- Test 1: encrypt ----
        $display("Test 1: encrypt HELLO");
        load_program(1'b0);
        key = 8'hA7;                                   // same start key as cipher.asm
        for (i = 0; i < 5; i = i + 1) begin
            key = {key[6:0], key[7]} + 8'h5B;          // same stirring as the program
            send_byte(plain[i], got);
            cipher[i] = got;
            check(got, plain[i] ^ key, "cipher");
        end

        // ---- Test 2: decrypt ----
        $display("Test 2: decrypt back to HELLO");
        load_program(1'b0);                            // reloading resets the key
        for (i = 0; i < 5; i = i + 1) begin
            send_byte(cipher[i], got);
            check(got, plain[i], "plain");
        end

        // ---- Test 3: the other instructions ----
        $display("Test 3: count program (LDI, SUB, JZ, JC, HLT)");
        load_program(1'b1);
        wait_valid(got); check(got, 8'h03, "count");
        wait_valid(got); check(got, 8'h02, "count");
        wait_valid(got); check(got, 8'h01, "count");
        wait_valid(got); check(got, 8'hE0, "carry");
        repeat (40) @(posedge clk);
        if (dut.halted !== 1'b1) begin
            $display("FAIL: CPU did not halt");
            errors = errors + 1;
        end else begin
            $display("  ok: CPU halted");
        end
        if (uio_oe !== 8'h80) begin
            $display("FAIL: uio_oe is %02h, expected 80", uio_oe);
            errors = errors + 1;
        end

        if (errors == 0)
            $display("ALL TESTS PASSED");
        else
            $display("TESTS FAILED: %0d error(s)", errors);
        $finish;
    end

endmodule
