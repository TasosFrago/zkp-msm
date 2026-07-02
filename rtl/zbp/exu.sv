module exu
    import zbp_pkg::*;
(
    input logic clk,
    input logic rst,

    pipeline_if.in exec_if,
    pipeline_if.out wb_if
);

    assign exec_if.ready = TRUE;

    logic [31:0] imm_val;
    always_comb begin
        unique case (exec_if.data.imm.fmt)
            IMM_U:   imm_val = { exec_if.data.imm.bits, 12'b0 };
            IMM_J_B: imm_val = { {11{exec_if.data.imm.bits[19]}}, exec_if.data.imm.bits, 1'b0 };
            IMM_I_S: imm_val = { {12{exec_if.data.imm.bits[19]}}, exec_if.data.imm.bits };
        endcase
    end

    // VMADD

    // VMMUL

    // VCMP

    // SALU
    swb_t wbS_out;

    logic [31:0] alu_opa, alu_opb;

    assign alu_opa = (exec_if.data.op_tag == OP_AUIPC) ?
        exec_if.data.pc :
        exec_if.data.rs1[31:0];

    assign alu_opb = (exec_if.data.rs2_is_imm) ?
        imm_val :
        exec_if.data.rs2[31:0];

    salu salu_inst(
        .clk(clk),
        .rst(rst),

        .valid_in(exec_if.valid & exec_if.ready
            & (exec_if.data.eu_tag == EU_SALU)
            & exec_if.data.rd.en
        ),
        .tid(exec_if.data.tid),
        .rd(exec_if.data.rd.idx),
        .opa(alu_opa),
        .opb(alu_opb),
        .res(wbS_out),
        .op_tag(exec_if.data.op_tag)
    );

    // CFU
    swb_t cfu_rd_out;
    cf_redirect_t cf_redirect_p;
    cf_pc_adv_t   cf_pc_adv;

    logic cfu_fire;
    assign cfu_fire = exec_if.valid & exec_if.ready &
                      (exec_if.data.eu_tag == EU_CF);

    always_ff @(posedge clk) begin
        if (rst) begin
            cf_pc_adv <= '{default: '0};
        end
        else begin
            cf_pc_adv <= '{
                vld: (exec_if.valid & exec_if.ready),
                tid: exec_if.data.tid
            };
        end
    end

    cfu cfu_inst(
        .clk(clk),
        .rst(rst),

        .valid_in(cfu_fire),
        .tid(exec_if.data.tid),
        .pc(exec_if.data.pc),
        .rd(exec_if.data.rd.idx),
        .rd_en(exec_if.data.rd.en),
        .rs1(exec_if.data.rs1[31:0]),
        .rs2(exec_if.data.rs2[31:0]),
        .imm(imm_val),
        .op_tag(exec_if.data.op_tag),

        .res(cfu_rd_out),
        .cf_redirect(cf_redirect_p)
    );

    // LSU

    // Writeback
    wb_out_t wb_p;

    always_comb begin
        wb_p = '{default: '0};

        // Scalar WB
        case (1'b1)
            wbS_out.tag.en:    wb_p.wbS = wbS_out;
            cfu_rd_out.tag.en: wb_p.wbS = cfu_rd_out;
            default:           wb_p.wbS = '{default: '0};
        endcase

        // Vector port A WB
        case (1'b1)
            default: wb_p.wbA = '{default: '0};
        endcase

        // Vector port B WB
        case (1'b1)
            default: wb_p.wbB = '{default: '0};
        endcase

        wb_p.cf_redirect_p = cf_redirect_p;
        wb_p.cf_pc_adv_p = cf_pc_adv;
    end

    assign wb_if.data = wb_p;

    assign wb_if.valid = wb_p.wbS.tag.en |
                         wb_p.wbA.tag.en |
                         wb_p.wbB.tag.en |
                         wb_p.cf_redirect_p.vld |
                         wb_p.cf_pc_adv_p.vld;

endmodule : exu
