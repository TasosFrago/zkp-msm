module mmul_variable #(
    parameter int NUMBER_SIZE = 256,
    parameter int W = 32
) (
    input logic clk,
    input logic rst,

    input  logic [(NUMBER_SIZE / W)-1:0][W-1:0] a,
    input  logic [(NUMBER_SIZE / W)-1:0][W-1:0] b,
    input  logic [(NUMBER_SIZE / W)-1:0][W-1:0] modulus,
    input  logic [                       W-1:0] n_prime,
    output logic [(NUMBER_SIZE / W)-1:0][W-1:0] res
);

    localparam int CHUNKS = (NUMBER_SIZE / W);

    logic [CHUNKS-1:0][W-1:0] a_in;
    logic [CHUNKS-1:0][W-1:0] b_in;

    always_ff @(posedge clk) begin
        if (rst) begin
            a_in <= '0;
            b_in <= '0;

        end else begin
            for (int i = 0; i < CHUNKS; i++) begin
                a_in[i] <= a[i];
                b_in[i] <= b[i];
            end
        end
    end

    systolic_array_var #(
        .W(W),
        .CHUNKS(CHUNKS)
    ) mod (
        .clk(clk),
        .rst(rst),

        .a      (a_in),
        .b      (b_in),
        .modulus(modulus),
        .n_prime(n_prime),
        .res(res)
    );

endmodule : mmul_variable
