`timescale 1ns / 1ps
module zbp_tb;

    import "DPI-C" context task dpi_tb_init();
    import "DPI-C" context task dpi_tb_verify();

    localparam int MAX_CYCLES = 100000;

    logic clk = 0;
    logic rst;
    logic program_done;

    tb_top dut(.*);

    int cycles = 0;
    always #5ns clk = ~clk;

    initial begin
        $dumpfile("dump.vcd");
        $dumpvars(0, dut);
        // forever #5ns clk = ~clk;

        $timeformat(-9, 0, " ns", 6);
        $display("Starting simulation...\n");

        RESET();
        dpi_tb_init();

        // repeat (MAX_CYCLES) @(posedge clk);
        forever @(posedge clk);

        $display("Simulation Ended after %d clk cycles", cycles);

        dpi_tb_verify();

        $finish;
    end

    always_ff @(posedge clk) begin
        if (program_done) begin
            $display("PROGRAM DONE FLAG TRIGGERED");
            $display("Simulation Ended after %d clk cycles", cycles);

            dpi_tb_verify();

            $finish;
        end
    end

    always_ff @(posedge clk) begin
        cycles <= cycles + 1;
    end

    task RESET();
        rst <= 1;
        repeat (2) @(posedge clk);
        rst <= 0;
    endtask

endmodule : zbp_tb
