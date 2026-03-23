module pe_standard #(
    parameter int W = 16
) (
    input logic [W-1:0] a_in,
    input logic [W-1:0] b_in,
    input logic [W-1:0] q_in,
    input logic [W-1:0] n_in,
    input logic [(2*W)-1:0] c_in,
    input logic [(2*W)-1:0] s_in,

    output logic [W-1:0] a_out,
    output logic [W-1:0] b_out,
    output logic [W-1:0] q_out,
    output logic [W-1:0] n_out,
    output logic [(2*W)-1:0] c_out,
    output logic [W-1:0] s_out
);
    assign a_out = a_in;
    assign b_out = b_in;
    assign q_out = q_in;
    assign n_out = n_in;

    logic [(3*W)-1:0] res;
    assign res   = (a_in * b_in) + (q_in * n_in) + ((3 * W)'(s_in)) + (3 * W)'(c_in);

    assign c_out = res[(3*W)-1:W];
    assign s_out = res[W-1:0];

endmodule : pe_standard
