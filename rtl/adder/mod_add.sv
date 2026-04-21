module mod_add #(
    parameter int W = 16,
    parameter int CHUNKS = 4,
    parameter logic [(CHUNKS*W)-1:0] MODULUS
) (
    input logic clk,
    input logic rst,

    input logic op,  // 0 = addition, 1 = subtraction

    input logic [CHUNKS-1:0][W-1:0] a,
    input logic [CHUNKS-1:0][W-1:0] b,

    output logic [CHUNKS-1:0][W-1:0] res
);

    logic [CHUNKS:0] op_delay;

    always_ff @(posedge clk) begin
        if (rst) begin
            op_delay <= '{default: '0};
        end else begin
            op_delay <= {op_delay[CHUNKS-1:0], op};
        end
    end

    logic [W-1:0] a_shifted_in[CHUNKS];
    logic [W-1:0] b_shifted_in[CHUNKS];

    logic [W-1:0] res0_shifted[CHUNKS];
    logic [W-1:0] res1_shifted[CHUNKS];

    logic [W-1:0] res0[CHUNKS];
    logic [W-1:0] res1[CHUNKS];

    generate
        for (genvar i = 0; i < CHUNKS; i++) begin : gen_input_shiftReg

            logic [W-1:0] a_shiftReg_in[i+1];
            logic [W-1:0] b_shiftReg_in[i+1];

            logic [W-1:0] res0_shiftReg[CHUNKS-i];
            logic [W-1:0] res1_shiftReg[CHUNKS-i];

            always_ff @(posedge clk) begin
                if (rst) begin
                    for (int k = 0; k < (i + 1); k++) begin
                        a_shiftReg_in[k] <= '0;
                        b_shiftReg_in[k] <= '0;
                    end
                    for (int j = 0; j < (CHUNKS - i); j++) begin
                        res0_shiftReg[j] <= '0;
                        res1_shiftReg[j] <= '0;
                    end

                end else begin
                    a_shiftReg_in[0] <= a[i];
                    b_shiftReg_in[0] <= b[i];

                    res0_shiftReg[0] <= res0[i];
                    res1_shiftReg[0] <= res1[i];

                    for (int k = 1; k < (i + 1); k++) begin
                        a_shiftReg_in[k] <= a_shiftReg_in[k-1];
                        b_shiftReg_in[k] <= b_shiftReg_in[k-1];
                    end
                    for (int j = 1; j < (CHUNKS - i); j++) begin
                        res0_shiftReg[j] <= res0_shiftReg[j-1];
                        res1_shiftReg[j] <= res1_shiftReg[j-1];
                    end
                end
            end

            assign a_shifted_in[i] = a_shiftReg_in[(i+1)-1];
            assign b_shifted_in[i] = b_shiftReg_in[(i+1)-1];

            assign res0_shifted[i] = res0_shiftReg[(CHUNKS-i)-1];
            assign res1_shifted[i] = res1_shiftReg[(CHUNKS-i)-1];
        end
    endgenerate

    logic c0[CHUNKS];
    logic cn[CHUNKS];

    generate
        for (genvar i = 0; i < CHUNKS; i++) begin : gen_adder_block
            logic [W:0] tmp_res0, tmp_res1;
            logic c0_out, cn_out;

            always_comb begin
                if (!op_delay[i]) begin : g_addition
                    tmp_res0 = (i == 0)
                        ? a_shifted_in[i] + b_shifted_in[i]
                        : a_shifted_in[i] + b_shifted_in[i] + (W+1)'(c0[i-1]);

                    c0_out = tmp_res0[W];
                    res0[i] = tmp_res0[W-1:0];

                    tmp_res1 = (i == 0)
                        ? tmp_res0[W-1:0] - MODULUS[W-1:0]
                        : tmp_res0[W-1:0] - MODULUS[i*W+:W] - (W+1)'(cn[i-1]);

                    cn_out = tmp_res1[W];
                    res1[i] = tmp_res1[W-1:0];

                end else begin : g_subtraction
                    tmp_res0 = (i == 0)
                        ? a_shifted_in[i] - b_shifted_in[i]
                        : a_shifted_in[i] - b_shifted_in[i] - (W+1)'(c0[i-1]);

                    c0_out = tmp_res0[W];
                    res0[i] = tmp_res0[W-1:0];

                    tmp_res1 = (i == 0)
                        ? tmp_res0[W-1:0] + MODULUS[W-1:0]
                        : tmp_res0[W-1:0] + MODULUS[i*W+:W] + (W+1)'(cn[i-1]);

                    cn_out = tmp_res1[W];
                    res1[i] = tmp_res1[W-1:0];
                end
            end

            always_ff @(posedge clk) begin
                if (rst) begin
                    c0[i] <= '0;
                    cn[i] <= '0;
                end else begin
                    c0[i] <= c0_out;
                    cn[i] <= cn_out;
                end
            end
        end
    endgenerate

    always_comb begin
        for (int k = 0; k < CHUNKS; k++) begin
            if (!op_delay[CHUNKS]) begin
                res[k] = (cn[CHUNKS-1] & ~c0[CHUNKS-1]) ? res0_shifted[k] : res1_shifted[k];
            end else begin
                res[k] = (~c0[CHUNKS-1]) ? res0_shifted[k] : res1_shifted[k];
            end
        end
    end

endmodule : mod_add
