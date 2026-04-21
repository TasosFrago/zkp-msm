module cmul #(
    parameter int W = 16,
    parameter logic [W-1:0] CONSTANT = '0
) (
    input logic [W-1:0] p_in,
    output logic [(2*W)-1:0] q_out
);

    localparam int MAX_OPS = (W + 5) / 4;

    typedef struct packed {
        logic valid;
        logic sign;
        logic [2:0] case_sel;
        logic [$clog2(W+2)-1:0] shift;
    } op_t;

    function automatic op_t [MAX_OPS-1:0] compute_schedule(logic [W-1:0] c);
        op_t [MAX_OPS-1:0] schedule;
        int naf[W+2];
        logic [W+1:0] val = {2'b0, c};
        int op_idx = 0;
        int i = 0;

        for (int k = 0; k < MAX_OPS; k++) schedule[k] = '0;
        for (int k = 0; k < W + 2; k++) naf[k] = '0;

        // Converting to NAF encoding
        for (int k = 0; k < W + 2; k++) begin
            if (val == 0) break;

            if (val[0] == 1'b1) begin
                if (val[1] == 1'b1) begin
                    naf[k] = -1;
                    val = (val >> 1) + 1;

                end
                else begin
                    naf[k] = 1;
                    val = (val >> 1);
                end

            end
            else begin
                naf[k] = 0;
                val = (val >> 1);
            end
        end

        // Spliting into windows
        while (i < W + 2) begin
            if (naf[i] != 0) begin
                int v0 = naf[i];
                int v1 = (i + 1 < W + 2) ? naf[i+1] : 0;
                int v2 = (i + 2 < W + 2) ? naf[i+2] : 0;
                int v3 = (i + 3 < W + 2) ? naf[i+3] : 0;

                int window_val = (v3 * 8) + (v2 * 4) + (v1 * 2) + v0;
                int abs_val = (window_val < 0) ? -window_val : window_val;

                schedule[op_idx].valid = 1'b1;
                schedule[op_idx].sign  = (window_val < 0) ? 1'b1 : 1'b0;
                schedule[op_idx].shift = $clog2(W + 2)'(i);

                case (abs_val)
                    1: schedule[op_idx].case_sel = 3'd0;
                    3: schedule[op_idx].case_sel = 3'd1;
                    5: schedule[op_idx].case_sel = 3'd2;
                    7: schedule[op_idx].case_sel = 3'd3;
                    9: schedule[op_idx].case_sel = 3'd4;
                    default: ;
                endcase

                op_idx++;
                i = i + 4;
            end
            else begin
                i++;
            end
        end

        return schedule;
    endfunction

    localparam op_t [MAX_OPS-1:0] SCHEDULE = compute_schedule(CONSTANT);

    logic [(2*W)-1:0] mul_1, mul_3, mul_5, mul_7, mul_9;
    logic [(2*W)-1:0] p;
    logic [(2*W)-1:0] pos_sum, neg_sum;

    assign p = (2 * W)'(p_in);

    assign mul_1 = p;
    assign mul_3 = (p << 1) + p;
    assign mul_5 = (p << 2) + p;
    assign mul_7 = (p << 3) - p;
    assign mul_9 = (p << 3) + p;

    always_comb begin
        pos_sum = '0;
        neg_sum = '0;

        for (int i = 0; i < MAX_OPS; i++) begin
            if (SCHEDULE[i].valid) begin
                logic [(2*W)-1:0] selected;
                logic [(2*W)-1:0] shifted_term;

                case (SCHEDULE[i].case_sel)
                    3'd0: selected = mul_1;
                    3'd1: selected = mul_3;
                    3'd2: selected = mul_5;
                    3'd3: selected = mul_7;
                    3'd4: selected = mul_9;
                    default: selected = '0;
                endcase

                shifted_term = selected << SCHEDULE[i].shift;

                if (SCHEDULE[i].sign == 1'b0) begin
                    pos_sum = pos_sum + shifted_term;
                end
                else begin
                    neg_sum = neg_sum + shifted_term;
                end
            end
        end
    end

    assign q_out = pos_sum - neg_sum;

endmodule : cmul
