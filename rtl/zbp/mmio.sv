module mmio
    import zbp_pkg::*;
#(
    parameter int BASE_ADDR = 32'hF0000000,
    parameter int GLOBAL_REGS = 3
) (
    input logic clk,
    input logic rst,

    pipeline_if.in mmio_req_if,

    output logic intercept,

    pipeline_if.out mmio_rsp_if,
    output logic program_done
);

    localparam int BYTES_PER_SLOT       = NUMBER_SIZE / 8;
    localparam int SLOT_SHIFT           = $clog2(BYTES_PER_SLOT);
    localparam int NPRIME_SLOT_IDX      = 1;
    localparam int GVALS_START_SLOT_IDX = 2;
    localparam int DONE_SLOT_IDX        = GVALS_START_SLOT_IDX + GLOBAL_REGS;
    localparam int TOTAL_SLOTS          = DONE_SLOT_IDX + 1;
    localparam int SLOT_IDX_W           = $clog2(TOTAL_SLOTS);
    localparam int G_IDX_W              = $clog2(GLOBAL_REGS);


    // TODO: Make a struct with all the special registers to input to the exec
    // units needing them
    logic [NUMBER_SIZE-1:0] g_modulus;
    logic [          W-1:0] g_n_prime;
    logic [NUMBER_SIZE-1:0] g_values_space[GLOBAL_REGS];
    logic done_reg;

    assign program_done = done_reg;

    dmem_req_t req_d;
    assign req_d = mmio_req_if.data;

    logic [SLOT_IDX_W-1:0] slot_idx;
    assign slot_idx = SLOT_IDX_W'((req_d.addr - BASE_ADDR) >> SLOT_SHIFT);

    logic in_range;
    assign in_range = (req_d.addr >= BASE_ADDR) &
                      (slot_idx < SLOT_IDX_W'(TOTAL_SLOTS));

    assign intercept = mmio_req_if.valid & in_range;

    logic is_modulus_slot, is_n_prime_slot, is_g_vals_slot, is_done_slot;

    assign is_modulus_slot = in_range & (slot_idx == 0);
    assign is_n_prime_slot = in_range & (slot_idx == SLOT_IDX_W'(NPRIME_SLOT_IDX));
    assign is_g_vals_slot  = in_range & (slot_idx >= SLOT_IDX_W'(GVALS_START_SLOT_IDX)) &
                                        (slot_idx <  SLOT_IDX_W'(DONE_SLOT_IDX));
    assign is_done_slot    = in_range & (slot_idx == SLOT_IDX_W'(DONE_SLOT_IDX));

    logic [G_IDX_W-1:0] gval_idx;
    assign gval_idx = G_IDX_W'(slot_idx - SLOT_IDX_W'(GVALS_START_SLOT_IDX));

    dmem_rsp_t rsp_data_reg;
    logic      rsp_valid_reg;

    assign mmio_rsp_if.data  = rsp_data_reg;
    assign mmio_rsp_if.valid = rsp_valid_reg;
    assign mmio_req_if.ready = ~rsp_valid_reg;

    logic mmio_accept;
    assign mmio_accept = intercept & mmio_req_if.ready & ~req_d.send_data;

    always_ff @(posedge clk) begin
        if (rst) begin
            g_modulus <= '0;
            g_n_prime <= '0;
            g_values_space <= '{default: '0};
            done_reg <= '0;
            rsp_valid_reg <= FALSE;
            rsp_data_reg <= '{ size: DMEM_B, default: '0 };
        end
        else begin
            if (mmio_rsp_if.valid & mmio_rsp_if.ready) begin
                rsp_valid_reg <= FALSE;
            end

            if (mmio_accept) begin
                rsp_valid_reg <= TRUE;
                case (1'b1)
                    is_modulus_slot: rsp_data_reg <= '{
                        data: g_modulus,
                        size: DMEM_V
                    };

                    is_n_prime_slot: rsp_data_reg <= '{
                        data: NUMBER_SIZE'(g_n_prime),
                        size: DMEM_W
                    };

                    is_g_vals_slot: rsp_data_reg <= '{
                        data: g_values_space[gval_idx],
                        size: DMEM_V
                    };

                    is_done_slot: rsp_data_reg <= '{
                        data: NUMBER_SIZE'(done_reg),
                        size: DMEM_W
                    };
                    default: rsp_data_reg <= '0;
                endcase
            end

            if (intercept & mmio_req_if.ready & req_d.send_data) begin
                unique case (1'b1)
                    is_modulus_slot: g_modulus <= req_d.data;
                    is_n_prime_slot: g_n_prime <= req_d.data[W-1:0];
                    is_g_vals_slot:  g_values_space[gval_idx] <= req_d.data;
                    is_done_slot:    done_reg <= req_d.data[0];
                endcase
            end
        end
    end

    // synthesis translate_off
    property g_val_idx_in_range;
        @(posedge clk) disable iff (rst)
        (is_g_vals_slot & mmio_req_if.valid) |-> (gval_idx < G_IDX_W'(GLOBAL_REGS));
    endproperty

    assert property (g_val_idx_in_range)
    else $error("MMIO: g_val_idx_out_of_range with address: 0x%0h", req_d.addr);
    // synthesis translate_on

endmodule : mmio
