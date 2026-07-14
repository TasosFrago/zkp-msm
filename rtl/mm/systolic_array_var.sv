module systolic_array_var #(
    parameter int W = 32,
    parameter int CHUNKS = 8
) (
    input logic clk,
    input logic rst,

    input logic [CHUNKS-1:0][W-1:0] a,
    input logic [CHUNKS-1:0][W-1:0] b,
    input logic [CHUNKS-1:0][W-1:0] modulus,
    input logic [W-1:0] n_prime,

    output logic [CHUNKS-1:0][W-1:0] res
);
    logic [    W-1:0] pe_a_out [CHUNKS][CHUNKS];
    logic [    W-1:0] pe_q_out [CHUNKS][CHUNKS];
    logic [(2*W)-1:0] pe_c_out [CHUNKS][CHUNKS];
    logic [    W-1:0] pe_s_out [CHUNKS][CHUNKS];
    logic [    W-1:0] pe_b_out [CHUNKS][CHUNKS];
    logic [    W-1:0] pe_np_out[CHUNKS][CHUNKS];
    logic [    W-1:0] pe_n_out [CHUNKS][CHUNKS];

    logic [    W-1:0] b_delay[CHUNKS][CHUNKS];
    logic [(2*W)-1:0] c_delay[CHUNKS];

    logic [W-1:0] a_shift[CHUNKS];
    logic [W-1:0] b_shift[CHUNKS];

    genvar a_idx, b_idx;
    generate
        for (a_idx = 0; a_idx < CHUNKS; a_idx++) begin : gen_AshiftReg
            if (a_idx == 0) begin : g_a_shift_row0
                assign a_shift[a_idx] = a[a_idx];
            end
            else begin : g_a_shift_row
                logic [W-1:0] a_shiftreg[2*a_idx];

                always_ff @(posedge clk) begin
                    if (rst) begin
                        for (int k = 0; k < 2 * a_idx; k++) a_shiftreg[k] <= '0;

                    end
                    else begin
                        a_shiftreg[0] <= a[a_idx];

                        for (int k = 1; k < 2 * a_idx; k++) begin
                            a_shiftreg[k] <= a_shiftreg[k-1];
                        end
                    end
                end

                assign a_shift[a_idx] = a_shiftreg[(2*a_idx)-1];
            end
        end
    endgenerate

    generate
        for (b_idx = 0; b_idx < CHUNKS; b_idx++) begin : gen_BshfitReg
            if (b_idx == 0) begin : g_b_shift_row0
                assign b_shift[b_idx] = b[b_idx];

            end
            else begin : g_b_shift_row
                logic [W-1:0] b_shiftreg[b_idx];

                always_ff @(posedge clk) begin
                    if (rst) begin
                        for (int k = 0; k < b_idx; k++) b_shiftreg[k] <= '0;

                    end
                    else begin
                        b_shiftreg[0] <= b[b_idx];

                        for (int k = 1; k < b_idx; k++) begin
                            b_shiftreg[k] <= b_shiftreg[k-1];
                        end
                    end
                end

                assign b_shift[b_idx] = b_shiftreg[b_idx-1];
            end
        end
    endgenerate

    always_ff @(posedge clk) begin
        if (rst) begin
            for (int i = 0; i < CHUNKS; i++) begin
                c_delay[i] <= '0;
                for (int j = 0; j < CHUNKS; j++) begin
                    b_delay[i][j] <= '0;
                end
            end
        end
        else begin
            for (int i = 0; i < CHUNKS; i++) begin

                c_delay[i] <= pe_c_out[i][CHUNKS-1];

                for (int j = 0; j < CHUNKS; j++) begin
                    b_delay[i][j] <= pe_b_out[i][j];
                end
            end
        end
    end

    genvar i, j;
    for (i = 0; i < CHUNKS; i++) begin : gen_rows
        for (j = 0; j < CHUNKS; j++) begin : gen_cols

            if (j == 0) begin : g_pe_rightmost
                pe_rightmost_var #(
                    .W(W)
                ) pe (
                    .clk(clk),
                    .rst(rst),

                    .a_in (a_shift[i]),
                    .b0_in(i == 0 ? b_shift[j] : b_delay[i-1][j]),
                    .np_in(i == 0 ? n_prime : pe_np_out[i-1][j]),
                    .s_in (i == 0 ? '0 : pe_s_out[i-1][1]),
                    .n_in (i == 0 ? modulus[0] : pe_n_out[i-1][j]),

                    .a_out (pe_a_out[i][0]),
                    .b0_out(pe_b_out[i][0]),
                    .np_out(pe_np_out[i][0]),
                    .n_out (pe_n_out[i][0]),
                    .c_out (pe_c_out[i][0]),
                    .q_out (pe_q_out[i][0])
                );
            end
            else if (j == (CHUNKS - 1)) begin : g_pe_leftmost
                pe_standard_var #(
                    .W(W)
                ) pe (
                    .clk(clk),
                    .rst(rst),

                    .a_in(pe_a_out[i][j-1]),
                    .b_in(i == 0 ? b_shift[j] : b_delay[i-1][j]),
                    .q_in(pe_q_out[i][j-1]),
                    .c_in(pe_c_out[i][j-1]),
                    .s_in(i == 0 ? '0 : c_delay[i-1]),
                    .n_in(i == 0 ? modulus[j] : pe_n_out[i-1][j]),

                    .a_out(pe_a_out[i][j]),
                    .b_out(pe_b_out[i][j]),
                    .q_out(pe_q_out[i][j]),
                    .c_out(pe_c_out[i][j]),
                    .s_out(pe_s_out[i][j]),
                    .n_out(pe_n_out[i][j])
                );
            end
            else begin : g_pe_standard
                pe_standard_var #(
                    .W(W)
                ) pe (
                    .clk(clk),
                    .rst(rst),

                    .a_in(pe_a_out[i][j-1]),
                    .b_in(i == 0 ? b_shift[j] : b_delay[i-1][j]),
                    .q_in(pe_q_out[i][j-1]),
                    .c_in(pe_c_out[i][j-1]),
                    .s_in(i == 0 ? '0 : (2 * W)'(pe_s_out[i-1][j+1])),
                    .n_in(i == 0 ? modulus[j] : pe_n_out[i-1][j]),

                    .a_out(pe_a_out[i][j]),
                    .b_out(pe_b_out[i][j]),
                    .q_out(pe_q_out[i][j]),
                    .c_out(pe_c_out[i][j]),
                    .s_out(pe_s_out[i][j]),
                    .n_out(pe_n_out[i][j])
                );
            end
        end
    end

    logic [W-1:0] bottom_row_res[CHUNKS];

    always_comb begin
        for (int j = 0; j < CHUNKS; j++) begin
            if (j == (CHUNKS - 1)) begin
                bottom_row_res[j] = pe_c_out[CHUNKS-1][j][W-1:0];
            end
            else begin
                bottom_row_res[j] = pe_s_out[CHUNKS-1][j+1];
            end
        end
    end

    generate
        for (genvar j = 0; j < CHUNKS; j++) begin : gen_deskew
            localparam int DELAY = (j == CHUNKS - 1) ? 0 : (CHUNKS - 2) - j;

            if (DELAY == 0) begin : g_deskew_col
                always_ff @(posedge clk) begin
                    if (rst) res[j] <= '0;
                    else res[j] <= bottom_row_res[j];
                end

            end
            else begin : g_deskew_col
                logic [W-1:0] deskew_shift[DELAY];

                always_ff @(posedge clk) begin
                    if (rst) begin
                        for (int k = 0; k < DELAY; k++) deskew_shift[k] <= '0;
                        res[j] <= '0;

                    end
                    else begin
                        deskew_shift[0] <= bottom_row_res[j];

                        for (int k = 1; k < DELAY; k++) begin
                            deskew_shift[k] <= deskew_shift[k-1];
                        end

                        res[j] <= deskew_shift[DELAY-1];
                    end
                end
            end
        end
    endgenerate

endmodule : systolic_array_var
