module regfile_stage
    import zbp_pkg::*;
(
    input logic clk,
    input logic rst,

    pipeline_if.in  iss_if,
    pipeline_if.out exec_if,

    input vwb_t wbA,
    input vwb_t wbB,
    input swb_t wbS
);

    typedef enum logic { RF_IDLE, RF_WAIT_RS2 } rf_state_t;
    rf_state_t state;

    scoreboard_out_t dec_data;
    assign dec_data = iss_if.data;

    // Detangle operands
    op_info_t rs1, rs2, rd;
    assign rs1 = dec_data.rs1;

    always_comb begin
        if (~dec_data.rs2.is_imm) begin
            rs2 = '{
                en:   TRUE,
                idx:  dec_data.rs2.val.as_r.idx,
                is_v: dec_data.rs2.val.as_r.is_v
            };
            rd  = dec_data.rd;
        end
        else if (dec_data.rd_is_rs) begin
            rs2 = dec_data.rd;
            rd  = '0;
        end
        else begin
            rs2 = '0;
            rd  = dec_data.rd;
        end
    end

    typedef struct packed {
        logic [TID_W-1:0] tid;
        logic [31:0] pc;
        eu_tag_t eu_tag;
        op_tag_t op_tag;

        op_info_t rd;
        op_info_t rs1;
        op_info_t rs2;

        logic rs2_is_imm;
        imm_t imm;

        logic was_stall;
    } rf_hold_t;

    rf_hold_t hold;
    logic [NUMBER_SIZE-1:0] buffered_rs1;

    logic valid_q;
    logic stall_out;
    assign stall_out = valid_q & ~exec_if.ready;

    assign iss_if.ready = (state == RF_IDLE) & (~valid_q | exec_if.ready);

    logic [TID_W-1:0]       v_rd_tid, s_rd_tid;

    logic                   v_rd_en_A, v_rd_en_B;
    logic [R_IDX_W-1:0]     v_rd_reg_A, v_rd_reg_B;
    logic [NUMBER_SIZE-1:0] v_rd_data_A, v_rd_data_B;

    logic               s_rd_en_A, s_rd_en_B;
    logic [R_IDX_W-1:0] s_rd_reg_A, s_rd_reg_B;
    logic [W-1:0]       s_rd_data_A, s_rd_data_B;

    always_comb begin
        v_rd_en_A  = FALSE;
        v_rd_reg_A = '0;
        v_rd_en_B  = FALSE;
        v_rd_reg_B = '0;
        s_rd_en_A  = FALSE;
        s_rd_reg_A = '0;
        s_rd_en_B  = FALSE;
        s_rd_reg_B = '0;

        v_rd_tid = hold.tid;
        s_rd_tid = hold.tid;

        if (~stall_out) begin
            unique case (state)
                RF_IDLE: begin
                    if (iss_if.valid & iss_if.ready) begin
                        v_rd_tid = iss_if.data.tid;
                        s_rd_tid = iss_if.data.tid;

                        if (iss_if.data.read_stall) begin
                            v_rd_en_A = 1'b1;
                            v_rd_reg_A = rs1.idx;
                        end
                        else begin
                            v_rd_en_A = rs1.en & rs1.is_v;
                            v_rd_reg_A = rs1.idx;
                            v_rd_en_B = rs2.en & rs2.is_v;
                            v_rd_reg_B = rs2.idx;

                            s_rd_en_A = rs1.en & ~rs1.is_v;
                            s_rd_reg_A = rs1.idx;
                            s_rd_en_B = rs2.en & ~rs2.is_v;
                            s_rd_reg_B = rs2.idx;
                        end
                    end
                end
                RF_WAIT_RS2: begin
                    v_rd_tid = hold.tid;
                    v_rd_en_A = TRUE;
                    v_rd_reg_A = hold.rs2.idx;
                end
            endcase
        end
    end

    always_ff @(posedge clk) begin
        if (rst) begin
            state <= RF_IDLE;
            valid_q <= FALSE;
            hold <= '{eu_tag: EU_NOOP, op_tag: OP_NONE, default: '0};
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
                        hold.rs2_is_imm <= iss_if.data.rs2.is_imm;
                        hold.imm <= iss_if.data.rs2.val.as_imm;
                        hold.was_stall <= iss_if.data.read_stall;

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
                    buffered_rs1 <= v_rd_data_A;
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
            rs2_val = v_rd_data_A;
        end
        else begin
            rs1_val = ~hold.rs1.en ? '0 :
                      hold.rs1.is_v ? v_rd_data_A :
                      { {(NUMBER_SIZE-W){1'b0}}, s_rd_data_A };

            rs2_val = ~hold.rs2.en ? '0 :
                      hold.rs2.is_v ? v_rd_data_B :
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
        rs2_is_imm: hold.rs2_is_imm,
        imm: hold.imm,
        rs1: rs1_val,
        rs2: rs2_val
    };

    vregfile vregfile_inst(
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

        .rd_tid(v_rd_tid),

        .rd_data_A(v_rd_data_A),
        .rd_en_A(v_rd_en_A),
        .rd_reg_A(v_rd_reg_A),

        .rd_data_B(v_rd_data_B),
        .rd_en_B(v_rd_en_B),
        .rd_reg_B(v_rd_reg_B)
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
