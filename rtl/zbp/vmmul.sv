module vmmul
    import zbp_pkg::*;
(
    input logic clk,
    input logic rst,

    input logic                   valid_in,
    input logic [      TID_W-1:0] tid,
    input logic [    R_IDX_W-1:0] rd,
    input logic [NUMBER_SIZE-1:0] opa,
    input logic [NUMBER_SIZE-1:0] opb,

    input mmio_registers_t mmio_regs,

    output logic valid_out,
    output vwb_t wbV
);

    localparam int MMUL_LAT = 3 * (NUMBER_SIZE / W);

    wb_tag_t wb_tag_in;
    assign wb_tag_in = '{
        en: valid_in,
        tid: tid,
        rd: rd
    };

    wb_tag_t wb_tag_out;

    assign valid_out = wb_tag_out.en;
    assign wbV.tag = wb_tag_out;

    shiftreg #(
        .DATA_W($bits(wb_tag_t)),
        .DEPTH(MMUL_LAT)
    ) tag_tracker (
        .clk(clk),
        .rst(rst),

        .valid_in(valid_in),
        .data_in (wb_tag_in),
        .data_out(wb_tag_out)
    );

    mmul_variable #(
        .NUMBER_SIZE(NUMBER_SIZE),
        .W(W)
    ) mmul_inst (
        .clk(clk),
        .rst(rst),

        .a(valid_in ? opa : '0),
        .b(valid_in ? opb : '0),

        .modulus(mmio_regs.modulus),
        .n_prime(mmio_regs.n_prime),

        .res(wbV.data)
    );

endmodule : vmmul
