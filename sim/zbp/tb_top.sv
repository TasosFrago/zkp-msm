module tb_top(
    input logic clk,
    input logic rst
);

    import zbp_pkg::imem_req_t;
    import zbp_pkg::imem_rsp_t;
    import zbp_pkg::dmem_req_t;
    import zbp_pkg::dmem_rsp_t;

    pipeline_if#(.T(imem_req_t)) imem_req_if();
    pipeline_if#(.T(imem_rsp_t)) imem_rsp_if();

    pipeline_if#(.T(dmem_req_t)) dmem_req_if();
    pipeline_if#(.T(dmem_rsp_t)) dmem_rsp_if();

    imem #(
        .INIT_FILE("../../fw/output.vh"),
        .MEM_SIZE_WORDS(1024),
        .RANDOM_STALLS(1'b0)
    ) imem_inst (
        .clk(clk),
        .rst(rst),

        .req_if(imem_req_if),
        .rsp_if(imem_rsp_if)
    );

    dmem #(
        .INIT_FILE(""),
        .MEM_SIZE_BYTES(65536),
        .MIN_LATENCY(1),
        .MAX_RANDOM_DELAY(5),
        .RANDOM_STALL_PROB(20),
        .MAX_PENDING_REQS(8)
    ) dmem_inst (
        .clk(clk),
        .rst(rst),

        .req_if(dmem_req_if),
        .rsp_if(dmem_rsp_if)
    );

    zbp_core cpu_core_inst(
        .clk(clk),
        .rst(rst),

        .imem_req_if(imem_req_if),
        .imem_rsp_if(imem_rsp_if),

        .dmem_req_if(dmem_req_if),
        .dmem_rsp_if(dmem_rsp_if)
    );


    function void dump_sregs(input string filename);
        int fd;
        int idx;
        static string abi_names[32] = '{
            "zero", "ra", "sp", "gp", "tp", "t0", "t1", "t2",
            "s0",   "s1", "a0", "a1", "a2", "a3", "a4", "a5",
            "a6",   "a7", "s2", "s3", "s4", "s5", "s6", "s7",
            "s8",   "s9", "s10","s11","t3", "t4", "t5", "t6"
        };

        fd = $fopen(filename, "w");

        if (fd != 0) begin
            for (int t = 0; t < 32; t++) begin
                $fdisplay(fd, "========================================");
                $fdisplay(fd, "             THREAD ID: %0d             ", t);
                $fdisplay(fd, "========================================");

                for (int i = 0; i < 32; i++) begin
                    idx = (32 * t) + i;

                    $fdisplay(fd, "%-4s (x%0d)\t= 0x%08h | %0d",
                              abi_names[i],
                              i,
                              ((i == 0) ? 0 : (i == 4) ? t : cpu_core_inst.regfile_inst.sregfile_inst.regs[idx]),
                              ((i == 0) ? 0 : (i == 4) ? t : cpu_core_inst.regfile_inst.sregfile_inst.regs[idx])
                    );
                end
                $fdisplay(fd, "\n");
            end
            $fclose(fd);
            $display("Dump saved to scalar_regs_dump.txt");
        end else begin
            $error("Failed to open dump file!");
        end
    endfunction : dump_sregs

    final begin
        $display("Simulation ended. Dumping Multithreaded Scalar Registers...");
        dump_sregs("scalar_regs_dump.txt");
    end

endmodule : tb_top
