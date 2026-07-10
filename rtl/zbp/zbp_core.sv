module zbp_core
    import zbp_pkg::*;
(
    input logic clk,
    input logic rst,

    // Instruction Memory Req/Rsp connection
    pipeline_if.out imem_req_if,
    pipeline_if.in  imem_rsp_if,

    // Data Memory Req/Rsp connection
    pipeline_if.out dmem_req_if,
    pipeline_if.in  dmem_rsp_if,

    output logic program_done
);

    ////////////////////////////////////////////////////////
    //                                                    //
    //                 PIPELINE INTERFACES                //
    //                                                    //
    ////////////////////////////////////////////////////////
    pipeline_if#(.T(fetch_out_t))      fetch_to_dec_if();
    pipeline_if#(.T(decode_out_t))     dec_to_scb_if();
    pipeline_if#(.T(scoreboard_out_t)) scb_to_rf_if();
    pipeline_if#(.T(exec_in_t))        rf_to_ex_if();
    pipeline_if#(.T(wb_out_t))         ex_to_wb_if();

    pipeline_if#(.T(iss_back_t))       scb_to_dec_iss_back_if();
    pipeline_if#(.T(iss_back_t))       dec_to_fetch_iss_back_if();

    wb_out_t wb_bus;
    assign wb_bus = ex_to_wb_if.data;
    assign ex_to_wb_if.ready = TRUE;

    logic [4-1:0] bank_tracker;
    logic [2-1:0] port_tracker;

    ////////////////////////////////////////////////////////
    //                                                    //
    //                     FETCH STAGE                    //
    //                                                    //
    ////////////////////////////////////////////////////////
    fetch fetch_inst(
        .clk(clk),
        .rst(rst),

        .imem_req_if(imem_req_if),
        .imem_rsp_if(imem_rsp_if),

        .cf_redirect(wb_bus.cf_redirect_p),
        .cf_pc_adv  (wb_bus.cf_pc_adv_p),

        .iss_back_if(dec_to_fetch_iss_back_if),
        .fetch_if(fetch_to_dec_if)
    );

    ////////////////////////////////////////////////////////
    //                                                    //
    //                    DECODE STAGE                    //
    //                                                    //
    ////////////////////////////////////////////////////////
    decode decode_inst(
        .clk(clk),
        .rst(rst),

        .fetch_if (fetch_to_dec_if),
        .iss_back_to_fetch_if(dec_to_fetch_iss_back_if),
        .decode_if(dec_to_scb_if),
        .iss_back_to_decode_if(scb_to_dec_iss_back_if)
    );

    ////////////////////////////////////////////////////////
    //                                                    //
    //                  SCOREBOARD STAGE                  //
    //                                                    //
    ////////////////////////////////////////////////////////
    scoreboard scb_inst(
        .clk(clk),
        .rst(rst),

        .decode_if(dec_to_scb_if),
        .iss_back_to_decode_if(scb_to_dec_iss_back_if),
        .iss_if   (scb_to_rf_if),

        .wbA(wb_bus.wbA.tag),
        .wbB(wb_bus.wbB.tag),
        .wbS(wb_bus.wbS.tag),

        .wb_bank_tracker_out(bank_tracker),
        .wb_port_tracker_out(port_tracker)
    );

    ////////////////////////////////////////////////////////
    //                                                    //
    //                 REGISTER FILE STAGE                //
    //                                                    //
    ////////////////////////////////////////////////////////
    regfile_stage regfile_inst(
        .clk(clk),
        .rst(rst),

        .iss_if(scb_to_rf_if),
        .exec_if(rf_to_ex_if),

        .wbA(wb_bus.wbA),
        .wbB(wb_bus.wbB),
        .wbS(wb_bus.wbS)
    );

    ////////////////////////////////////////////////////////
    //                                                    //
    //                   EXECUTION STAGE                  //
    //                                                    //
    ////////////////////////////////////////////////////////
    exu exec_inst(
        .clk(clk),
        .rst(rst),

        .exec_if(rf_to_ex_if),
        .wb_if  (ex_to_wb_if),

        .dmem_req_if(dmem_req_if),
        .dmem_rsp_if(dmem_rsp_if),

        .bank_tracker(bank_tracker),
        .port_tracker(port_tracker),

        .program_done(program_done)
    );

endmodule : zbp_core
