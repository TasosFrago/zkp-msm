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

    output logic program_done
);

    logic memory_stall;
    logic wb_fifo_full;
    logic salu_ready, salu_can_output;
    logic cfu_ready;
    logic write_collision;

    assign exec_if.ready = ~memory_stall & ~wb_fifo_full &
        (((exec_if.data.eu_tag == EU_SALU)  ? salu_ready :
          (exec_if.data.eu_tag == EU_CF)    ? cfu_ready :
          (exec_if.data.eu_tag == EU_BMMUL) ? ~write_collision :
          (exec_if.data.eu_tag == EU_BMADD) ? ~write_collision :
          (exec_if.data.eu_tag == EU_BALU)  ? ~write_collision :
          TRUE));

    logic [31:0] imm_val;
    always_comb begin
        unique case (exec_if.data.imm.fmt)
            IMM_U:   imm_val = { exec_if.data.imm.bits, 12'b0 };
            IMM_J_B: imm_val = { {11{exec_if.data.imm.bits[19]}}, exec_if.data.imm.bits, 1'b0 };
            IMM_I_S: imm_val = { {12{exec_if.data.imm.bits[19]}}, exec_if.data.imm.bits };
        endcase
    end

    mmio_registers_t mmio_regs_out;

    // BN WB Tracker
    logic [4-1:0] bank_tracker;
    logic [2-1:0] port_tracker;

    bnwb_tracker wb_track_inst (
        .clk(clk),
        .rst(rst),

        .is_bn_write(exec_if.data.rd.en & exec_if.data.rd.is_bn),
        .eu_tag(exec_if.data.eu_tag),
        .op_tag(exec_if.data.op_tag),
        .bank_target(exec_if.data.rd.idx[1:0]),

        .valid_in(exec_if.valid & exec_if.ready),

        .write_collision(write_collision),
        .bank_tracker(bank_tracker),
        .port_tracker(port_tracker)
    );

    // BALU
    bwb_t wb_balu_out;
    logic balu_valid_in, balu_valid_out;

    assign balu_valid_in = exec_if.valid & exec_if.ready &
        (exec_if.data.eu_tag == EU_BALU) &
        exec_if.data.rd.en;

    balu balu_inst (
        .clk(clk),
        .rst(rst),

        .valid_in(balu_valid_in),
        .tid(exec_if.data.tid),
        .rd(exec_if.data.rd.idx),
        .op_tag(exec_if.data.op_tag),

        .opa(exec_if.data.rs1),
        .opb(exec_if.data.rs2),

        .valid_out(balu_valid_out),
        .wbBN(wb_balu_out)
    );

    // BMADD
    bwb_t wb_bmadd_out;
    logic bmadd_valid_in, bmadd_valid_out;

    assign bmadd_valid_in = exec_if.valid & exec_if.ready
        & (exec_if.data.eu_tag == EU_BMADD)
        & exec_if.data.rd.en;

    bmadd bmadd_inst (
        .clk(clk),
        .rst(rst),

        .valid_in(bmadd_valid_in),
        .tid(exec_if.data.tid),
        .rd(exec_if.data.rd.idx),
        .op_tag(exec_if.data.op_tag),

        .opa(exec_if.data.rs1),
        .opb(exec_if.data.rs2),

        .modulus(mmio_regs_out.modulus),

        .valid_out(bmadd_valid_out),
        .wbBN(wb_bmadd_out)
    );

    // BMMUL
    bwb_t wb_bmmul_out;

    logic bmmul_valid_in, bmmul_valid_out;

    assign bmmul_valid_in = exec_if.valid & exec_if.ready
            & (exec_if.data.eu_tag == EU_BMMUL)
            & exec_if.data.rd.en;

    bmmul bmmul_inst (
        .clk(clk),
        .rst(rst),

        .valid_in(bmmul_valid_in),
        .tid(exec_if.data.tid),
        .rd(exec_if.data.rd.idx),
        .opa(exec_if.data.rs1),
        .opb(exec_if.data.rs2),

        .mmio_regs(mmio_regs_out),
        .valid_out(bmmul_valid_out),
        .wbBN(wb_bmmul_out)
    );

    // BCMP

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
        .mmio_regs_out(mmio_regs_out),
        .program_done(program_done)
    );

    // LSU
    swb_t wbS_lsu_out;
    bwb_t wbBN_lsu_out;
    logic lsu_bnport_sel;
    logic lsu_bnport_is_0, lsu_bnport_is_1;

    logic lsu_fire;
    assign lsu_fire = exec_if.valid & (exec_if.data.eu_tag == EU_LSU);

    assign lsu_bnport_is_0 = (lsu_bnport_sel == 1'b0);
    assign lsu_bnport_is_1 = (lsu_bnport_sel == 1'b1);

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
        .wbBN_out(wbBN_lsu_out),
        .bp_sel(lsu_bnport_sel)
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

    typedef struct packed {
        logic valid;
        bwb_t data;
    } bcand_t;

    localparam int NUM_BNCANDS = 4;
    bcand_t bncands[NUM_BNCANDS];
    logic assigned_A;

    assign bncands[0] = '{valid: wbBN_lsu_out.tag.en, data: wbBN_lsu_out};
    assign bncands[1] = '{valid: bmmul_valid_out,     data: wb_bmmul_out};
    assign bncands[2] = '{valid: bmadd_valid_out,     data: wb_bmadd_out};
    assign bncands[3] = '{valid: balu_valid_out,      data: wb_balu_out};


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

        assigned_A = FALSE;
        for(int i = 0; i < NUM_BNCANDS; i++) begin
            if (bncands[i].valid) begin
                if(~assigned_A) begin
                    wb_p.wbA = bncands[i].data;
                    assigned_A = TRUE;
                end
                else begin
                    wb_p.wbB = bncands[i].data;
                end
            end
        end

        wb_p.cf_redirect_p = cf_redirect_p;
        wb_p.cf_pc_adv_p = cf_pc_adv;
    end

    assign wb_if.data = wb_p;

    assign wb_if.valid = wb_p.wbS.tag.en |
                         wb_p.wbA.tag.en |
                         wb_p.wbB.tag.en |
                         wb_p.cf_redirect_p.vld |
                         wb_p.cf_pc_adv_p.vld;

    // synthesis translate_off
    logic [NUM_BNCANDS-1:0] active_bn_cands;
    int i;

    always_comb begin
        for(int i = 0; i < NUM_BNCANDS; i++) begin
            active_bn_cands[i] = bncands[i].valid;
        end
    end
    property max_two_bn_wb_cands;
        @(posedge clk) disable iff (rst)
        $countones(active_bn_cands) <= 2;
    endproperty

    assert property (max_two_bn_wb_cands) else begin
        for(i = 0; i < NUM_BNCANDS; i++) begin
            if (bncands[i].valid)
                $display(" [%d]: TID[%d], RD: %d",
                i, bncands[i].data.tag.tid, bncands[i].data.tag.rd);
        end
        $error("WB: More than two canditates at BN Ports");
    end
    // synthesis translate_on

endmodule : exu
