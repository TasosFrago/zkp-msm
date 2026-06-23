module regfile #(
    parameter int NUMBER_SIZE = 256,
    parameter int W = 32,
    parameter int MAX_THREADS = 32,
    parameter int REGISTERS = 20
) (
    input logic clk,
    input logic rst,

    // =============== Writer Interface ===============
    input logic [        NUMBER_SIZE-1:0] wr_data_A,
    input logic                           wr_en_A,
    input logic [$clog2(MAX_THREADS)-1:0] wr_tid_A,
    input logic [  $clog2(REGISTERS)-1:0] wr_reg_A,

    input logic [        NUMBER_SIZE-1:0] wr_data_B,
    input logic                           wr_en_B,
    input logic [$clog2(MAX_THREADS)-1:0] wr_tid_B,
    input logic [  $clog2(REGISTERS)-1:0] wr_reg_B,

    // =============== Reader Interface ===============
    output logic [        NUMBER_SIZE-1:0] rd_data_A,
    input  logic                           rd_en_A,
    input  logic [$clog2(MAX_THREADS)-1:0] rd_tid_A,
    input  logic [  $clog2(REGISTERS)-1:0] rd_reg_A,

    output logic [        NUMBER_SIZE-1:0] rd_data_B,
    input  logic                           rd_en_B,
    input  logic [$clog2(MAX_THREADS)-1:0] rd_tid_B,
    input  logic [  $clog2(REGISTERS)-1:0] rd_reg_B
);

    logic [NUMBER_SIZE-1:0] mem_bank0[MAX_THREADS * (REGISTERS / 4)];
    logic [NUMBER_SIZE-1:0] mem_bank1[MAX_THREADS * (REGISTERS / 4)];
    logic [NUMBER_SIZE-1:0] mem_bank2[MAX_THREADS * (REGISTERS / 4)];
    logic [NUMBER_SIZE-1:0] mem_bank3[MAX_THREADS * (REGISTERS / 4)];

    always_ff @(posedge clk) begin
        // Write Port A
        if (wr_en_A) begin
            unique case (wr_reg_A[1:0])
                2'd0: mem_bank0[{wr_tid_A, wr_reg_A[$clog2(REGISTERS)-1:2]}] <= wr_data_A;
                2'd1: mem_bank1[{wr_tid_A, wr_reg_A[$clog2(REGISTERS)-1:2]}] <= wr_data_A;
                2'd2: mem_bank2[{wr_tid_A, wr_reg_A[$clog2(REGISTERS)-1:2]}] <= wr_data_A;
                2'd3: mem_bank3[{wr_tid_A, wr_reg_A[$clog2(REGISTERS)-1:2]}] <= wr_data_A;
            endcase
        end

        // Write Port B
        if (wr_en_B) begin
            unique case (wr_reg_B[1:0])
                2'd0: mem_bank0[{wr_tid_B, wr_reg_B[$clog2(REGISTERS)-1:2]}] <= wr_data_B;
                2'd1: mem_bank1[{wr_tid_B, wr_reg_B[$clog2(REGISTERS)-1:2]}] <= wr_data_B;
                2'd2: mem_bank2[{wr_tid_B, wr_reg_B[$clog2(REGISTERS)-1:2]}] <= wr_data_B;
                2'd3: mem_bank3[{wr_tid_B, wr_reg_B[$clog2(REGISTERS)-1:2]}] <= wr_data_B;
            endcase
        end
    end

    always_ff @(posedge clk) begin
        // Read Port A
        if (rd_en_A) begin
            if (rd_en_A) begin
                unique case (rd_reg_A[1:0])
                    2'd0: rd_data_A <= mem_bank0[{rd_tid_A, rd_reg_A[$clog2(REGISTERS)-1:2]}];
                    2'd1: rd_data_A <= mem_bank1[{rd_tid_A, rd_reg_A[$clog2(REGISTERS)-1:2]}];
                    2'd2: rd_data_A <= mem_bank2[{rd_tid_A, rd_reg_A[$clog2(REGISTERS)-1:2]}];
                    2'd3: rd_data_A <= mem_bank3[{rd_tid_A, rd_reg_A[$clog2(REGISTERS)-1:2]}];
                endcase
            end
        end

        // Read Port B
        if (rd_en_B) begin
            if (rd_en_B) begin
                unique case (rd_reg_B[1:0])
                    2'd0: rd_data_B <= mem_bank0[{rd_tid_B, rd_reg_B[$clog2(REGISTERS)-1:2]}];
                    2'd1: rd_data_B <= mem_bank1[{rd_tid_B, rd_reg_B[$clog2(REGISTERS)-1:2]}];
                    2'd2: rd_data_B <= mem_bank2[{rd_tid_B, rd_reg_B[$clog2(REGISTERS)-1:2]}];
                    2'd3: rd_data_B <= mem_bank3[{rd_tid_B, rd_reg_B[$clog2(REGISTERS)-1:2]}];
                endcase
            end
        end
    end

endmodule : regfile
