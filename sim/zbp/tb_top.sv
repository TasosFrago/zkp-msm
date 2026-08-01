module tb_top #(
    parameter int MEM_SIZE_BYTES = 65536
) (
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
        .MEM_SIZE_WORDS(5500/4),
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
        .MEM_SIZE_BYTES(MEM_SIZE_BYTES),
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

    bind zbp_core dbg_core_probe_if core_probe_inst (
        .clk(clk),
        .rst(rst),

        .imem_rsp_ready(imem_rsp_if.ready),
        .imem_rsp_valid(imem_rsp_if.valid),
        .imem_rsp_data_in(imem_rsp_if.data),

        .pc_tb(fetch_inst.pc_tb),

        .fetch_to_dec_ready(fetch_to_dec_if.ready),
        .fetch_to_dec_valid(fetch_to_dec_if.valid),
        .fetch_to_dec_data_in(fetch_to_dec_if.data),

        .dec_to_scb_ready(dec_to_scb_if.ready),
        .dec_to_scb_valid(dec_to_scb_if.valid),
        .dec_to_scb_data_in(dec_to_scb_if.data),

        .scb_operand1_rdy(scb_inst.operand1_rdy),
        .scb_operand2_rdy(scb_inst.operand2_rdy),
        .scb_read_stall(scb_inst.read_stall),
        .scb_bshfl_stall(scb_inst.bshfl_stall),
        .scb_can_issue(scb_inst.can_issue),
        .scb_buff_rdy(scb_inst.buff_rdy),

        .scb_to_rf_ready(scb_to_rf_if.ready),
        .scb_to_rf_valid(scb_to_rf_if.valid),
        .scb_to_rf_data_in(scb_to_rf_if.data),

        .rf_to_exu_ready(rf_to_ex_if.ready),
        .rf_to_exu_valid(rf_to_ex_if.valid),
        .rf_to_exu_data_in(rf_to_ex_if.data),

        .ex_bn_write_collision(exec_inst.write_collision),
        .ex_wb_bank_tracker(exec_inst.wb_track_inst.wb_bank_tracker),
        .ex_wb_port_tracker(exec_inst.wb_track_inst.wb_port_tracker),
        .ex_exec_lat(exec_inst.wb_track_inst.exec_lat),

        .exu_to_wb_ready(ex_to_wb_if.ready),
        .exu_to_wb_valid(ex_to_wb_if.valid),
        .exu_to_wb_data_in(ex_to_wb_if.data)
    );

    event sim_finish_ev;

    PcHeartbeatMonitor pc_mon;
    FetchMonitor fetch_mon;
    DbgProbe probe;

    initial begin
        dbg_config_t cfg;
        cfg = dbg_get_config();

        $display("Starting Monitors.");

        fork
            begin
                if (cfg.pc_heartbeat_en) begin
                    $display("PCHeartbeat ENABLED");
                    pc_mon = new(
                        cpu_core_inst.fetch_inst.trace_if,
                        cfg.heartbeat_period,
                        sim_finish_ev,
                        cfg.stuck_threashold,
                        'h28 // <hang> function
                        );
                    pc_mon.run();
                end
            end
            begin
                if (cfg.fetch_monitor_en) begin
                    $display("FetchMonitor ENABLED");
                    fetch_mon = new(cpu_core_inst.fetch_inst.trace_if, "traces");
                    fetch_mon.run();
                end
            end
            begin
                if (cfg.dbg_probe_en) begin
                    $display("DBGProbe ENABLED");
                    probe = new(cpu_core_inst.core_probe_inst, 0,
                                "dbg_logs");
                    probe.run();
                end
            end
        join_none

        @(sim_finish_ev)
        $display("\nSimulation Finished\n");
        $finish;
    end

    always_ff @(posedge clk) begin
        if (~rst & program_done) begin
            $display("Program DONE!");
            ->sim_finish_ev;
        end
    end

    final begin
        $display("Simulation ended. Dumping Multithreaded Scalar Registers...");
        dump_sregs("scalar_regs_dump.txt", cpu_core_inst.regfile_inst.sregfile_inst.regs);

        $display("Ending Fetch Instruction Monitor.");
        if(fetch_mon != null) fetch_mon.close_files();
        if(probe != null) probe.close_files();
    end

endmodule : tb_top
