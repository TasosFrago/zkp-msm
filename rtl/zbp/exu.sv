module exu
    import zbp_pkg::*;
(
    input logic clk,
    input logic rst,

    pipeline_if.in exec_if,
    pipeline_if.out wb_if,

    // Dmem Interface
    pipeline_if.out dmem_req_if,
    pipeline_if.in  dmem_rsp_if,

    input logic [4-1:0] bank_tracker,
    input logic [2-1:0] port_tracker
);

    logic memory_stall;
    logic wb_fifo_full;

    assign exec_if.ready = ~memory_stall & ~wb_fifo_full;

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
    swb_t wbS_salu_out;

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

        .valid_in(exec_if.valid
            & (exec_if.data.eu_tag == EU_SALU)
            & exec_if.data.rd.en
        ),
        .tid(exec_if.data.tid),
        .rd(exec_if.data.rd.idx),
        .opa(alu_opa),
        .opb(alu_opb),
        .res(wbS_salu_out),
        .op_tag(exec_if.data.op_tag)
    );

    // CFU
    swb_t wbS_cfu_out;
    cf_redirect_t cf_redirect_p;
    cf_pc_adv_t   cf_pc_adv;

    logic cfu_fire;
    assign cfu_fire = exec_if.valid & (exec_if.data.eu_tag == EU_CF);

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

        .res(wbS_cfu_out),
        .cf_redirect(cf_redirect_p)
    );

    // LSU
    swb_t wbS_lsu_out;
    vwb_t wbV_lsu_out;
    logic lsu_vport_sel;
    logic lsu_vport_is_0, lsu_vport_is_1;

    logic lsu_fire;
    assign lsu_fire = exec_if.valid & (exec_if.data.eu_tag == EU_LSU);

    assign lsu_vport_is_0 = (lsu_vport_sel == 1'b0);
    assign lsu_vport_is_1 = (lsu_vport_sel == 1'b1);

    lsu #(
        .INTERMEDIATE_STORAGE(2),
        .PENDING_REQS(4)
    ) lsu_inst (
        .clk(clk),
        .rst(rst),

        .valid_in(lsu_fire),
        .op_tag(exec_if.data.op_tag),
        .tid(exec_if.data.tid),
        .rs1(exec_if.data.rs1[31:0]),
        .rs2(exec_if.data.rs2),
        .imm(imm_val),
        .rd(exec_if.data.rd),

        .dmem_req_if(dmem_req_if),
        .dmem_rsp_if(dmem_rsp_if),

        .bank_tracker(bank_tracker),
        .port_tracker(port_tracker),

        .mem_stall(memory_stall),
        .wbS_out(wbS_lsu_out),
        .wbV_out(wbV_lsu_out),
        .vp_sel(lsu_vport_sel)
    );

    // Writeback FIFO for scalars
    logic wb_fifo_push, wb_fifo_pop;
    logic wb_fifo_empty;
    logic wb_incoming_d;

    swb_t wb_fifo_din;
    swb_t wb_fifo_dout;

    assign wb_incoming_d =
        wbS_salu_out.tag.en |
        wbS_cfu_out.tag.en;

    assign wb_fifo_push = wb_incoming_d & (wbS_lsu_out.tag.en | ~wb_fifo_empty) & ~wb_fifo_full;
    assign wb_fifo_pop =  ~wbS_lsu_out.tag.en & ~wb_fifo_empty;

    assign wb_fifo_din =
        wbS_salu_out.tag.en ?
            wbS_salu_out :
        wbS_cfu_out.tag.en ?
            wbS_cfu_out :
            '0;

    sync_fifo #(
        .DATA_W($bits(swb_t)),
        .DEPTH(4)
    ) wb_fifo_s (
        .clk(clk),
        .rst(rst),

        .wdata_in(wb_fifo_din),
        .wr_en(wb_fifo_push),
        .full(wb_fifo_full),

        .rdata_out(wb_fifo_dout),
        .rd_en(wb_fifo_pop),
        .empty(wb_fifo_empty)
    );

    // Writeback
    wb_out_t wb_p;

    always_comb begin
        wb_p = '{default: '0};

        // Scalar WB
        if (wbS_lsu_out.tag.en) begin
            wb_p.wbS = wbS_lsu_out;
        end
        else if (~wb_fifo_empty) begin
            wb_p.wbS = wb_fifo_dout;
        end
        else if (wb_incoming_d) begin
            wb_p.wbS = wb_fifo_din;
        end
        else begin
            wb_p.wbS = '{default: '0};
        end

        // case (1'b1)
        //     wbS_salu_out.tag.en: wb_p.wbS = wbS_salu_out;
        //     wbS_cfu_out.tag.en:  wb_p.wbS = wbS_cfu_out;
        //     wbS_lsu_out.tag.en:  wb_p.wbS = wbS_lsu_out;
        //     default:             wb_p.wbS = '{default: '0};
        // endcase

        // Vector port A WB
        case (1'b1)
            (wbV_lsu_out.tag.en & lsu_vport_is_0): wb_p.wbA = wbV_lsu_out;
            default:                               wb_p.wbA = '{default: '0};
        endcase

        // Vector port B WB
        case (1'b1)
            (wbV_lsu_out.tag.en & lsu_vport_is_1): wb_p.wbB = wbV_lsu_out;
            default:                               wb_p.wbB = '{default: '0};
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
