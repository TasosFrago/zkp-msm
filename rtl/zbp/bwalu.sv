module bwalu
    import zbp_pkg::*;
(
    input logic clk,
    input logic rst,

    input logic                   valid_in,
    output logic                  ready_in,
    input logic [      TID_W-1:0] tid,
    input logic [    R_IDX_W-1:0] rd,
    input op_tag_t                op_tag,
    input logic [NUMBER_SIZE-1:0] opa,
    input logic [NUMBER_SIZE-1:0] opb,
    input logic [          W-1:0] imm_val,

    output logic valid_out,
    input  logic ready_out,
    output swb_t res
);

    logic [7:0] bit_offset;
    logic [5:0] win_size;
    logic [W-1:0] shifted_data, mask;
    logic [W-1:0] bext_res;
    logic [W-1:0] res_out;

    assign win_size = 6'(imm_val[4:0] + 1);
    assign bit_offset = opb[7:0] * win_size;
    assign shifted_data = 32'(opa >> bit_offset);
    assign mask = (32'd1 << win_size) - 32'd1;
    assign bext_res = shifted_data & mask;

    always_comb begin
        case (op_tag)
            OP_BEXT_W: res_out = bext_res;
            default:   res_out = 32'hbaadbeef;
        endcase
    end

    swb_t res_comb;
    assign res_comb = '{
        tag: '{en: valid_in, tid: tid, rd: rd},
        data: res_out
    };

    skid_buffer #(
        .DATA_W($bits(swb_t))
    ) salu_out_buff (
        .clk(clk),
        .rst(rst),

        .valid_in(valid_in),
        .ready_in(ready_in),
        .data_in (res_comb),

        .valid_out(valid_out),
        .ready_out(ready_out),
        .data_out (res)
    );

endmodule : bwalu
