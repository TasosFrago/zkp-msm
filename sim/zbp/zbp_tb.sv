`timescale 1ns / 1ns
module zbp_tb;

    localparam int MAX_CYCLES = 1000;

    logic clk = 0;
    logic rst;

    tb_top dut(.*);

    always #5ns clk = ~clk;

    initial begin
        // $dumpfile("dump.vcd");
        // $dumpvars(0, dut);

        $timeformat(-9, 0, " ns", 6);
        $display("Starting simulation...\n");
        RESET();

        repeat (MAX_CYCLES) @(posedge clk);

        $finish;
    end

    task RESET();
        rst <= 1;
        repeat (2) @(posedge clk);
        rst <= 0;
    endtask

endmodule : zbp_tb
