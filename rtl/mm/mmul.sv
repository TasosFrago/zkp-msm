module mmul #(
    parameter int NUMBER_SIZE = 256,
    parameter int W = 16,
    parameter logic [NUMBER_SIZE-1:0] MODULUS = '0,

    parameter string CORE_TYPE = "BASE"
) (
    input logic clk,
    input logic rst,

    input  logic [(NUMBER_SIZE / W)-1:0][W-1:0] a,
    input  logic [(NUMBER_SIZE / W)-1:0][W-1:0] b,
    output logic [(NUMBER_SIZE / W)-1:0][W-1:0] res
);

    function automatic logic [W-1:0] calc_np(input logic [W-1:0] n0_in);
        logic [W-1:0] x = 1;

        int count = $clog2(W);

        for (int i = 0; i < count; i++) begin
            x = x * (W'(2) - n0_in * x);
        end

        return -x;
    endfunction : calc_np

    generate
        if (CORE_TYPE == "BASE") begin : g_base
            systolic_array #(
                .W(W),
                .CHUNKS((NUMBER_SIZE / W)),
                .MODULUS(MODULUS),
                .N_PRIME(calc_np(MODULUS[W-1:0]))
            ) mod (
                .*
            );

        end else if (CORE_TYPE == "PIPE") begin : g_pipelined
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

            systolic_array_pipe #(
                .W(W),
                .CHUNKS(CHUNKS),
                .MODULUS(MODULUS),
                .N_PRIME(calc_np(MODULUS[W-1:0]))
            ) mod (
                .clk(clk),
                .rst(rst),

                .a  (a_in),
                .b  (b_in),
                .res(res)
            );
        end
    endgenerate

endmodule : mmul

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
