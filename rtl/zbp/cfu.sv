module cfu
    import zbp_pkg::*;
(
    input logic clk,
    input logic rst,

    input  logic             valid_in,
    output logic             ready_in,
    input  logic [TID_W-1:0] tid,
    input  logic [    W-1:0] pc,

    input logic [R_IDX_W-1:0] rd,
    input logic               rd_en,

    input logic [W-1:0] rs1,
    input logic [W-1:0] rs2,
    input logic [W-1:0] imm,

    input op_tag_t op_tag,

    output logic branch_taken,
    pipeline_if cfu_out_if
    // output swb_t res,
    // output cf_redirect_t cf_redirect
);

    logic is_eq, is_lt, is_ltu;

    assign is_eq  = (rs1 == rs2);
    assign is_lt  = ($signed(rs1) < $signed(rs2));
    assign is_ltu = (rs1 < rs2);

    // logic branch_taken;

    always_comb begin
        case (op_tag)
            OP_BEQ:  branch_taken = is_eq;
            OP_BNE:  branch_taken = ~is_eq;
            OP_BLT:  branch_taken = is_lt;
            OP_BGE:  branch_taken = ~is_lt;
            OP_BLTU: branch_taken = is_ltu;
            OP_BGEU: branch_taken = ~is_ltu;
            OP_JAL,
            OP_JALR: branch_taken = TRUE;
            default: branch_taken = FALSE;
        endcase
    end

    logic [31:0] target_pc;
    assign target_pc = (op_tag == OP_JALR) ?
        (rs1 + imm) & ~32'd1 :
        pc + imm;

    swb_t rd_res;
    cf_redirect_t cf_redirect;

    assign rd_res = '{
        tag: '{
            en: valid_in & rd_en & (op_tag == OP_JAL || op_tag == OP_JALR),
            tid: tid,
            rd: rd
        },
        data: pc + 32'd4
    };

    assign cf_redirect = '{
        vld: valid_in & branch_taken,
        tid: tid,
        pc:  target_pc
    };

    // always_ff @(posedge clk) begin
    //     if (rst) begin
    //         res         <= '{default: '0};
    //         cf_redirect <= '{default: '0};
    //     end
    //     else begin
    //         res <= '{
    //             tag: '{
    //                 en: valid_in & rd_en &
    //                     (op_tag == OP_JAL || op_tag == OP_JALR),
    //                 tid: tid,
    //                 rd: rd
    //             },
    //             data: pc + 32'd4
    //         };
    //
    //         cf_redirect <= '{
    //             vld: valid_in & branch_taken,
    //             tid: tid,
    //             pc: target_pc
    //         };
    //     end
    // end

    skid_buffer #(
        .DATA_W($bits(cf_redirect_t) + $bits(swb_t))
    ) cfu_out_buff(
        .clk(clk),
        .rst(rst),

        .valid_in(valid_in),
        .ready_in(ready_in),
        .data_in ({rd_res, cf_redirect}),

        .valid_out(cfu_out_if.valid),
        .ready_out(cfu_out_if.ready),
        .data_out (cfu_out_if.data)
    );

endmodule : cfu
