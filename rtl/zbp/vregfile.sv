module vregfile
    import regfile_pkg::*;
#(
    parameter int NUMBER_SIZE = 256,
    parameter int W = 32,
    parameter int MAX_THREADS = 32
) (
    input logic clk,
    input logic rst,

    // =============== Writer Interface ===============
    input logic [        NUMBER_SIZE-1:0] wr_data_A,
    input logic                           wr_en_A,
    input logic [$clog2(MAX_THREADS)-1:0] wr_tid_A,
    input logic [           VR_IDX_W-1:0] wr_reg_A,

    input logic [        NUMBER_SIZE-1:0] wr_data_B,
    input logic                           wr_en_B,
    input logic [$clog2(MAX_THREADS)-1:0] wr_tid_B,
    input logic [           VR_IDX_W-1:0] wr_reg_B,

    // =============== Reader Interface ===============
    output logic [        NUMBER_SIZE-1:0] rd_data_A,
    input  logic                           rd_en_A,
    input  logic [$clog2(MAX_THREADS)-1:0] rd_tid_A,
    input  logic [           VR_IDX_W-1:0] rd_reg_A,

    output logic [        NUMBER_SIZE-1:0] rd_data_B,
    input  logic                           rd_en_B,
    input  logic [$clog2(MAX_THREADS)-1:0] rd_tid_B,
    input  logic [           VR_IDX_W-1:0] rd_reg_B,

    // Modulus output
    output logic [NUMBER_SIZE-1:0] modulus_reg
);
    logic [NUMBER_SIZE-1:0] global_reg_0;
    logic [NUMBER_SIZE-1:0] global_reg_1;
    logic [NUMBER_SIZE-1:0] global_reg_2;

    logic [NUMBER_SIZE-1:0] mem_bank0[MAX_THREADS * (V_L_REGS / 4)];
    logic [NUMBER_SIZE-1:0] mem_bank1[MAX_THREADS * (V_L_REGS / 4)];
    logic [NUMBER_SIZE-1:0] mem_bank2[MAX_THREADS * (V_L_REGS / 4)];
    logic [NUMBER_SIZE-1:0] mem_bank3[MAX_THREADS * (V_L_REGS / 4)];

    localparam BANK_IDX_W = $clog2(MAX_THREADS * (V_L_REGS / 4));

    logic [BANK_IDX_W-1:0] b_idx_wr_A, b_idx_wr_B;
    logic [BANK_IDX_W-1:0] b_idx_rd_A, b_idx_rd_B;

    assign b_idx_wr_A  = BANK_IDX_W'(wr_tid_A * (V_L_REGS / 4)) +
                         BANK_IDX_W'(wr_reg_A[VR_IDX_W-1:2] > 0 ? wr_reg_A[VR_IDX_W-1:2] - 1 : 0);
    assign b_idx_wr_B  = BANK_IDX_W'(wr_tid_B * (V_L_REGS / 4)) +
                         BANK_IDX_W'(wr_reg_B[VR_IDX_W-1:2] > 0 ? wr_reg_B[VR_IDX_W-1:2] - 1 : 0);
    assign b_idx_rd_A  = BANK_IDX_W'(rd_tid_A * (V_L_REGS / 4)) +
                         BANK_IDX_W'(rd_reg_A[VR_IDX_W-1:2] > 0 ? wr_reg_A[VR_IDX_W-1:2] - 1 : 0);
    assign b_idx_rd_B  = BANK_IDX_W'(rd_tid_B * (V_L_REGS / 4)) +
                         BANK_IDX_W'(rd_reg_B[VR_IDX_W-1:2] > 0 ? wr_reg_B[VR_IDX_W-1:2] - 1 : 0);

    assign modulus_reg = global_reg_0;

    always_ff @(posedge clk) begin
        if (rst) begin
            global_reg_0 <= '0;
            global_reg_1 <= '0;
            global_reg_2 <= '0;
        end
        else begin
            // Write Port A
            if (wr_en_A) begin
                if (wr_reg_A == VZERO_REG);  // Zero ignore write
                else if (wr_reg_A == VG_REG_START)   global_reg_0 <= wr_data_A;
                else if (wr_reg_A == VG_REG_START+1) global_reg_1 <= wr_data_A;
                else if (wr_reg_A == VG_REG_START+2) global_reg_2 <= wr_data_A;
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
                if (wr_reg_B == VZERO_REG);  // Zero ignore write
                else if (wr_reg_B == VG_REG_START)   global_reg_0 <= wr_data_B;
                else if (wr_reg_B == VG_REG_START+1) global_reg_1 <= wr_data_B;
                else if (wr_reg_B == VG_REG_START+2) global_reg_2 <= wr_data_B;
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
    end

    always_ff @(posedge clk) begin
        // Read Port A
        if (rd_en_A) begin
            if (rd_reg_A == VZERO_REG)           rd_data_A <= '0;
            else if (rd_reg_A == VG_REG_START)   rd_data_A <= global_reg_0;
            else if (rd_reg_A == VG_REG_START+1) rd_data_A <= global_reg_1;
            else if (rd_reg_A == VG_REG_START+2) rd_data_A <= global_reg_2;
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
            if (rd_reg_B == VZERO_REG)           rd_data_B <= '0;
            else if (rd_reg_B == VG_REG_START)   rd_data_B <= global_reg_0;
            else if (rd_reg_B == VG_REG_START+1) rd_data_B <= global_reg_1;
            else if (rd_reg_B == VG_REG_START+2) rd_data_B <= global_reg_2;
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
    property check_wr_A; @(posedge clk) disable iff (rst) wr_en_A |-> (wr_reg_A <= VL_REG_END); endproperty
    property check_wr_B; @(posedge clk) disable iff (rst) wr_en_B |-> (wr_reg_B <= VL_REG_END); endproperty
    property check_rd_A; @(posedge clk) disable iff (rst) rd_en_A |-> (rd_reg_A <= VL_REG_END); endproperty
    property check_rd_B; @(posedge clk) disable iff (rst) rd_en_B |-> (rd_reg_B <= VL_REG_END); endproperty

    assert property (check_wr_A) else $error("Vector Regfile: wr_reg_A out of bounds (> %0d) | Reg: %0d", VL_REG_END, wr_reg_A);
    assert property (check_wr_B) else $error("Vector Regfile: wr_reg_B out of bounds (> %0d) | Reg: %0d", VL_REG_END, wr_reg_B);
    assert property (check_rd_A) else $error("Vector Regfile: rd_reg_A out of bounds (> %0d) | Reg: %0d", VL_REG_END, rd_reg_A);
    assert property (check_rd_B) else $error("Vector Regfile: rd_reg_B out of bounds (> %0d) | Reg: %0d", VL_REG_END, rd_reg_B);
    // syntesis translate_on

endmodule : vregfile
