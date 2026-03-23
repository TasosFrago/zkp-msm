module pe_rightmost #(
    parameter int W = 16
) (
    input logic [W-1:0] a_in,
    input logic [W-1:0] b0_in,
    input logic [W-1:0] n0_in,
    input logic [W-1:0] np_in,
    input logic [W-1:0] s_in,

    output logic [W-1:0] a_out,
    output logic [W-1:0] b0_out,
    output logic [W-1:0] n0_out,
    output logic [W-1:0] np_out,

    output logic [(2*W)-1:0] c_out,
    output logic [W-1:0] q_out
);
    assign a_out  = a_in;
    assign b0_out = b0_in;
    assign n0_out = n0_in;
    assign np_out = np_in;

    logic [(2*W)-1:0] p;
    assign p = (a_in * b0_in) + (2 * W)'(s_in);

    assign q_out = (p[W-1:0] * np_in);

    logic [(3*W)-1:0] sum;
    assign sum   = (3 * W)'(p) + (q_out * n0_in);

    assign c_out = sum[(3*W)-1:W];

endmodule : pe_rightmost
