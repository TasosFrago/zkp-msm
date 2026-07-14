module tb_top(
    input logic clk,
    input logic rst,

    output logic program_done
);

    import zbp_pkg::imem_req_t;
    import zbp_pkg::imem_rsp_t;
    import zbp_pkg::dmem_req_t;
    import zbp_pkg::dmem_rsp_t;
    import dbg_pkg::*;

    pipeline_if#(.T(imem_req_t)) imem_req_if();
    pipeline_if#(.T(imem_rsp_t)) imem_rsp_if();

    pipeline_if#(.T(dmem_req_t)) dmem_req_if();
    pipeline_if#(.T(dmem_rsp_t)) dmem_rsp_if();

    imem #(
        .INIT_FILE("../../fw/imem.vh"),
        .MEM_SIZE_WORDS(1024),
        .RANDOM_STALLS(1'b0)
    ) imem_inst (
        .clk(clk),
        .rst(rst),

        .req_if(imem_req_if),
        .rsp_if(imem_rsp_if)
    );

    dmem #(
        .INIT_FILE("../../fw/dmem.vh"),
        .OUTPUT_FILE("dmem_dump.txt"),
        .OUTPUT_HEX_FILE("dmem_dump.hex"),
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
        .dmem_rsp_if(dmem_rsp_if),

        .program_done(program_done)
    );

    bind fetch dbg_fetch_trace_if trace_if (
        .clk(clk),
        .rst(rst),
        .fetch_valid(fetch_if.valid),
        .fetch_ready(fetch_if.ready),
        .fetch_instr(fetch_if.data.instr),
        .fetch_tid(fetch_if.data.tid),
        .cf_redirect(cf_redirect),
        .cf_pc_adv(cf_pc_adv),
        .pc_tb(pc_tb)
    );


    FetchMonitor instr_monitor;
    PcHeartbeatMonitor heartbeatPC_mon;

    initial begin
        instr_monitor = new(cpu_core_inst.fetch_inst.trace_if, "traces");
        heartbeatPC_mon = new(cpu_core_inst.fetch_inst.trace_if, 100000);

        $display("Starting Fetch Instruction Monitor.");
        fork
            instr_monitor.run();
            heartbeatPC_mon.run();
        join_none
    end

    always_ff @(posedge clk) begin
        if (~rst & program_done) begin
            $display("Program DONE!");
            $finish();
        end
    end

    final begin
        $display("Simulation ended. Dumping Multithreaded Scalar Registers...");
        dump_sregs("scalar_regs_dump.txt", cpu_core_inst.regfile_inst.sregfile_inst.regs);

        $display("Ending Fetch Instruction Monitor.");
        if(instr_monitor != null) instr_monitor.close_files();
    end

endmodule : tb_top
