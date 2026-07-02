module tb_top(
    input logic clk,
    input logic rst
);

    import zbp_pkg::imem_req_t;
    import zbp_pkg::imem_rsp_t;

    pipeline_if#(.T(imem_req_t)) req_if();
    pipeline_if#(.T(imem_rsp_t)) rsp_if();

    imem #(
        .INIT_FILE("../../fw/output.vh"),
        .MEM_SIZE_WORDS(1024),
        .RANDOM_STALLS(1'b0)
    ) imem_inst (
        .clk(clk),
        .rst(rst),

        .req_if(req_if),
        .rsp_if(rsp_if)
    );

    zbp_core cpu_core_inst(
        .clk(clk),
        .rst(rst),

        .imem_req_if(req_if),
        .imem_rsp_if(rsp_if)
    );

    final begin
        $display("Simulation ended. Dumping Scalar Register...");
        $writememh("scalar_regs_dump.hex", cpu_core_inst.regfile_inst.sregfile_inst.regs);
    end

endmodule : tb_top
