module mmul_top_tb #(
    parameter int NUMBER_SIZE = 256,
    parameter int W = 16,
    parameter logic [NUMBER_SIZE-1:0] MODULUS = '0
) (
    input logic clk,
    input logic rst,

    input logic [(NUMBER_SIZE / W)-1:0][W-1:0] a,
    input logic [(NUMBER_SIZE / W)-1:0][W-1:0] b,
    input logic [(NUMBER_SIZE / W)-1:0][W-1:0] modulus,
    input logic [W-1:0] n_prime,

    output logic [(NUMBER_SIZE / W)-1:0][W-1:0] res_n,
    output logic [(NUMBER_SIZE / W)-1:0][W-1:0] res_p,
    output logic [(NUMBER_SIZE / W)-1:0][W-1:0] res_v
);

    mmul #(
        .NUMBER_SIZE(NUMBER_SIZE),
        .W(W),
        .MODULUS(MODULUS),
        .CORE_TYPE("BASE")
    ) dut_base (
        .clk(clk),
        .rst(rst),
        .a  (a),
        .b  (b),
        .res(res_n)
    );

    mmul #(
        .NUMBER_SIZE(NUMBER_SIZE),
        .W(W),
        .MODULUS(MODULUS),
        .CORE_TYPE("PIPE")
    ) dut_pipe (
        .clk(clk),
        .rst(rst),
        .a  (a),
        .b  (b),
        .res(res_p)
    );

    mmul_variable #(
        .NUMBER_SIZE(NUMBER_SIZE),
        .W(W)
    ) dut_var (
        .clk(clk),
        .rst(rst),

        .a(a),
        .b(b),
        .modulus(modulus),
        .n_prime(n_prime),
        .res(res_v)
    );

endmodule : mmul_top_tb
