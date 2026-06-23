module sregfile
    import regfile_pkg::*;
#(
    parameter int W = 32,
    parameter int MAX_THREADS = 32
) (
    input clk,
    input rst,

    // =============== Writer Interface ===============
    input logic [                  W-1:0] wr_data,
    input logic                           wr_en,
    input logic [$clog2(MAX_THREADS)-1:0] wr_tid,
    input logic [           SR_IDX_W-1:0] wr_reg,

    // =============== Reader Interface ===============
    output logic [                  W-1:0] rd_data_A,
    input  logic                           rd_en_A,
    input  logic [$clog2(MAX_THREADS)-1:0] rd_tid_A,
    input  logic [           SR_IDX_W-1:0] rd_reg_A,

    output logic [                  W-1:0] rd_data_B,
    input  logic                           rd_en_B,
    input  logic [$clog2(MAX_THREADS)-1:0] rd_tid_B,
    input  logic [           SR_IDX_W-1:0] rd_reg_B
);
    localparam int L_IDX_W = $clog2(MAX_THREADS * L_REGS);

    logic [W-1:0] global_regs[S_G_REGS];
    logic [W-1:0] local_regs[MAX_THREADS * S_L_REGS];

    // ========== Write Port ===========
    always_ff @(posedge clk) begin
        if (rst) begin
            global_regs <= '{default: '0};
        end
        else if (wr_en) begin
            if (wr_reg == SZERO_REG); // zero ignore
            else if (wr_reg == STID_REG); // tid ignore
            else if (wr_reg >= SG_REG_START && wr_reg <= SG_REG_END) begin
                global_regs[wr_reg - SG_REG_START] <= wr_data;
            end
            else begin
                local_regs[L_IDX_W'(wr_tid*S_L_REGS) + L_IDX_W'(wr_reg - SL_REG_START)] <= wr_data;
            end
        end
    end

    // ========== Read Port A ===========
    always_ff @(posedge clk) begin
        if (rd_en_A) begin
            if (rd_reg_A == SZERO_REG) rd_data_A <= '0;
            else if (rd_reg_A == STID_REG) rd_data_A <= W'(rd_tid_A);
            else if (rd_reg_A >= SG_REG_START && rd_reg_A <= SG_REG_END) begin
                rd_data_A <= global_regs[rd_reg_A - SG_REG_START];
            end
            else begin
                rd_data_A <= local_regs[L_IDX_W'(rd_tid_A * S_L_REGS) + L_IDX_W'(rd_reg_A - SL_REG_START)];
            end
        end
    end

    // ========== Read Port B ===========
    always_ff @(posedge clk) begin
        if (rd_en_B) begin
            if (rd_reg_B == SZERO_REG) rd_data_B <= '0;
            else if (rd_reg_B == STID_REG) rd_data_B <= W'(rd_tid_B);
            else if (rd_reg_B >= SG_REG_START && rd_reg_B <= SG_REG_END) begin
                rd_data_B <= global_regs[rd_reg_B - SG_REG_START];
            end
            else begin
                rd_data_B <= local_regs[L_IDX_W'(rd_tid_B * S_L_REGS) + L_IDX_W'(rd_reg_B - SL_REG_START)];
            end
        end
    end

endmodule : sregfile
