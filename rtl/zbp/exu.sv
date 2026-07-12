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
    input logic [2-1:0] port_tracker,

    output logic program_done
);

    logic memory_stall;
    logic wb_fifo_full;
    logic salu_ready, salu_can_output;
    logic cfu_ready;

    assign exec_if.ready = ~memory_stall & ~wb_fifo_full &
        (((exec_if.data.eu_tag == EU_SALU) ? salu_ready :
          (exec_if.data.eu_tag == EU_CF) ? cfu_ready : TRUE));

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

    logic salu_fired, salu_valid_in, salu_valid_out;

    assign salu_valid_in = exec_if.valid
                    & (exec_if.data.eu_tag == EU_SALU)
                    & exec_if.data.rd.en;

    assign salu_fired = salu_valid_in & salu_ready;

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

        .valid_in(salu_valid_in),
        .ready_in(salu_ready),
        .tid(exec_if.data.tid),
        .rd(exec_if.data.rd.idx),
        .op_tag(exec_if.data.op_tag),
        .opa(alu_opa),
        .opb(alu_opb),

        .valid_out(salu_valid_out),
        .ready_out(salu_can_output),
        .res(wbS_salu_out)
    );

    // =========== CFU ===========
    pipeline_if#(.T(logic [($bits(cf_redirect_t) + $bits(swb_t))-1:0])) cfu_out_if();

    swb_t wbS_cfu_out;
    cf_redirect_t cf_redirect_p;
    cf_pc_adv_t   cf_pc_adv;

    logic cfu_branch_taken;

    logic cfu_fired, cfu_valid_in;
    assign cfu_valid_in = exec_if.valid & (exec_if.data.eu_tag == EU_CF);

    assign cfu_fired = cfu_valid_in & cfu_ready;

    assign {wbS_cfu_out, cf_redirect_p} =
        cfu_out_if.valid ? cfu_out_if.data : '0;

    cfu cfu_inst(
        .clk(clk),
        .rst(rst),

        .valid_in(cfu_valid_in),
        .ready_in(cfu_ready),
        .tid(exec_if.data.tid),
        .pc(exec_if.data.pc),
        .rd(exec_if.data.rd.idx),
        .rd_en(exec_if.data.rd.en),
        .rs1(exec_if.data.rs1[31:0]),
        .rs2(exec_if.data.rs2[31:0]),
        .imm(imm_val),
        .op_tag(exec_if.data.op_tag),

        .branch_taken(cfu_branch_taken),
        .cfu_out_if(cfu_out_if)
    );

    // MMIO
    pipeline_if#(.T(dmem_req_t)) mmio_req_if();
    pipeline_if#(.T(dmem_rsp_t)) mmio_rsp_if();
    logic mmio_intercept;

    mmio #(
        .BASE_ADDR(32'hF0000000),
        .GLOBAL_REGS(3)
    ) mmio_inst (
        .clk(clk),
        .rst(rst),

        .mmio_req_if(mmio_req_if),
        .intercept  (mmio_intercept),
        .mmio_rsp_if(mmio_rsp_if),
        .program_done(program_done)
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

        .wbS_fifo_full(wb_fifo_full),

        .mmio_req_if(mmio_req_if),
        .mmio_rsp_if(mmio_rsp_if),
        .mmio_intercept(mmio_intercept),

        .dmem_req_if(dmem_req_if),
        .dmem_rsp_if(dmem_rsp_if),

        .bank_tracker(bank_tracker),
        .port_tracker(port_tracker),

        .mem_stall(memory_stall),
        .wbS_out(wbS_lsu_out),
        .wbV_out(wbV_lsu_out),
        .vp_sel(lsu_vport_sel)
    );


    // ========= Advance PC =========
    always_ff @(posedge clk) begin
        if (rst) begin
            cf_pc_adv <= '{default: '0};
        end
        else if (exec_if.valid & exec_if.ready) begin
            cf_pc_adv <= '{
                vld: ~(cfu_valid_in & cfu_branch_taken),
                tid: exec_if.data.tid
            };
        end
        else begin
            cf_pc_adv <= '{default: '0};
        end
    end
    // synthesis translate_off
    property scalar_units_dont_fire_together;
        @(posedge clk) disable iff (rst)
        ~(salu_fired & cfu_fired)
    endproperty

    assert property (scalar_units_dont_fire_together) else
    $error("Scalar units fired together.");
    // synthesis translate_on

    // ========= Writeback FIFO for scalars =========
    logic wb_fifo_push, wb_fifo_pop;
    logic wb_fifo_empty;

    swb_t wb_fifo_din;
    swb_t wb_fifo_dout;

    logic salu_has_wb, cfu_has_wb;
    assign salu_has_wb = salu_valid_out & wbS_salu_out.tag.en;
    assign cfu_has_wb  = cfu_out_if.valid & wbS_cfu_out.tag.en;

    // Incoming wb data from scalar units
    logic wb_incoming_d;
    assign wb_incoming_d = salu_has_wb | cfu_has_wb;

    // CFU & SALU Ready_out signals
    assign salu_can_output = ~salu_has_wb | ~wb_fifo_full;
    assign cfu_out_if.ready = ~cfu_has_wb | (~salu_has_wb & ~wb_fifo_full);

    assign wb_fifo_din =
        salu_has_wb ?
            wbS_salu_out :
        cfu_has_wb ?
            wbS_cfu_out :
            '0;

    assign wb_fifo_push = wb_incoming_d & (wbS_lsu_out.tag.en | ~wb_fifo_empty) & ~wb_fifo_full;
    assign wb_fifo_pop =  ~wbS_lsu_out.tag.en & ~wb_fifo_empty;

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

        `ifdef DEBUG
        wb_p.instr = exec_if.data.instr;
        `endif

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
