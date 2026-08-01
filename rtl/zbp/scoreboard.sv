module scoreboard
    import zbp_pkg::*;
(
    input logic clk,
    input logic rst,

    pipeline_if.in  decode_if,
    pipeline_if.out iss_back_to_decode_if,
    pipeline_if.out iss_if,

    // Writeback Interface
    input wb_tag_t wbA,
    input wb_tag_t wbB,

    input wb_tag_t wbS  // Writeback to scalar reg
);

    localparam int SIDX_W = $clog2(REGISTERS * MAX_THREADS);
    localparam int BIDX_W = $clog2(BREGISTERS * MAX_THREADS);

    logic [(REGISTERS * MAX_THREADS)-1:0]  s_ready_vals;
    logic [(BREGISTERS * MAX_THREADS)-1:0] b_ready_vals;

    iss_back_t iss_back_data;

    decode_out_t dec_data;
    assign dec_data = decode_if.data;

    op_info_t rs1, rs2, rd;

    assign rs1 = dec_data.rs1;

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

    always_comb begin
        if (~dec_data.rs2.is_imm) begin
            rs2 = '{
                en:    TRUE,
                idx:   dec_data.rs2.val.as_r.idx,
                is_bn: dec_data.rs2.val.as_r.is_bn
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

    function automatic logic bget_rdy( input logic [4:0] reg_idx, input logic [TID_W-1:0] t);
        if (reg_idx == ZERO_REG) return 1'b1;
        else if (reg_idx < BREGISTERS) return b_ready_vals[BIDX_W'(BREGISTERS * t) + BIDX_W'(reg_idx)];
        else return 1'b1;
    endfunction : bget_rdy

    function automatic logic sget_rdy( input logic [4:0] reg_idx, input logic [TID_W-1:0] t);
        if (reg_idx == ZERO_REG || reg_idx == TID_REG) return 1'b1;
        else return s_ready_vals[SIDX_W'(REGISTERS * t) + SIDX_W'(reg_idx)];
    endfunction : sget_rdy


    logic operands_rdy, read_stall, bshfl_stall, can_issue, buff_rdy;
    logic operand1_rdy, operand2_rdy;

    assign bshfl_stall = (dec_data.eu_tag == EU_BALU)
                       & (dec_data.op_tag == OP_BSHFL);

    assign operand1_rdy =
        (rs1.en ?
        (rs1.is_bn ? bget_rdy(rs1.idx, dec_data.tid) : sget_rdy(rs1.idx, dec_data.tid)) :
        1'b1);

    assign operand2_rdy =
        (rs2.en ?
        (rs2.is_bn ? bget_rdy(rs2.idx, dec_data.tid) : sget_rdy(rs2.idx, dec_data.tid)) :
        1'b1);

    assign operands_rdy = operand1_rdy & operand2_rdy;

    assign read_stall =
        (rs1.en & rs2.en) & (rs1.is_bn & rs2.is_bn) &
        (rs1.idx[1:0] == rs2.idx[1:0]) &
        (rs1.idx < BREGISTERS && rs2.idx < BREGISTERS);

    assign can_issue = decode_if.valid & operands_rdy;

    assign iss_back_data = '{
        issued: can_issue & buff_rdy,
        tid:    dec_data.tid
    };
    assign iss_back_to_decode_if.data = iss_back_data;
    assign iss_back_to_decode_if.valid = decode_if.valid;

    assign decode_if.ready = buff_rdy;

    always_ff @(posedge clk) begin
        if (rst) begin
            s_ready_vals <= '1;
            b_ready_vals <= '1;
        end
        else begin

            if (wbA.en && wbA.rd < BREGISTERS) begin
                b_ready_vals[BIDX_W'(BREGISTERS * wbA.tid) + BIDX_W'(wbA.rd)] <= 1'b1;
            end

            if (wbB.en && wbB.rd < BREGISTERS) begin
                b_ready_vals[BIDX_W'(BREGISTERS * wbB.tid) + BIDX_W'(wbB.rd)] <= 1'b1;
            end

            if (wbS.en) begin
                s_ready_vals[SIDX_W'(REGISTERS * wbS.tid) + SIDX_W'(wbS.rd)] <= 1'b1;
            end

            if (decode_if.ready & can_issue) begin

                if (rd.en && rd.is_bn && rd.idx < BREGISTERS) begin
                    b_ready_vals[BIDX_W'(BREGISTERS * dec_data.tid) + BIDX_W'(rd.idx)] <= 1'b0;
                end
                else if (rd.en && ~rd.is_bn && (rd.idx != ZERO_REG) && (rd.idx != TID_REG)) begin
                    s_ready_vals[SIDX_W'(REGISTERS * dec_data.tid) + SIDX_W'(rd.idx)] <= 1'b0;
                end
            end

        end
    end

    scoreboard_out_t scb_out;
    assign scb_out = '{
        tid:         dec_data.tid,
        pc:          dec_data.pc,
        eu_tag:      dec_data.eu_tag,
        op_tag:      dec_data.op_tag,
        rs1:         dec_data.rs1,
        rs2:         dec_data.rs2,
        rd:          dec_data.rd,
        rd_is_rs:    dec_data.rd_is_rs,
        read_stall:  read_stall,
        bshfl_stall: bshfl_stall

        `ifdef DEBUG
        ,instr: dec_data.instr
        `endif
    };

    skid_buffer #(
        .DATA_W($bits(scoreboard_out_t))
    ) scoreboard_pipe_buff (
        .clk(clk),
        .rst(rst),

        .valid_in(can_issue),
        .ready_in(buff_rdy),
        .data_in (scb_out),

        .valid_out(iss_if.valid),
        .ready_out(iss_if.ready),
        .data_out (iss_if.data)
    );

    // synthesis translate_off

    // Debug Assertions
    /*
    property track_operands_not_rdy;
        @(posedge clk) disable iff (rst)
        decode_if.valid |-> operands_rdy;
    endproperty

    assert property (track_operands_not_rdy) else
        $info("Operands_not_RDY, TID[%0d] | RS1: rdy: %b (idx: %0d),  RS2: rdy: %b (idx: %0d)",
        dec_data.tid,
        operand1_rdy, rs1.idx,
        operand2_rdy, rs2.idx);

    property track_writeback_of_breg;
        @(posedge clk) disable iff (rst) (wbA.en | wbB.en)
    endproperty

    assert property (track_writeback_of_breg)
        $info("Writeback of A: TID[%0d] BREG[v%0d], B: TID[%0d] BREG[v%0d]",
            (wbA.en ? wbA.tid : 0), (wbA.en ? wbA.rd : 0),
            (wbB.en ? wbB.tid : 0), (wbB.en ? wbB.rd : 0));

    property track_writeback_of_sreg;
        @(posedge clk) disable iff (rst) wbS.en
    endproperty

    assert property (track_writeback_of_sreg)
        $info("Writeback of TID[%0d] SREG[x%0d]", wbS.tid, wbS.rd);
    */

    // Protection Assertions
    property check_iss_b_rs1;
        @(posedge clk) disable iff (rst)
        (decode_if.valid & rs1.en & rs1.is_bn) |-> (rs1.idx < BREGISTERS);
    endproperty
    property check_iss_b_rs2;
        @(posedge clk) disable iff (rst)
        (decode_if.valid & rs2.en & rs2.is_bn) |-> (rs2.idx < BREGISTERS);
    endproperty

    assert property (check_iss_b_rs1) else
    $error("SCOREBOARD: Bgn rs1 out of bounds. IDX: %0d, EU_TAG: %s, OP_TAG: %s, TID: %0d, PC: 0x%0X",
    rs1.idx, dec_data.eu_tag.name(), dec_data.op_tag.name(), dec_data.tid, dec_data.pc);

    assert property (check_iss_b_rs2) else
    $error("SCOREBOARD: Bgn rs2 out of bounds. IDX: %0d, EU_TAG: %s, OP_TAG: %s, TID: %0d, PC: 0x%0X",
    rs2.idx, dec_data.eu_tag.name(), dec_data.op_tag.name(), dec_data.tid, dec_data.pc);
    // synthesis translate_on

endmodule : scoreboard
