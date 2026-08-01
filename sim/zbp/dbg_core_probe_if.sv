interface dbg_core_probe_if
    import zbp_pkg::*;
(
    input logic clk,
    input logic rst,

    // ----- FETCH STAGE -----
    input logic imem_rsp_ready,
    input logic imem_rsp_valid,
    input imem_rsp_t imem_rsp_data_in,

    input logic [31:0] pc_tb [MAX_THREADS],

    // ----- DECODE STAGE -----
    input logic fetch_to_dec_ready,
    input logic fetch_to_dec_valid,
    input fetch_out_t fetch_to_dec_data_in,

    // ----- SCOREBOARD STAGE -----
    input logic dec_to_scb_ready,
    input logic dec_to_scb_valid,
    input decode_out_t dec_to_scb_data_in,
    input logic scb_operand1_rdy,
    input logic scb_operand2_rdy,
    input logic scb_read_stall,
    input logic scb_bshfl_stall,
    input logic scb_can_issue,
    input logic scb_buff_rdy,

    // ----- REGFILE STAGE -----
    input logic scb_to_rf_ready,
    input logic scb_to_rf_valid,
    input scoreboard_out_t scb_to_rf_data_in,

    // ----- EXEC STAGE -----
    input logic rf_to_exu_ready,
    input logic rf_to_exu_valid,
    input exec_in_t rf_to_exu_data_in,

    input logic        ex_bn_write_collision,
    input logic [31:0] ex_wb_bank_tracker[4],
    input logic [31:0] ex_wb_port_tracker[2],
    input logic [ 4:0] ex_exec_lat,

    // ----- WB STAGE -----
    input logic exu_to_wb_ready,
    input logic exu_to_wb_valid,
    input wb_out_t exu_to_wb_data_in
);

    logic imem_rsp_fire;
    logic fetch_to_dec_fire;
    logic dec_to_scb_fire;
    logic scb_to_rf_fire;
    logic rf_to_exu_fire;
    logic exu_to_wb_fire;

    assign imem_rsp_fire     = imem_rsp_ready & imem_rsp_valid;
    assign fetch_to_dec_fire = fetch_to_dec_ready & fetch_to_dec_valid;
    assign dec_to_scb_fire   = dec_to_scb_ready & dec_to_scb_valid;
    assign scb_to_rf_fire    = scb_to_rf_ready & scb_to_rf_valid;
    assign rf_to_exu_fire    = rf_to_exu_ready & rf_to_exu_valid;
    assign exu_to_wb_fire    = exu_to_wb_ready & exu_to_wb_valid;

    clocking cb @(posedge clk);
        input rst;

        // ----- FETCH STAGE -----
        input imem_rsp_fire;
        input imem_rsp_data = imem_rsp_data_in;

        input imem_rsp_pc = pc_tb[imem_rsp_data_in.tid];

        // ----- DECODE STAGE -----
        input fetch_to_dec_fire;
        input fetch_to_dec_rdy = fetch_to_dec_ready;
        input fetch_to_dec_vld = fetch_to_dec_valid;
        input fetch_to_dec_data = fetch_to_dec_data_in;

        // ----- SCOREBOARD STAGE -----
        input dec_to_scb_fire;
        input dec_to_scb_rdy = dec_to_scb_ready;
        input dec_to_scb_vld = dec_to_scb_valid;
        input dec_to_scb_data = dec_to_scb_data_in;
        input scb_operand1_rdy;
        input scb_operand2_rdy;
        input scb_read_stall;
        input scb_bshfl_stall;
        input scb_can_issue;
        input scb_buff_rdy;

        // ----- REGFILE STAGE -----
        input scb_to_rf_fire;
        input scb_to_rf_rdy = scb_to_rf_ready;
        input scb_to_rf_vld = scb_to_rf_valid;
        input scb_to_rf_data = scb_to_rf_data_in;

        // ----- EXEC STAGE -----
        input rf_to_exu_fire;
        input rf_to_exu_rdy = rf_to_exu_ready;
        input rf_to_exu_vld = rf_to_exu_valid;
        input rf_to_exu_data = rf_to_exu_data_in;

        input ex_bn_write_collision;
        input ex_wb_bank_tracker;
        input ex_wb_port_tracker;
        input ex_exec_lat;

        // ----- WB STAGE -----
        input exu_to_wb_fire;
        input exu_to_wb_rdy = exu_to_wb_ready;
        input exu_to_wb_vld = exu_to_wb_valid;
        input exu_to_wb_data = exu_to_wb_data_in;
    endclocking : cb

endinterface : dbg_core_probe_if
