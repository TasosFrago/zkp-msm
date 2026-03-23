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
        end
    endgenerate

endmodule : mmul
