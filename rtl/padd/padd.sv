module padd #(
    parameter int NUMBER_SIZE = 256,
    parameter int W = 32,
    parameter logic [NUMBER_SIZE-1:0] MODULUS = '0
) (
    input logic clk,
    input logic rst,

    output logic rdy_in,
    input  logic vld_in,

    input logic [NUMBER_SIZE-1:0] bus0_in,
    input logic [NUMBER_SIZE-1:0] bus1_in,

    output logic [NUMBER_SIZE-1:0] bus_out,
    output logic valid_out
);
    localparam int CHUNKS = (NUMBER_SIZE / W);
    localparam int MUL_LAT = 3 * CHUNKS;
    localparam int ADD_LAT = CHUNKS + 1;
    localparam int THREAD_CNT = 16;
    localparam int LOAD_STEPS = 3;

    import instr_pkg::*;

    localparam type alu_tag_t = alu_metadata#(
        .W(W),
        .TID_BITS($clog2(THREAD_CNT))
    )::tag_t;
    localparam type alu_dest_t = alu_metadata#(
        .W(W),
        .TID_BITS($clog2(THREAD_CNT))
    )::dest_t;

    logic [$clog2(THREAD_CNT)-1:0] active_tid;
    logic [THREAD_CNT-1:0] active_threads_map;
    logic [4:0] pc[THREAD_CNT];
    logic [THREAD_CNT-1:0] active_side;

    logic control_path_active;

    // Thread select + Fetch/Decode
    typedef struct packed {
        logic valid;
        logic [$clog2(THREAD_CNT)-1:0] tid;
        logic [4:0] pc;
        instr_t instr;
        logic finished;
    } stage1_pipereg_t;

    // Dependency Check
    typedef struct packed {
        logic valid;
        logic [$clog2(THREAD_CNT)-1:0] tid;
        instr_t instr;
        logic deps_met;
        logic finished;
    } stage2_pipereg_t;

    typedef struct packed {
        logic valid;
        logic [$clog2(THREAD_CNT)-1:0] tid;
        instr_t instr;
        logic same_srcs;
        logic deps_met;
    } stage3_pipereg_t;

    // ==================================================
    //  Stage 1: Thread Select + Fetch/Decode
    // ==================================================
    stage1_pipereg_t s1_reg;

    logic init_flush, reg_flush;
    logic [$clog2(THREAD_CNT)-1:0] init_flush_tid, reg_flush_tid;
    logic init_flush_side;

    logic [THREAD_CNT-1:0] current_1hot, next_1hot, upper_threads;
    logic [$clog2(THREAD_CNT)-1:0] next_tid;

    assign current_1hot = 1'b1 << active_tid;
    assign upper_threads = active_threads_map & ~((current_1hot << 1) - 1'b1);

    assign next_1hot = upper_threads ?
          (upper_threads & -upper_threads)
        : (active_threads_map & -active_threads_map);

    always_comb begin
        next_tid = '0;
        for (int i = 0; i < THREAD_CNT; i++) begin
            if (next_1hot[i]) next_tid = i[$clog2(THREAD_CNT)-1:0];
        end
    end

    assert property (@(posedge clk) control_path_active |-> |active_threads_map);

    always_ff @(posedge clk) begin
        if (rst) begin
            s1_reg <= '{default: '0};
            active_tid <= '0;
            active_threads_map <= '0;
            active_side <= '0;
            init_flush <= '0;
            init_flush_tid <= '0;
            init_flush_side <= '0;
            reg_flush <= '0;
            reg_flush_tid <= '0;
        end
        else begin
            init_flush <= '0;
            reg_flush  <= '0;

            if (control_path_active) begin
                if (pc[active_tid] == NUM_OPS) begin
                    // TODO:
                    // * ~switch active side~
                    // * ~empty active_tid's init_vals~
                    // * ~zero pc[tid]~

                    init_flush_side <= active_side[active_tid];
                    active_side[active_tid] <= ~active_side[active_tid];

                    init_flush <= 1'b1;
                    init_flush_tid <= active_tid;

                    reg_flush <= 1'b1;
                    reg_flush_tid <= active_tid;

                    s1_reg.valid <= 1'b0;
                    active_tid <= next_tid;
                end
                else begin
                    s1_reg.valid <= 1'b1;
                    s1_reg.tid <= active_tid;
                    s1_reg.pc <= pc[active_tid];
                    s1_reg.instr <= instr_rom[pc[active_tid]];
                    s1_reg.finished <= 1'b0;

                    active_tid <= next_tid;
                end
            end
            else begin
                s1_reg.valid <= 0;
            end
        end
    end

    // PC Registers
    always_ff @(posedge clk) begin
        if (rst) begin
            pc <= '{default: '0};
        end
        else begin
            if (pc[active_tid] == NUM_OPS && control_path_active) begin
                pc[active_tid] <= '0;
            end
            else if (s2_reg.valid && s2_reg.deps_met) begin
                pc[s2_reg.tid] <= pc[s2_reg.tid] + 1;
            end
        end
    end

    // ==================================================
    //  Stage 2: Dependency Check
    // ==================================================
    stage2_pipereg_t s2_reg;
    logic deps_met;
    logic init_vld_a, init_vld_b, reg_vld_a, reg_vld_b;
    logic srcA_rdy, srcB_rdy;

    logic [1:0] s1_init_bank_a, s1_init_bank_b;
    assign s1_init_bank_a = s1_reg.instr.src_a.bank + (active_side[s1_reg.tid] ? 2'd2 : 2'd0);
    assign s1_init_bank_b = s1_reg.instr.src_b.bank + (active_side[s1_reg.tid] ? 2'd2 : 2'd0);

    assign srcA_rdy = s1_reg.instr.src_a.is_init ? init_vld_a : reg_vld_a;
    assign srcB_rdy = s1_reg.instr.src_b.is_init ? init_vld_b : reg_vld_b;

    assign deps_met = srcA_rdy & srcB_rdy;

    always_ff @(posedge clk) begin
        if (rst) begin
            s2_reg <= '{default: '0};
        end
        else begin
            s2_reg.valid <= s1_reg.valid;
            s2_reg.tid <= s1_reg.tid;
            s2_reg.instr <= s1_reg.instr;
            s2_reg.finished <= s1_reg.finished;
            s2_reg.deps_met <= deps_met;
        end
    end

    // ==================================================
    //  Stage 3: Delay
    // ==================================================
    stage3_pipereg_t s3_reg;
    logic same_srcs;

    assign same_srcs = (s2_reg.instr.src_a == s2_reg.instr.src_b);

    always_ff @(posedge clk) begin
        if (rst) begin
            s3_reg <= '{default: '0};
        end
        else begin
            s3_reg.valid <= s2_reg.valid;
            s3_reg.tid <= s2_reg.tid;
            s3_reg.instr <= s2_reg.instr;
            s3_reg.deps_met <= s2_reg.deps_met;
            s3_reg.same_srcs <= same_srcs;
        end
    end

    // ==================================================
    //  Stage 4: ALU Issue
    // ==================================================
    logic [NUMBER_SIZE-1:0] init_data_A, init_data_B;
    logic [NUMBER_SIZE-1:0] reg_data_A, reg_data_B;
    logic [NUMBER_SIZE-1:0] alu_inA, alu_inB;
    logic vld_issue;

    assign alu_inA = s3_reg.instr.src_a.is_init ? init_data_A : reg_data_A;
    assign alu_inB = s3_reg.same_srcs ? alu_inA :
        s3_reg.instr.src_b.is_init ? init_data_B : reg_data_B;

    assign vld_issue = s3_reg.valid & s3_reg.deps_met;

    // ==================================================
    //  Stage 5: Writeback
    // ==================================================
    logic [NUMBER_SIZE-1:0] add_out, mul_out;
    alu_tag_t add_tag_out, mul_tag_out;

    // ==================================================
    //  Background Loader
    // ==================================================
    logic [1:0] load_step;
    logic [$clog2(THREAD_CNT)-1:0] load_tid;
    logic load_side;

    assign rdy_in = ~control_path_active | (load_side != active_side[load_tid]);

    always_ff @(posedge clk) begin
        if (rst) begin
            load_step <= '0;
            load_tid <= '0;
            load_side <= '0;
            control_path_active <= 1'b0;
        end
        else begin
            if (vld_in & rdy_in) begin
                if (load_step == (LOAD_STEPS - 1)) begin
                    load_step <= '0;

                    if (load_tid == THREAD_CNT - 1) begin
                        load_tid <= '0;
                        load_side <= ~load_side;

                        control_path_active <= 1'b1;
                    end
                    else begin
                        load_tid <= load_tid + 1;
                    end
                end
                else begin
                    load_step <= load_step + 1;
                end
            end
        end
    end

    regfile #(
        .NUMBER_SIZE(NUMBER_SIZE),
        .W(W),
        .MAX_THREADS(THREAD_CNT),
        .BANK_SLOTS('{3, 3, 3, 3}),
        .MAX_DEPTH(3)
    ) init_vals_regfile (
        // Write Port A
        .wr_data_A(bus0_in),
        .wr_en_A  (vld_in & rdy_in),
        .wr_bank_A({load_side, 1'b0}),
        .wr_tid_A (load_tid),
        .wr_idx_A (load_step),

        // Write Port B
        .wr_data_B(bus1_in),
        .wr_en_B  (vld_in & rdy_in),
        .wr_bank_B({load_side, 1'b1}),
        .wr_tid_B (load_tid),
        .wr_idx_B (load_step),

        // Read Port A
        .rd_data_A(init_data_A),
        .rd_en_A  (s2_reg.valid & s2_reg.instr.src_a.is_init),
        .rd_bank_A(s2_reg.instr.src_a.bank + (active_side[s2_reg.tid] ? 2'd2 : 2'd0)),
        .rd_tid_A (s2_reg.tid),
        .rd_idx_A (s2_reg.instr.src_a.idx),

        // Read Port B
        .rd_data_B(init_data_B),
        .rd_en_B  (s2_reg.valid & s2_reg.instr.src_b.is_init & ~same_srcs),
        .rd_bank_B(s2_reg.instr.src_b.bank + (active_side[s2_reg.tid] ? 2'd2 : 2'd0)),
        .rd_tid_B (s2_reg.tid),
        .rd_idx_B (s2_reg.instr.src_b.idx),

        // Vld Read Port A
        .rd_vld_A(init_vld_a),
        .rd_vld_bank_A(s1_init_bank_a),
        .rd_vld_tid_A(s1_reg.tid),
        .rd_vld_idx_A(s1_reg.instr.src_a.idx),

        // Vld Read Port B
        .rd_vld_B(init_vld_b),
        .rd_vld_bank_B(s1_init_bank_b),
        .rd_vld_tid_B(s1_reg.tid),
        .rd_vld_idx_B(s1_reg.instr.src_b.idx),

        // Flush
        .flush_vld(init_flush),
        .flush_tid(init_flush_tid),
        .flush_bank_mask(init_flush_side ? 4'b1100 : 4'b0011)
    );

    regfile #(
        .NUMBER_SIZE(NUMBER_SIZE),
        .W(W),
        .MAX_THREADS(THREAD_CNT),
        .BANK_SLOTS('{8, 5, 2, 2}),
        .MAX_DEPTH(8)
    ) val_regfile (
        // Write Port A
        .wr_data_A(add_out),
        .wr_en_A  (add_tag_out.vld_tag),
        .wr_bank_A(add_tag_out.dest.bank),
        .wr_tid_A (add_tag_out.tid),
        .wr_idx_A (add_tag_out.dest.idx),

        // Write Port B
        .wr_data_B(mul_out),
        .wr_en_B  (mul_tag_out.vld_tag),
        .wr_bank_B(mul_tag_out.dest.bank),
        .wr_tid_B (mul_tag_out.tid),
        .wr_idx_B (mul_tag_out.dest.idx),

        // Read Port A
        .rd_data_A(reg_data_A),
        .rd_en_A  (s2_reg.valid & ~s2_reg.instr.src_a.is_init),
        .rd_bank_A(s2_reg.instr.src_a.bank),
        .rd_tid_A (s2_reg.tid),
        .rd_idx_A (s2_reg.instr.src_a.idx),

        // Read Port B
        .rd_data_B(reg_data_B),
        .rd_en_B  (s2_reg.valid & ~s2_reg.instr.src_b.is_init & ~same_srcs),
        .rd_bank_B(s2_reg.instr.src_b.bank),
        .rd_tid_B (s2_reg.tid),
        .rd_idx_B (s2_reg.instr.src_b.idx),

        // Vld Read Port A
        .rd_vld_A(reg_vld_a),
        .rd_vld_bank_A(s1_reg.instr.src_a.bank),
        .rd_vld_tid_A(s1_reg.tid),
        .rd_vld_idx_A(s1_reg.instr.src_a.idx),

        // Vld Read Port B
        .rd_vld_B(reg_vld_b),
        .rd_vld_bank_B(s1_reg.instr.src_b.bank),
        .rd_vld_tid_B(s1_reg.tid),
        .rd_vld_idx_B(s1_reg.instr.src_b.idx),

        // Flush
        .flush_vld(reg_flush),
        .flush_tid(reg_flush_tid),
        .flush_bank_mask(4'b1111)
    );

    alu #(
        .NUMBER_SIZE(NUMBER_SIZE),
        .W(W),
        .MODULUS(MODULUS),
        .THREAD_CNT(THREAD_CNT)
    ) alu_0 (
        .clk(clk),
        .rst(rst),

        // In
        .op  (vld_issue ? s3_reg.instr.op : OP_NOOP),
        .tid (s3_reg.tid),
        .dest(s3_reg.instr.dest),
        .in_a(alu_inA),
        .in_b(alu_inB),

        // Out
        .add_out(add_out),
        .add_tag_out(add_tag_out),
        .mul_out(mul_out),
        .mul_tag_out(mul_tag_out),
    );

    // ==================================================
    //  Stage 5: Writeback
    // ==================================================
    logic is_X3_add, is_Y3_add;
    logic is_ZZ3_mul, is_ZZZ3_mul;

    assign is_X3_add = add_tag_out.vld_tag &
        (add_tag_out.dest.bank == X3.bank) & (add_tag_out.dest.idx == X3.idx);

    assign is_Y3_add = add_tag_out.vld_tag &
        (add_tag_out.dest.bank == Y3.bank) & (add_tag_out.dest.idx == Y3.idx);

    assign is_ZZ3_mul = mul_tag_out.vld_tag &
        (mul_tag_out.dest.bank == ZZ3.bank) & (mul_tag_out.dest.idx == ZZ3.idx);

    assign is_ZZZ3_mul = mul_tag_out.vld_tag &
        (mul_tag_out.dest.bank == ZZZ3.bank) & (mul_tag_out.dest.idx == ZZZ3.idx);

    logic [NUMBER_SIZE-1:0] out_buf_X3[THREAD_CNT];
    logic [NUMBER_SIZE-1:0] out_buf_Y3[THREAD_CNT];
    logic [NUMBER_SIZE-1:0] out_buf_ZZ3[THREAD_CNT];
    logic [NUMBER_SIZE-1:0] out_buf_ZZZ3[THREAD_CNT];

    logic [$clog2(THREAD_CNT)-1:0] ready_fifo[THREAD_CNT];
    logic [$clog2(THREAD_CNT)-1:0] fifo_wr_ptr, fifo_rd_ptr;
    logic fifo_empty;

    assign fifo_empty = (fifo_wr_ptr == fifo_rd_ptr);

    always_ff @(posedge clk) begin
        if (rst) begin
            fifo_wr_ptr <= '0;
        end
        else begin
            if (is_X3_add) out_buf_X3[add_tag_out.tid] <= add_out;
            if (is_ZZ3_mul) out_buf_ZZ3[mul_tag_out.tid] <= mul_out;
            if (is_ZZZ3_mul) out_buf_ZZZ3[mul_tag_out.tid] <= mul_out;

            if (is_Y3_add) begin
                out_buf_Y3[add_tag_out.tid] <= add_out;
                ready_fifo[fifo_wr_ptr%THREAD_CNT] <= add_tag_out.tid;
                fifo_wr_ptr <= fifo_wr_ptr + 1;
            end
        end
    end

    logic [2:0] stream_step;
    logic [$clog2(THREAD_CNT)-1:0] stream_tid;

    always_ff @(posedge clk) begin
        if (rst) begin
            stream_step <= '0;
            stream_tid  <= '0;
            fifo_rd_ptr <= '0;
            valid_out   <= '0;
        end
        else begin
            valid_out <= 1'b0;

            if (stream_step == 0) begin
                if (!fifo_empty) begin
                    stream_tid  <= ready_fifo[fifo_rd_ptr%THREAD_CNT];
                    fifo_rd_ptr <= fifo_rd_ptr + 1;
                    stream_step <= 3'd1;
                end
            end
            else begin
                valid_out <= 1'b1;

                unique case (stream_step)
                    3'd1: bus_out <= out_buf_X3[stream_tid];
                    3'd2: bus_out <= out_buf_Y3[stream_tid];
                    3'd3: bus_out <= out_buf_ZZ3[stream_tid];
                    3'd4: bus_out <= out_buf_ZZZ3[stream_tid];
                    default: bus_out <= '0;
                endcase

                if (stream_step == 3'd4) begin
                    stream_step <= 3'd0;
                end
                else begin
                    stream_step <= stream_step + 1;
                end
            end
        end
    end

endmodule : padd
