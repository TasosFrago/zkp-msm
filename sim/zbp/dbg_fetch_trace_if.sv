interface dbg_fetch_trace_if
    import zbp_pkg::*;
(
    input logic clk,
    input logic rst,

    input logic fetch_valid,
    input logic fetch_ready,
    input logic [31:0] fetch_instr,
    input logic [4:0]  fetch_tid,

    input cf_redirect_t cf_redirect,
    input cf_pc_adv_t   cf_pc_adv,

    input logic [31:0] pc_tb [MAX_THREADS]
);

    clocking cb @(posedge clk);
        input rst;
        input fetch_valid, fetch_ready, fetch_instr, fetch_tid;
        input cf_redirect, cf_pc_adv;
        input pc_tb;
    endclocking : cb

endinterface : dbg_fetch_trace_if
