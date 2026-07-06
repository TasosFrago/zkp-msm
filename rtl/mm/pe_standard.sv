module pe_standard #(
    parameter int W = 16,
    parameter logic [W-1:0] N_CHUNK = '0
) (
    input logic [W-1:0] a_in,
    input logic [W-1:0] b_in,
    input logic [W-1:0] q_in,
    input logic [(2*W)-1:0] c_in,
    input logic [(2*W)-1:0] s_in,

    output logic [W-1:0] a_out,
    output logic [W-1:0] b_out,
    output logic [W-1:0] q_out,
    output logic [(2*W)-1:0] c_out,
    output logic [W-1:0] s_out
);
    assign a_out = a_in;
    assign b_out = b_in;
    assign q_out = q_in;

    logic [(2*W)-1:0] q_n_prod;

    cmul #(
        .W(W),
        .CONSTANT(N_CHUNK)
    ) q_n_mul (
        .p_in (q_in),
        .q_out(q_n_prod)
    );

    logic [(3*W)-1:0] res;
    assign res   = (a_in * b_in) + (3 * W)'(q_n_prod) + ((3 * W)'(s_in)) + (3 * W)'(c_in);

    assign c_out = res[(3*W)-1:W];
    assign s_out = res[W-1:0];

endmodule : pe_standard

module pe_standard_pipe #(
    parameter int W = 16,
    parameter logic [W-1:0] N_CHUNK = '0
) (
    input logic clk,
    input logic rst,

    input logic [W-1:0] a_in,
    input logic [W-1:0] b_in,
    input logic [W-1:0] q_in,
    input logic [(2*W)-1:0] c_in,
    input logic [(2*W)-1:0] s_in,

    output logic [W-1:0] a_out,
    output logic [W-1:0] b_out,
    output logic [W-1:0] q_out,
    output logic [(2*W)-1:0] c_out,
    output logic [W-1:0] s_out
);

    logic [(2*W)-1:0] q_n_prod;
    cmul #(
        .W(W),
        .CONSTANT(N_CHUNK)
    ) q_n_mul (
        .p_in (q_in),
        .q_out(q_n_prod)
    );

    logic [(3*W)-1:0] res;
    assign res = (a_in * b_in) + (3 * W)'(q_n_prod) + ((3 * W)'(s_in)) + (3 * W)'(c_in);

    always_ff @(posedge clk) begin
        if (rst) begin
            a_out <= '0;
            b_out <= '0;
            q_out <= '0;
            c_out <= '0;
            s_out <= '0;
        end else begin
            a_out <= a_in;
            b_out <= b_in;
            q_out <= q_in;

            c_out <= res[(3*W)-1:W];
            s_out <= res[W-1:0];
        end
    end


endmodule : pe_standard_pipe

module pe_standard_var #(
    parameter int W = 32
) (
    input logic clk,
    input logic rst,

    input logic [W-1:0] a_in,
    input logic [W-1:0] b_in,
    input logic [W-1:0] q_in,
    input logic [(2*W)-1:0] c_in,
    input logic [(2*W)-1:0] s_in,
    input logic [W-1:0] n_in,

    output logic [W-1:0] a_out,
    output logic [W-1:0] b_out,
    output logic [W-1:0] q_out,
    output logic [(2*W)-1:0] c_out,
    output logic [W-1:0] s_out,
    output logic [W-1:0] n_out
);

    logic [(2*W)-1:0] q_n_prod;
    assign q_n_prod = q_in * n_in;

    logic [(3*W)-1:0] res;
    assign res = (a_in * b_in) + (3 * W)'(q_n_prod) + ((3 * W)'(s_in)) + (3 * W)'(c_in);

    always_ff @(posedge clk) begin
        if (rst) begin
            a_out <= '0;
            b_out <= '0;
            q_out <= '0;
            c_out <= '0;
            s_out <= '0;
            n_out <= '0;
        end else begin
            a_out <= a_in;
            b_out <= b_in;
            q_out <= q_in;
            n_out <= n_in;

            c_out <= res[(3*W)-1:W];
            s_out <= res[W-1:0];
        end
    end


endmodule : pe_standard_var
