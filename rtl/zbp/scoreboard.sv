module scoreboard
    import zbp_pkg::*;
(
    input logic clk,
    input logic rst,

    pipeline_if.in  decode_if,
    pipeline_if.out iss_if,

    // Writeback Interface
    input wb_tag_t wbA,
    input wb_tag_t wbB,

    input wb_tag_t wbS  // Writeback to scalar reg
);

    localparam int SIDX_W = $clog2(REGISTERS * MAX_THREADS);
    localparam int VIDX_W = $clog2(VREGISTERS * MAX_THREADS);

    logic [(REGISTERS * MAX_THREADS)-1:0]  s_ready_vals;
    logic [(VREGISTERS * MAX_THREADS)-1:0] v_ready_vals;

    logic [31:0] wb_bank_tracker[4];
    logic [31:0] wb_port_tracker[2];

    decode_out_t dec_data;
    assign dec_data = '{
        tid:      decode_if.data.tid,
        pc:       decode_if.data.pc,
        eu_tag:   decode_if.data.eu_tag,
        op_tag:   decode_if.data.op_tag,
        rs1:      decode_if.data.rs1,
        rs2:      decode_if.data.rs2,
        rd:       decode_if.data.rd,
        rd_is_rs: decode_if.data.rd_is_rs
    };

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

    function automatic logic vget_rdy( input logic [4:0] reg_idx, input logic [TID_W-1:0] t);
        if (reg_idx == ZERO_REG) return 1'b1;
        else if (reg_idx < VREGISTERS) return v_ready_vals[VIDX_W'(VREGISTERS * t) + VIDX_W'(reg_idx)];
        else return 1'b1;
    endfunction : vget_rdy

    function automatic logic sget_rdy( input logic [4:0] reg_idx, input logic [TID_W-1:0] t);
        if (reg_idx == ZERO_REG || reg_idx == TID_REG) return 1'b1;
        else return s_ready_vals[SIDX_W'(REGISTERS * t) + SIDX_W'(reg_idx)];
    endfunction : sget_rdy


    logic operands_rdy, read_stall, write_collision, can_issue, buff_rdy;
    logic [4:0] eu_lat, exec_lat;

    always_comb begin
        // TODO: Come back and correct the latencies when finishing the exec
        // stage
        case (dec_data.eu_tag)
            EU_VMADD: eu_lat = 'd9;
            EU_VMMUL: eu_lat = 'd24;
            EU_VCMP:  eu_lat = 'd2;
            default:  eu_lat = 'd1;
        endcase
    end

    assign operands_rdy =
        (rs1.en ?
        (rs1.is_v ? vget_rdy(rs1.idx, dec_data.tid) : sget_rdy(rs1.idx, dec_data.tid)) :
        1'b1) &
        (rs2.en ?
        (rs2.is_v ? vget_rdy(rs2.idx, dec_data.tid) : sget_rdy(rs2.idx, dec_data.tid)) :
        1'b1);

    assign read_stall =
        (rs1.en & rs2.en) & (rs1.is_v & rs2.is_v) &
        (rs1.idx[1:0] == rs2.idx[1:0]) &
        (rs1.idx < VREGISTERS && rs2.idx < VREGISTERS);

    assign exec_lat = eu_lat + (read_stall ? 5'd1 : 5'd0);

    assign write_collision = rd.en & rd.is_v & (rd.idx < VREGISTERS) &
        (wb_bank_tracker[rd.idx[1:0]][exec_lat] |
        (wb_port_tracker[0][exec_lat] & wb_port_tracker[1][exec_lat]));

    assign can_issue = decode_if.valid & operands_rdy & ~write_collision;

    assign decode_if.ready = can_issue & buff_rdy;

    always_ff @(posedge clk) begin
        if (rst) begin
            wb_bank_tracker <= '{default: '0};
            wb_port_tracker <= '{default: '0};

            s_ready_vals <= '0;
            v_ready_vals <= '0;
        end
        else begin

            for(int b = 0; b < 4; b++) begin
                wb_bank_tracker[b] <= wb_bank_tracker[b] >> 1;
            end
            for(int p = 0; p < 2; p++) begin
                wb_port_tracker[p] <= wb_port_tracker[p] >> 1;
            end

            if (wbA.en && wbA.rd < VREGISTERS) begin
                v_ready_vals[VIDX_W'(VREGISTERS * wbA.tid) + VIDX_W'(wbA.rd)] <= 1'b1;
            end

            if (wbB.en && wbB.rd < VREGISTERS) begin
                v_ready_vals[VIDX_W'(VREGISTERS * wbB.tid) + VIDX_W'(wbB.rd)] <= 1'b1;
            end

            if (wbS.en) begin
                s_ready_vals[SIDX_W'(REGISTERS * wbS.tid) + SIDX_W'(wbS.rd)] <= 1'b1;
            end

            if (decode_if.ready) begin

                if (rd.en && rd.is_v && rd.idx < VREGISTERS) begin
                    v_ready_vals[VIDX_W'(VREGISTERS * dec_data.tid) + VIDX_W'(rd.idx)] <= 1'b0;

                    wb_bank_tracker[rd.idx[1:0]][exec_lat] <= 1'b1;

                    if (~wb_port_tracker[0][exec_lat]) begin
                        wb_port_tracker[0] [exec_lat] <= 1'b1;
                    end
                    else begin
                        assert(~wb_port_tracker[1][exec_lat])
                        else $fatal(1, "scoreboard Fatal: Both write ports already allocated.");
                        wb_port_tracker[1][exec_lat] <= 1'b1;
                    end
                end
                else if (rd.en && ~rd.is_v && rd.idx != ZERO_REG && rd.idx != TID_REG) begin
                    s_ready_vals[SIDX_W'(REGISTERS * dec_data.tid) + SIDX_W'(rd.idx)] <= 1'b0;
                end
            end

        end
    end

    scoreboard_out_t scb_out;
    assign scb_out = '{
        tid:        dec_data.tid,
        pc:         dec_data.pc,
        eu_tag:     dec_data.eu_tag,
        op_tag:     dec_data.op_tag,
        rs1:        dec_data.rs1,
        rs2:        dec_data.rs2,
        rd:         dec_data.rd,
        rd_is_rs:   dec_data.rd_is_rs,
        read_stall: read_stall
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
    property check_iss_v_rs1; @(posedge clk) disable iff (rst) (decode_if.valid & rs1.en & rs1.is_v) |-> (rs1.idx < VREGISTERS); endproperty
    property check_iss_v_rs2; @(posedge clk) disable iff (rst) (decode_if.valid & rs2.en & rs2.is_v) |-> (rs2.idx < VREGISTERS); endproperty


    assert property (check_iss_v_rs1) else $error("SCOREBOARD: Vector rs1 out of bounds");
    assert property (check_iss_v_rs2) else $error("SCOREBOARD: Vector rs2 out of bounds");
    // synthesis translate_on

endmodule : scoreboard
