module vmadd
    import zbp_pkg::*;
(
    input logic clk,
    input logic rst,

    input logic                   valid_in,
    input logic [      TID_W-1:0] tid,
    input logic [    R_IDX_W-1:0] rd,
    input op_tag_t                op_tag,
    input logic [NUMBER_SIZE-1:0] opa,
    input logic [NUMBER_SIZE-1:0] opb,

    input logic [NUMBER_SIZE-1:0] modulus,

    output logic valid_out,
    output vwb_t wbV
);

    localparam int MADD_LAT = (NUMBER_SIZE / W) + 1;

    wb_tag_t wb_tag_in;
    assign wb_tag_in = '{
        en:  valid_in,
        tid: tid,
        rd:  rd
    };

    shiftreg #(
        .DATA_W($bits(wb_tag_t)),
        .DEPTH(MADD_LAT)
    ) tag_tracker (
        .clk(clk),
        .rst(rst),

        .valid_in(valid_in),
        .data_in(wb_tag_in),
        .data_out(wbV.tag)
    );

    assign valid_out = wbV.tag.en;

    mod_add_var #(
        .W(W),
        .CHUNKS(NUMBER_SIZE / W)
    ) madd (
        .clk(clk),
        .rst(rst),

        .op((op_tag == OP_VMADD) ? 1'b0 : 1'b1), // 0: addition, 1: subtraction

        .a(valid_in ? opa : '0),
        .b(valid_in ? opb : '0),
        .modulus(modulus),

        .res(wbV.data)
    );

    // sythesis translate_off
    property op_tag_is_add_or_sub;
        @(posedge clk) disable iff(rst) valid_in |->
            (op_tag == OP_VMADD || op_tag == OP_VMSUB)
    endproperty

    assert property (op_tag_is_add_or_sub) else
    $error("Op tag given to VMADD is invalid: %s", op_tag.name());
    // sythesis translate_on

endmodule : vmadd
