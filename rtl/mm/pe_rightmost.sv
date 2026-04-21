module pe_rightmost #(
    parameter int W = 16,
    parameter logic [W-1:0] N_CHUNK = '0
) (
    input logic [W-1:0] a_in,
    input logic [W-1:0] b0_in,
    input logic [W-1:0] np_in,
    input logic [W-1:0] s_in,

    output logic [W-1:0] a_out,
    output logic [W-1:0] b0_out,
    output logic [W-1:0] np_out,

    output logic [(2*W)-1:0] c_out,
    output logic [W-1:0] q_out
);
    assign a_out  = a_in;
    assign b0_out = b0_in;
    assign np_out = np_in;

    logic [(2*W)-1:0] p;
    assign p = (a_in * b0_in) + (2 * W)'(s_in);

    assign q_out = (p[W-1:0] * np_in);

    logic [(2*W)-1:0] q_n_prod;
    cmul #(
        .W(W),
        .CONSTANT(N_CHUNK)
    ) q_n_mul (
        .p_in (q_out),
        .q_out(q_n_prod)
    );

    logic [(3*W)-1:0] sum;
    assign sum   = (3 * W)'(p) + (3 * W)'(q_n_prod);

    assign c_out = sum[(3*W)-1:W];

endmodule : pe_rightmost

module pe_rightmost_pipe #(
    parameter int W = 16,
    parameter logic [W-1:0] N_CHUNK = '0
) (
    input logic clk,
    input logic rst,

    input logic [W-1:0] a_in,
    input logic [W-1:0] b0_in,
    input logic [W-1:0] np_in,
    input logic [W-1:0] s_in,

    output logic [W-1:0] a_out,
    output logic [W-1:0] b0_out,
    output logic [W-1:0] np_out,

    output logic [(2*W)-1:0] c_out,
    output logic [W-1:0] q_out
);

    logic [(2*W)-1:0] p;
    assign p = (a_in * b0_in) + (2 * W)'(s_in);

    logic [W-1:0] q;
    assign q = (p[W-1:0] * np_in);

    logic [(2*W)-1:0] q_n_prod;
    cmul #(
        .W(W),
        .CONSTANT(N_CHUNK)
    ) q_n_mul (
        .p_in (q),
        .q_out(q_n_prod)
    );

    logic [(3*W)-1:0] sum;
    assign sum = (3 * W)'(p) + (3 * W)'(q_n_prod);

    always_ff @(posedge clk) begin
        if (rst) begin
            a_out  <= '0;
            b0_out <= '0;
            np_out <= '0;
            c_out  <= '0;
            q_out  <= '0;
        end else begin
            a_out  <= a_in;
            b0_out <= b0_in;
            np_out <= np_in;

            q_out  <= q;
            c_out  <= sum[(3*W)-1:W];
        end
    end

endmodule : pe_rightmost_pipe
