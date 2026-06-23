module scoreboard
    import regfile_pkg::*;
#(
    parameter int MAX_THREADS = 32,

    parameter int LAT_MUL = 24,
    parameter int LAT_ADD = 9,

    localparam int R_IDX_W = (VR_IDX_W > SR_IDX_W) ? VR_IDX_W : SR_IDX_W
) (
    input logic clk,
    input logic rst,

    // =============== Issue Interface ================
    input logic                           iss_req,
    input eu_tag_t                        iss_eu_tag,
    input logic [$clog2(MAX_THREADS)-1:0] iss_tid,
    input logic                           iss_rsA_en,
    input logic [            R_IDX_W-1:0] iss_rsA,
    input logic                           iss_rsA_is_v,
    input logic                           iss_rsB_en,
    input logic [            R_IDX_W-1:0] iss_rsB,
    input logic                           iss_rsB_is_v,
    input logic [            R_IDX_W-1:0] iss_rd,
    input logic                           iss_rd_is_v,

    output logic issue_valid,
    output logic read_stall,

    // ============= Writeback Interface ==============
    input logic                           wb_v_en_A,
    input logic [$clog2(MAX_THREADS)-1:0] wb_v_tid_A,
    input logic [            R_IDX_W-1:0] wb_v_reg_A,

    input logic                           wb_v_en_B,
    input logic [$clog2(MAX_THREADS)-1:0] wb_v_tid_B,
    input logic [            R_IDX_W-1:0] wb_v_reg_B,

    input logic                           wb_s_en,
    input logic [$clog2(MAX_THREADS)-1:0] wb_s_tid,
    input logic [            R_IDX_W-1:0] wb_s_reg
);

    logic [(S_L_REGS * MAX_THREADS)-1:0] sl_ready_vals;
    logic [                S_G_REGS-1:0] sg_ready_vals;

    logic [(V_L_REGS * MAX_THREADS)-1:0] vl_ready_vals;
    logic [                V_G_REGS-1:0] vg_ready_vals;

    logic [31:0] wb_bank_tracker[4];
    logic [31:0] wb_port_tracker[2];

    logic operands_rdy, write_collision;

    logic [4:0] exec_lat;

    always_comb begin
        case (iss_eu_tag)
            EU_ADD: exec_lat = LAT_ADD;
            EU_MUL: exec_lat = LAT_MUL;
            default: exec_lat = 'd1;
        endcase
    end

    function automatic logic vget_rdy_status(input logic [R_IDX_W-1:0] reg_idx);
        if (reg_idx == VZERO_REG) return 1'b1;
        else if (reg_idx >= VG_REG_START && reg_idx <= VG_REG_END) return vg_ready_vals[reg_idx-VG_REG_START];
        else return vl_ready_vals[(V_L_REGS*iss_tid)+(reg_idx - VL_REG_START)];
    endfunction : vget_rdy_status

    function automatic logic sget_rdy_status(input logic [R_IDX_W-1:0] reg_idx);
        if (reg_idx == SZERO_REG || reg_idx == STID_REG) return 1'b1;
        else if (reg_idx >= SG_REG_START && reg_idx <= SG_REG_END) return sg_ready_vals[reg_idx - SG_REG_START];
        else return sl_ready_vals[(S_L_REGS*iss_tid) + (reg_idx-SL_REG_START)];
    endfunction : sget_rdy_status

    assign operands_rdy =
        (iss_rsA_en ?
        (iss_rsA_is_v ? vget_rdy_status(iss_rsA) : sget_rdy_status(iss_rsA)) :
        1'b1) &
        (iss_rsB_en ?
        (iss_rsB_is_v ? vget_rdy_status(iss_rsB) : sget_rdy_status(iss_rsB)) :
        1'b1);

    assign read_stall = iss_req &
        (iss_rsA_en & iss_rsB_en) &
        (iss_rsA_is_v & iss_rsB_is_v) &
        (iss_rsA[1:0] == iss_rsB[1:0]) &
        (iss_rsA >= VL_REG_START && iss_rsB >= VL_REG_START);

    assign write_collision = iss_rd_is_v & (iss_rd >= VL_REG_START) &
        (wb_bank_tracker[iss_rd[1:0]][exec_lat] |
        (wb_port_tracker[0][exec_lat] & wb_port_tracker[1][exec_lat]));

    assign issue_valid = iss_req & operands_rdy & ~write_collision;

    always_ff @(posedge clk) begin
        if(rst) begin
            wb_bank_tracker <= '{default: '0};
            wb_port_tracker <= '{default: '0};

            sl_ready_vals <= '0;
            sg_ready_vals <= '0;
            vl_ready_vals <= '0;
            vg_ready_vals <= '0;
        end
        else begin

            for(int b = 0; b < 4; b++) begin
                wb_bank_tracker[b] <= wb_bank_tracker[b] >> 1;
            end
            for(int p = 0; p < 2; p++) begin
                wb_port_tracker[p] <= wb_port_tracker[p] >> 1;
            end

            if (wb_v_en_A) begin
                if(wb_v_reg_A >= VG_REG_START && wb_v_reg_A <= VG_REG_END) begin
                    vg_ready_vals[wb_v_reg_A - VG_REG_START] <= 1'b1;
                end
                else if (wb_v_reg_A >= VL_REG_START) begin
                    vl_ready_vals[(V_L_REGS*wb_v_tid_A) + (wb_v_reg_A - VL_REG_START)] <= 1'b1;
                end
            end

            if (wb_v_en_B) begin
                if(wb_v_reg_B >= VG_REG_START && wb_v_reg_B <= VG_REG_END) begin
                    vg_ready_vals[wb_v_reg_B - VG_REG_START] <= 1'b1;
                end
                else if (wb_v_reg_B >= VL_REG_START) begin
                    vl_ready_vals[(V_L_REGS*wb_v_tid_B) + (wb_v_reg_B - VL_REG_START)] <= 1'b1;
                end
            end

            if (wb_s_en) begin
                if (wb_s_reg >= SG_REG_START && wb_s_reg <= SG_REG_END) begin
                    sg_ready_vals[wb_s_reg - SG_REG_START] <= 1'b1;
                end
                else if (wb_s_reg >= SL_REG_START) begin
                    sl_ready_vals[(S_L_REGS * wb_s_tid) + (wb_s_reg - SL_REG_START)] <= 1'b1;
                end
            end

            // Issue Logic takes precident over writeback logic when writting
            // to the ready states.
            if (issue_valid) begin

                if (iss_rd_is_v) begin
                    if (iss_rd >= VG_REG_START && iss_rd <= VG_REG_END) vg_ready_vals[iss_rd - VG_REG_START] <= 1'b0;
                    else if (iss_rd >= VL_REG_START) vl_ready_vals[(V_L_REGS*iss_tid) + (iss_rd - VL_REG_START)] <= 1'b0;
                end
                else begin
                    if (iss_rd >= SG_REG_START && iss_rd <= SG_REG_END) sg_ready_vals[iss_rd - SG_REG_START] <= 1'b0;
                    else if (iss_rd >= SL_REG_START) sl_ready_vals[(S_L_REGS * iss_tid) + (iss_rd - SL_REG_START)] <= 1'b0;
                end

                if (iss_rd_is_v && (iss_rd >= VL_REG_START)) begin
                    wb_bank_tracker[iss_rd[1:0]][exec_lat] <= 1'b1;

                    if (~wb_port_tracker[0][exec_lat]) begin
                        wb_port_tracker[0][exec_lat] <= 1'b1;
                    end
                    else begin
                        assert(~wb_port_tracker[1][exec_lat])
                        else $fatal(1, "scoreboard Fatal: Both write ports already allocated.");
                        wb_port_tracker[1][exec_lat] <= 1'b1;
                    end
                end
            end

        end
    end

    // synthesis translate_off
    property check_iss_v_rsA; @(posedge clk) disable iff (rst) (iss_req & iss_rsA_en & iss_rsA_is_v) |-> (iss_rsA < V_TOTAL_REGS); endproperty
    property check_iss_s_rsA; @(posedge clk) disable iff (rst) (iss_req & iss_rsA_en & ~iss_rsA_is_v) |-> (iss_rsA < S_TOTAL_REGS); endproperty
    property check_iss_v_rsB; @(posedge clk) disable iff (rst) (iss_req & iss_rsB_en & iss_rsB_is_v) |-> (iss_rsB < V_TOTAL_REGS); endproperty
    property check_iss_s_rsB; @(posedge clk) disable iff (rst) (iss_req & iss_rsB_en & ~iss_rsB_is_v) |-> (iss_rsB < S_TOTAL_REGS); endproperty

    assert property (check_iss_v_rsA) else $error("Scoreboard: Vector rsA out of bounds");
    assert property (check_iss_s_rsA) else $error("Scoreboard: Scalar rsA out of bounds");
    assert property (check_iss_v_rsB) else $error("Scoreboard: Vector rsB out of bounds");
    assert property (check_iss_s_rsB) else $error("Scoreboard: Scalar rsB out of bounds");
    // synthesis translate_on

endmodule : scoreboard
