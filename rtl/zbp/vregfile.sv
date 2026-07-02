module vregfile
    import zbp_pkg::*;
(
    input logic clk,
    input logic rst,

    // =========== Writer Interface ===========
    input  logic [NUMBER_SIZE-1:0] wr_data_A,
    input  logic                   wr_en_A,
    input  logic [      TID_W-1:0] wr_tid_A,
    input  logic [    R_IDX_W-1:0] wr_reg_A,

    input  logic [NUMBER_SIZE-1:0] wr_data_B,
    input  logic                   wr_en_B,
    input  logic [      TID_W-1:0] wr_tid_B,
    input  logic [    R_IDX_W-1:0] wr_reg_B,

    // =========== Reader Interface ===========
    input  logic [      TID_W-1:0] rd_tid,

    output logic [NUMBER_SIZE-1:0] rd_data_A,
    input  logic                   rd_en_A,
    input  logic [    R_IDX_W-1:0] rd_reg_A,

    output logic [NUMBER_SIZE-1:0] rd_data_B,
    input  logic                   rd_en_B,
    input  logic [    R_IDX_W-1:0] rd_reg_B
);

    localparam int BANK_SIZE = 32'(VREGISTERS) / 4;

    logic [NUMBER_SIZE-1:0] mem_bank0[MAX_THREADS * BANK_SIZE];
    logic [NUMBER_SIZE-1:0] mem_bank1[MAX_THREADS * BANK_SIZE];
    logic [NUMBER_SIZE-1:0] mem_bank2[MAX_THREADS * BANK_SIZE];
    logic [NUMBER_SIZE-1:0] mem_bank3[MAX_THREADS * BANK_SIZE];

    localparam BANK_IDX_W = $clog2(MAX_THREADS * BANK_SIZE);

    logic [BANK_IDX_W-1:0] b_idx_wr_A, b_idx_wr_B;
    logic [BANK_IDX_W-1:0] b_idx_rd_A, b_idx_rd_B;

    assign b_idx_wr_A = BANK_IDX_W'(wr_tid_A * BANK_SIZE) + BANK_IDX_W'(wr_reg_A[R_IDX_W-1:2]);
    assign b_idx_wr_B = BANK_IDX_W'(wr_tid_B * BANK_SIZE) + BANK_IDX_W'(wr_reg_B[R_IDX_W-1:2]);
    assign b_idx_rd_A = BANK_IDX_W'(rd_tid   * BANK_SIZE) + BANK_IDX_W'(rd_reg_A[R_IDX_W-1:2]);
    assign b_idx_rd_B = BANK_IDX_W'(rd_tid   * BANK_SIZE) + BANK_IDX_W'(rd_reg_B[R_IDX_W-1:2]);

    always_ff @(posedge clk) begin
        // Write Port A
        if (wr_en_A) begin
            if (wr_reg_A == ZERO_REG);  // Zero ignore write
            else begin
                unique case (wr_reg_A[1:0])
                    2'd0: mem_bank0[b_idx_wr_A] <= wr_data_A;
                    2'd1: mem_bank1[b_idx_wr_A] <= wr_data_A;
                    2'd2: mem_bank2[b_idx_wr_A] <= wr_data_A;
                    2'd3: mem_bank3[b_idx_wr_A] <= wr_data_A;
                endcase
            end
        end

        // Write Port B
        if (wr_en_B) begin
            if (wr_reg_B == ZERO_REG);  // Zero ignore write
            else begin
                unique case (wr_reg_B[1:0])
                    2'd0: mem_bank0[b_idx_wr_B] <= wr_data_B;
                    2'd1: mem_bank1[b_idx_wr_B] <= wr_data_B;
                    2'd2: mem_bank2[b_idx_wr_B] <= wr_data_B;
                    2'd3: mem_bank3[b_idx_wr_B] <= wr_data_B;
                endcase
            end
        end
    end

    always_ff @(posedge clk) begin
        // Read Port A
        if (rd_en_A) begin
            if (rd_reg_A == ZERO_REG)  rd_data_A <= '0;
            else begin
                unique case (rd_reg_A[1:0])
                    2'd0: rd_data_A <= mem_bank0[b_idx_rd_A];
                    2'd1: rd_data_A <= mem_bank1[b_idx_rd_A];
                    2'd2: rd_data_A <= mem_bank2[b_idx_rd_A];
                    2'd3: rd_data_A <= mem_bank3[b_idx_rd_A];
                endcase
            end
        end

        // Read Port B
        if (rd_en_B) begin
            if (rd_reg_B == ZERO_REG)  rd_data_B <= '0;
            else begin
                unique case (rd_reg_B[1:0])
                    2'd0: rd_data_B <= mem_bank0[b_idx_rd_B];
                    2'd1: rd_data_B <= mem_bank1[b_idx_rd_B];
                    2'd2: rd_data_B <= mem_bank2[b_idx_rd_B];
                    2'd3: rd_data_B <= mem_bank3[b_idx_rd_B];
                endcase
            end
        end
    end

    // syntesis translate_off
    property check_wr_A; @(posedge clk) disable iff (rst) wr_en_A |-> (wr_reg_A < VREGISTERS); endproperty
    property check_wr_B; @(posedge clk) disable iff (rst) wr_en_B |-> (wr_reg_B < VREGISTERS); endproperty
    property check_rd_A; @(posedge clk) disable iff (rst) rd_en_A |-> (rd_reg_A < VREGISTERS); endproperty
    property check_rd_B; @(posedge clk) disable iff (rst) rd_en_B |-> (rd_reg_B < VREGISTERS); endproperty

    assert property (check_wr_A) else $error("Vector Regfile: wr_reg_A out of bounds (> %0d) | Reg: %0d", VREGISTERS, wr_reg_A);
    assert property (check_wr_B) else $error("Vector Regfile: wr_reg_B out of bounds (> %0d) | Reg: %0d", VREGISTERS, wr_reg_B);
    assert property (check_rd_A) else $error("Vector Regfile: rd_reg_A out of bounds (> %0d) | Reg: %0d", VREGISTERS, rd_reg_A);
    assert property (check_rd_B) else $error("Vector Regfile: rd_reg_B out of bounds (> %0d) | Reg: %0d", VREGISTERS, rd_reg_B);
    // syntesis translate_on

endmodule : vregfile
