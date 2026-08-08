module regfile_stage
    import zbp_pkg::*;
(
    input logic clk,
    input logic rst,

    pipeline_if.in  iss_if,
    pipeline_if.out exec_if,

    input bwb_t wbA,
    input bwb_t wbB,
    input swb_t wbS
);

    typedef enum logic [1:0] { RF_IDLE, RF_WAIT_RS2 /*, RF_WAIT_VSHFL*/ } rf_state_t;
    rf_state_t state;

    scoreboard_out_t dec_data;
    assign dec_data = iss_if.data;

    // Detangle operands
    op_info_t rs1, rs2, rd;

    assign rs1 = dec_data.rs1;
    assign rs2 = dec_data.rs2;
    assign rd  = dec_data.rd;

    // assign rs2 = (~dec_data.rs2.is_imm) ? '{
    //         en: TRUE,
    //         idx: dec_data.rs2.val.as_r.idx,
    //         is_bn: dec_data.rs2.val.as_r.is_bn
    //     } :
    //     (dec_data.rd_is_rs) ? dec_data.rd : '0;
    //
    // assign rd = (~dec_data.rs2.is_imm) ? dec_data.rd :
    //             (dec_data.rd_is_rs) ? '0 :
    //             dec_data.rd;

    // always_comb begin
    //     if (~dec_data.rs2.is_imm) begin
    //         rs2 = '{
    //             en:    TRUE,
    //             idx:   dec_data.rs2.val.as_r.idx,
    //             is_bn: dec_data.rs2.val.as_r.is_bn
    //         };
    //         rd  = dec_data.rd;
    //     end
    //     else if (dec_data.rd_is_rs) begin
    //         rs2 = dec_data.rd;
    //         rd  = '0;
    //     end
    //     else begin
    //         rs2 = '0;
    //         rd  = dec_data.rd;
    //     end
    // end

    typedef struct packed {
        logic [TID_W-1:0] tid;
        logic [31:0] pc;
        eu_tag_t eu_tag;
        op_tag_t op_tag;

        op_info_t rd;
        op_info_t rs1;
        op_info_t rs2;

        logic imm_en;
        imm_t imm;

        logic was_stall;
        logic was_bshfl_stall;

        `ifdef DEBUG
        logic [31:0] instr;
        `endif
    } rf_hold_t;

    rf_hold_t hold;
    logic [NUMBER_SIZE-1:0] buffered_rs1;

    logic valid_q;
    logic stall_out;
    assign stall_out = valid_q & ~exec_if.ready;

    assign iss_if.ready = (state == RF_IDLE) & (~valid_q | exec_if.ready);

    logic [TID_W-1:0]       bn_rd_tid, s_rd_tid;

    logic                   bn_rd_en_A, bn_rd_en_B;
    logic [R_IDX_W-1:0]     bn_rd_reg_A, bn_rd_reg_B;
    logic [NUMBER_SIZE-1:0] bn_rd_data_A, bn_rd_data_B;

    logic               s_rd_en_A, s_rd_en_B;
    logic [R_IDX_W-1:0] s_rd_reg_A, s_rd_reg_B;
    logic [W-1:0]       s_rd_data_A, s_rd_data_B;

    always_comb begin
        bn_rd_en_A  = FALSE;
        bn_rd_reg_A = '0;
        bn_rd_en_B  = FALSE;
        bn_rd_reg_B = '0;
        s_rd_en_A  = FALSE;
        s_rd_reg_A = '0;
        s_rd_en_B  = FALSE;
        s_rd_reg_B = '0;

        bn_rd_tid = hold.tid;
        s_rd_tid = hold.tid;

        if (~stall_out) begin
            unique case (state)
                RF_IDLE: begin
                    if (iss_if.valid & iss_if.ready) begin
                        bn_rd_tid = iss_if.data.tid;
                        s_rd_tid  = iss_if.data.tid;

                        if (iss_if.data.read_stall) begin
                            bn_rd_en_A  = TRUE;
                            bn_rd_reg_A = rs1.idx;
                        end
                        else if (iss_if.data.bshfl_stall) begin
                            s_rd_en_B  = TRUE;
                            s_rd_reg_B = rs2.idx;
                        end
                        else begin
                            bn_rd_en_A  = rs1.en & rs1.is_bn;
                            bn_rd_reg_A = rs1.idx;
                            bn_rd_en_B  = rs2.en & rs2.is_bn;
                            bn_rd_reg_B = rs2.idx;

                            s_rd_en_A  = rs1.en & ~rs1.is_bn;
                            s_rd_reg_A = rs1.idx;
                            s_rd_en_B  = rs2.en & ~rs2.is_bn;
                            s_rd_reg_B = rs2.idx;
                        end
                    end
                end
                RF_WAIT_RS2: begin
                    bn_rd_tid = hold.tid;
                    bn_rd_en_A = TRUE;
                    bn_rd_reg_A = hold.rs2.idx;
                end
            endcase
        end
    end

    always_ff @(posedge clk) begin
        if (rst) begin
            state <= RF_IDLE;
            valid_q <= FALSE;
            hold <= '{eu_tag: EU_NOOP, op_tag: OP_NONE, imm: '{fmt: IMM_U, default: '0}, default: '0};
            buffered_rs1 <= '0;
        end
        else if (~stall_out) begin
            unique case (state)
                RF_IDLE: begin
                    if (iss_if.valid & iss_if.ready) begin
                        hold.tid <= iss_if.data.tid;
                        hold.pc <= iss_if.data.pc;
                        hold.eu_tag <= iss_if.data.eu_tag;
                        hold.op_tag <= iss_if.data.op_tag;
                        hold.rd <= rd;
                        hold.rs1 <= rs1;
                        hold.rs2 <= rs2;
                        hold.imm_en <= iss_if.data.rs3.is_imm & iss_if.data.rs3.en;
                        hold.imm <= iss_if.data.rs3.val.as_imm;
                        hold.was_stall <= iss_if.data.read_stall;

                        `ifdef DEBUG
                        hold.instr <= dec_data.instr;
                        `endif

                        if (iss_if.data.read_stall) begin
                            state <= RF_WAIT_RS2;
                            valid_q <= FALSE;
                        end
                        else begin
                            state <= RF_IDLE;
                            valid_q <= TRUE;
                        end
                    end
                    else begin
                        valid_q <= FALSE;
                    end
                end

                RF_WAIT_RS2: begin
                    buffered_rs1 <= bn_rd_data_A;
                    state <= RF_IDLE;
                    valid_q <= TRUE;
                end
            endcase
        end
    end

    logic [NUMBER_SIZE-1:0] rs1_val, rs2_val;

    always_comb begin
        if (hold.was_stall) begin
            rs1_val = buffered_rs1;
            rs2_val = bn_rd_data_A;
        end
        else begin
            rs1_val = ~hold.rs1.en ? '0 :
                      hold.rs1.is_bn ? bn_rd_data_A :
                      { {(NUMBER_SIZE-W){1'b0}}, s_rd_data_A };

            rs2_val = ~hold.rs2.en ? '0 :
                      hold.rs2.is_bn ? bn_rd_data_B :
                      { {(NUMBER_SIZE-W){1'b0}}, s_rd_data_B };
        end
    end

    assign exec_if.valid = valid_q;
    assign exec_if.data = '{
        tid: hold.tid,
        pc: hold.pc,
        eu_tag: hold.eu_tag,
        op_tag: hold.op_tag,
        rd: hold.rd,
        imm_en: hold.imm_en,
        imm: hold.imm,
        rs1: rs1_val,
        rs2: rs2_val

        `ifdef DEBUG
        ,instr: hold.instr
        `endif

    };

    bregfile bregfile_inst(
        .clk(clk),
        .rst(rst),

        .wr_data_A(wbA.data),
        .wr_en_A(wbA.tag.en),
        .wr_tid_A(wbA.tag.tid),
        .wr_reg_A(wbA.tag.rd),

        .wr_data_B(wbB.data),
        .wr_en_B(wbB.tag.en),
        .wr_tid_B(wbB.tag.tid),
        .wr_reg_B(wbB.tag.rd),

        .rd_tid(bn_rd_tid),

        .rd_data_A(bn_rd_data_A),
        .rd_en_A(bn_rd_en_A),
        .rd_reg_A(bn_rd_reg_A),

        .rd_data_B(bn_rd_data_B),
        .rd_en_B(bn_rd_en_B),
        .rd_reg_B(bn_rd_reg_B)
    );

    sregfile sregfile_inst(
        .clk(clk),
        .rst(rst),

        .wr_data(wbS.data),
        .wr_en(wbS.tag.en),
        .wr_tid(wbS.tag.tid),
        .wr_reg(wbS.tag.rd),

        .rd_tid(s_rd_tid),

        .rd_data_A(s_rd_data_A),
        .rd_en_A(s_rd_en_A),
        .rd_reg_A(s_rd_reg_A),

        .rd_data_B(s_rd_data_B),
        .rd_en_B(s_rd_en_B),
        .rd_reg_B(s_rd_reg_B)
    );

endmodule : regfile_stage
