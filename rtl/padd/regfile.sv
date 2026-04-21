module regfile #(
    parameter int NUMBER_SIZE = 256,
    parameter int W = 32,
    parameter int MAX_THREADS = 10,

    parameter int BANK_SLOTS[4] = '{8, 5, 2, 2},
    parameter int MAX_DEPTH = 8
) (
    input logic clk,
    input logic rst,

    // =============== Writer Interface ===============
    input logic [        NUMBER_SIZE-1:0] wr_data_A,
    input logic [        NUMBER_SIZE-1:0] wr_data_B,
    input logic                           wr_en_A,
    input logic                           wr_en_B,
    input logic [                    1:0] wr_bank_A,
    input logic [                    1:0] wr_bank_B,
    input logic [$clog2(MAX_THREADS)-1:0] wr_tid_A,
    input logic [$clog2(MAX_THREADS)-1:0] wr_tid_B,
    input logic [  $clog2(MAX_DEPTH)-1:0] wr_idx_A,
    input logic [  $clog2(MAX_DEPTH)-1:0] wr_idx_B,

    // =============== Reader Interface ===============
    output logic [        NUMBER_SIZE-1:0] rd_data_A,
    output logic [        NUMBER_SIZE-1:0] rd_data_B,
    input  logic                           rd_en_A,
    input  logic                           rd_en_B,
    input  logic [                    1:0] rd_bank_A,
    input  logic [                    1:0] rd_bank_B,
    input  logic [$clog2(MAX_THREADS)-1:0] rd_tid_A,
    input  logic [$clog2(MAX_THREADS)-1:0] rd_tid_B,
    input  logic [  $clog2(MAX_DEPTH)-1:0] rd_idx_A,
    input  logic [  $clog2(MAX_DEPTH)-1:0] rd_idx_B,

    // ============= Vld Reader Interface =============
    output logic                           rd_vld_A,
    output logic                           rd_vld_B,
    input  logic [                    1:0] rd_vld_bank_A,
    input  logic [                    1:0] rd_vld_bank_B,
    input  logic [$clog2(MAX_THREADS)-1:0] rd_vld_tid_A,
    input  logic [$clog2(MAX_THREADS)-1:0] rd_vld_tid_B,
    input  logic [  $clog2(MAX_DEPTH)-1:0] rd_vld_idx_A,
    input  logic [  $clog2(MAX_DEPTH)-1:0] rd_vld_idx_B,

    input logic                           flush_vld,
    input logic [$clog2(MAX_THREADS)-1:0] flush_tid,
    input logic [                    3:0] flush_bank_mask
);

    logic [NUMBER_SIZE-1:0] mem_bank0[MAX_THREADS * BANK_SLOTS[0]];
    logic [NUMBER_SIZE-1:0] mem_bank1[MAX_THREADS * BANK_SLOTS[1]];
    logic [NUMBER_SIZE-1:0] mem_bank2[MAX_THREADS * BANK_SLOTS[2]];
    logic [NUMBER_SIZE-1:0] mem_bank3[MAX_THREADS * BANK_SLOTS[3]];

    logic [BANK_SLOTS[0]-1:0] vld_bank0[MAX_THREADS];
    logic [BANK_SLOTS[1]-1:0] vld_bank1[MAX_THREADS];
    logic [BANK_SLOTS[2]-1:0] vld_bank2[MAX_THREADS];
    logic [BANK_SLOTS[3]-1:0] vld_bank3[MAX_THREADS];

    // WRITE Data Port
    always_ff @(posedge clk) begin
        // Write Port A
        if (wr_en_A) begin
            unique case (wr_bank_A)
                2'd0: mem_bank0[(wr_tid_A*BANK_SLOTS[0])+wr_idx_A] <= wr_data_A;
                2'd1: mem_bank1[(wr_tid_A*BANK_SLOTS[1])+wr_idx_A] <= wr_data_A;
                2'd2: mem_bank2[(wr_tid_A*BANK_SLOTS[2])+wr_idx_A] <= wr_data_A;
                2'd3: mem_bank3[(wr_tid_A*BANK_SLOTS[3])+wr_idx_A] <= wr_data_A;
            endcase
        end

        // Write Port B
        if (wr_en_B) begin
            unique case (wr_bank_B)
                2'd0: mem_bank0[(wr_tid_B*BANK_SLOTS[0])+wr_idx_B] <= wr_data_B;
                2'd1: mem_bank1[(wr_tid_B*BANK_SLOTS[1])+wr_idx_B] <= wr_data_B;
                2'd2: mem_bank2[(wr_tid_B*BANK_SLOTS[2])+wr_idx_B] <= wr_data_B;
                2'd3: mem_bank3[(wr_tid_B*BANK_SLOTS[3])+wr_idx_B] <= wr_data_B;
            endcase
        end
    end

    // WRITE VLD Port
    always_ff @(posedge clk) begin
        if (rst) begin
            vld_bank0 <= '{default: '0};
            vld_bank1 <= '{default: '0};
            vld_bank2 <= '{default: '0};
            vld_bank3 <= '{default: '0};
        end
        else if (flush_vld) begin
            if (flush_bank_mask[0]) vld_bank0[flush_tid] <= '0;
            if (flush_bank_mask[1]) vld_bank1[flush_tid] <= '0;
            if (flush_bank_mask[2]) vld_bank2[flush_tid] <= '0;
            if (flush_bank_mask[3]) vld_bank3[flush_tid] <= '0;
        end
        else begin
            if (wr_en_A) begin
                unique case (wr_bank_A)
                    2'd0: vld_bank0[wr_tid_A][wr_idx_A] <= 1'b1;
                    2'd1: vld_bank1[wr_tid_A][wr_idx_A] <= 1'b1;
                    2'd2: vld_bank2[wr_tid_A][wr_idx_A] <= 1'b1;
                    2'd3: vld_bank3[wr_tid_A][wr_idx_A] <= 1'b1;
                endcase
            end

            if (wr_en_B) begin
                unique case (wr_bank_B)
                    2'd0: vld_bank0[wr_tid_B][wr_idx_B] <= 1'b1;
                    2'd1: vld_bank1[wr_tid_B][wr_idx_B] <= 1'b1;
                    2'd2: vld_bank2[wr_tid_B][wr_idx_B] <= 1'b1;
                    2'd3: vld_bank3[wr_tid_B][wr_idx_B] <= 1'b1;
                endcase
            end
        end
    end

    // READ Data Port
    always_ff @(posedge clk) begin
        // Read Port A
        if (rd_en_A) begin
            unique case (rd_bank_A)
                2'd0: rd_data_A <= mem_bank0[(rd_tid_A*BANK_SLOTS[0])+rd_idx_A];
                2'd1: rd_data_A <= mem_bank1[(rd_tid_A*BANK_SLOTS[1])+rd_idx_A];
                2'd2: rd_data_A <= mem_bank2[(rd_tid_A*BANK_SLOTS[2])+rd_idx_A];
                2'd3: rd_data_A <= mem_bank3[(rd_tid_A*BANK_SLOTS[3])+rd_idx_A];
            endcase
        end

        // Read Port B
        if (rd_en_B) begin
            unique case (rd_bank_B)
                2'd0: rd_data_B <= mem_bank0[(rd_tid_B*BANK_SLOTS[0])+rd_idx_B];
                2'd1: rd_data_B <= mem_bank1[(rd_tid_B*BANK_SLOTS[1])+rd_idx_B];
                2'd2: rd_data_B <= mem_bank2[(rd_tid_B*BANK_SLOTS[2])+rd_idx_B];
                2'd3: rd_data_B <= mem_bank3[(rd_tid_B*BANK_SLOTS[3])+rd_idx_B];
            endcase
        end
    end

    // READ VLD Port
    always_comb begin
        rd_vld_A = 1'b0;
        unique case (rd_vld_bank_A)
            2'd0: rd_vld_A = vld_bank0[rd_vld_tid_A][rd_vld_idx_A];
            2'd1: rd_vld_A = vld_bank1[rd_vld_tid_A][rd_vld_idx_A];
            2'd2: rd_vld_A = vld_bank2[rd_vld_tid_A][rd_vld_idx_A];
            2'd3: rd_vld_A = vld_bank3[rd_vld_tid_A][rd_vld_idx_A];
        endcase

        rd_vld_B = 1'b0;
        unique case (rd_vld_bank_B)
            2'd0: rd_vld_B = vld_bank0[rd_vld_tid_B][rd_vld_idx_B];
            2'd1: rd_vld_B = vld_bank1[rd_vld_tid_B][rd_vld_idx_B];
            2'd2: rd_vld_B = vld_bank2[rd_vld_tid_B][rd_vld_idx_B];
            2'd3: rd_vld_B = vld_bank3[rd_vld_tid_B][rd_vld_idx_B];
        endcase
    end

endmodule : regfile
