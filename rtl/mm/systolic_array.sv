/* verilator lint_off UNOPTFLAT */
module systolic_array #(
    parameter int W = 16,
    parameter int CHUNKS = 4,

    parameter logic [(CHUNKS*W)-1:0] MODULUS,
    parameter logic [W-1:0] N_PRIME
) (
    input logic clk,
    input logic rst,

    input logic [CHUNKS-1:0][W-1:0] a,
    input logic [CHUNKS-1:0][W-1:0] b,

    output logic [CHUNKS-1:0][W-1:0] res
);

    logic [W-1:0] pe_a_out[CHUNKS][CHUNKS];
    logic [W-1:0] pe_q_out[CHUNKS][CHUNKS];
    logic [(2*W)-1:0] pe_c_out[CHUNKS][CHUNKS];
    logic [W-1:0] pe_s_out[CHUNKS][CHUNKS];
    logic [W-1:0] pe_b_out[CHUNKS][CHUNKS];
    logic [W-1:0] pe_n_out[CHUNKS][CHUNKS];
    logic [W-1:0] pe_np_out[CHUNKS][CHUNKS];

    logic [W-1:0] a_in[CHUNKS];
    logic [W-1:0] b_in[CHUNKS];

    always_ff @(posedge clk) begin
        if (rst) begin
            a_in <= '{default: '0};
            b_in <= '{default: '0};
            res  <= '{default: '0};
        end else begin
            for (int i = 0; i < CHUNKS; i++) begin
                a_in[i] <= a[i];
                b_in[i] <= b[i];
                if (i == (CHUNKS - 1)) begin
                    res[i] <= pe_c_out[CHUNKS-1][i][W-1:0];
                end else begin
                    res[i] <= pe_s_out[CHUNKS-1][i+1];
                end
            end
        end
    end

    genvar i, j;
    for (i = 0; i < CHUNKS; i++) begin : gen_rows
        for (j = 0; j < CHUNKS; j++) begin : gen_cols

            if (j == 0) begin : g_pe_rightmost
                pe_rightmost #(
                    .W(W)
                ) pe (
                    .a_in  (a_in[i]),
                    .b0_in (i == 0 ? b_in[0] : pe_b_out[i-1][0]),
                    .n0_in (i == 0 ? MODULUS[W-1:0] : pe_n_out[i-1][0]),
                    .np_in (i == 0 ? N_PRIME : pe_np_out[i-1][0]),
                    .s_in  (i == 0 ? '0 : pe_s_out[i-1][1]),
                    .a_out (pe_a_out[i][0]),
                    .b0_out(pe_b_out[i][0]),
                    .n0_out(pe_n_out[i][0]),
                    .np_out(pe_np_out[i][0]),
                    .c_out (pe_c_out[i][0]),
                    .q_out (pe_q_out[i][0])
                );
            end else if (j == (CHUNKS - 1)) begin : g_pe_leftmost
                pe_standard #(
                    .W(W)
                ) pe (
                    .a_in (pe_a_out[i][j-1]),
                    .b_in (i == 0 ? b_in[j] : pe_b_out[i-1][j]),
                    .q_in (pe_q_out[i][j-1]),
                    .n_in (i == 0 ? MODULUS[j*W+:W] : pe_n_out[i-1][j]),
                    .c_in (pe_c_out[i][j-1]),
                    .s_in (i == 0 ? '0 : pe_c_out[i-1][j]),
                    .a_out(pe_a_out[i][j]),
                    .b_out(pe_b_out[i][j]),
                    .q_out(pe_q_out[i][j]),
                    .n_out(pe_n_out[i][j]),
                    .c_out(pe_c_out[i][j]),
                    .s_out(pe_s_out[i][j])
                );
            end else begin : g_pe_standard
                pe_standard #(
                    .W(W)
                ) pe (
                    .a_in (pe_a_out[i][j-1]),
                    .b_in (i == 0 ? b_in[j] : pe_b_out[i-1][j]),
                    .q_in (pe_q_out[i][j-1]),
                    .n_in (i == 0 ? MODULUS[j*W+:W] : pe_n_out[i-1][j]),
                    .c_in (pe_c_out[i][j-1]),
                    .s_in (i == 0 ? '0 : (2 * W)'(pe_s_out[i-1][j+1])),
                    .a_out(pe_a_out[i][j]),
                    .b_out(pe_b_out[i][j]),
                    .q_out(pe_q_out[i][j]),
                    .n_out(pe_n_out[i][j]),
                    .c_out(pe_c_out[i][j]),
                    .s_out(pe_s_out[i][j])
                );
            end
        end
    end

endmodule : systolic_array
/* verilator lint_on UNOPTFLAT */
