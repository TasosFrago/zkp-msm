module sregfile
    import zbp_pkg::*;
(
    input clk,
    input rst,

    // ======== Writer Interface ========
    input logic [      W-1:0] wr_data,
    input logic               wr_en,
    input logic [  TID_W-1:0] wr_tid,
    input logic [R_IDX_W-1:0] wr_reg,

    // ======== Reader Interface ========
    input  logic [  TID_W-1:0] rd_tid,
    output logic [      W-1:0] rd_data_A,
    input  logic               rd_en_A,
    input  logic [R_IDX_W-1:0] rd_reg_A,

    output logic [      W-1:0] rd_data_B,
    input  logic               rd_en_B,
    input  logic [R_IDX_W-1:0] rd_reg_B
);
    localparam int L_IDX_W = $clog2(MAX_THREADS * REGISTERS);

    logic [W-1:0] regs[MAX_THREADS * REGISTERS];

    // ========== Write Port ===========
    always_ff @(posedge clk) begin
        if (wr_en) begin
            if (wr_reg == ZERO_REG || wr_reg == TID_REG); // zero or tid ignore
            else begin
                regs[L_IDX_W'(REGISTERS * wr_tid) + L_IDX_W'(wr_reg)] <= wr_data;
            end
        end
    end

    // ========== Read Port A ===========
    always_ff @(posedge clk) begin
        if (rd_en_A) begin
            if (rd_reg_A == ZERO_REG)      rd_data_A <= '0;
            else if (rd_reg_A == TID_REG)  rd_data_A <= W'(rd_tid);
            else begin
                rd_data_A <= regs[L_IDX_W'(REGISTERS * rd_tid) + L_IDX_W'(rd_reg_A)];
            end
        end
    end

    // ========== Read Port B ===========
    always_ff @(posedge clk) begin
        if (rd_en_B) begin
            if (rd_reg_B == ZERO_REG)     rd_data_B <= '0;
            else if (rd_reg_B == TID_REG) rd_data_B <= W'(rd_tid);
            else begin
                rd_data_B <= regs[L_IDX_W'(REGISTERS * rd_tid) + L_IDX_W'(rd_reg_B)];
            end
        end
    end

endmodule : sregfile
