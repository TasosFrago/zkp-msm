module fetch
    import zbp_pkg::*;
(
    input logic clk,
    input logic rst,

    pipeline_if.out imem_req_if,
    pipeline_if.in  imem_rsp_if,

    input cf_redirect_t cf_redirect,
    input cf_pc_adv_t   cf_pc_adv,

    pipeline_if.out fetch_if
);

    logic [31:0] pc_tb[MAX_THREADS];
    logic [MAX_THREADS-1:0] busy_tb;
    logic [TID_W-1:0] tid_ptr;
    logic buff_ready;

    // Issue Insruction fetch from IMEM
    imem_req_t req_data;

    assign imem_req_if.valid = ~busy_tb[tid_ptr];
    assign req_data = '{
        tid: tid_ptr,
        pc:  pc_tb[tid_ptr]
    };
    assign imem_req_if.data  = req_data;

    always_ff @(posedge clk) begin
        if (rst) begin
            tid_ptr <= '0;
            busy_tb <= '0;
        end
        else begin
            if (~busy_tb[tid_ptr]) begin
                if (imem_req_if.ready) begin
                    busy_tb[tid_ptr] <= 1'b1;
                    tid_ptr <= (tid_ptr == TID_W'(MAX_THREADS-1)) ? '0 : tid_ptr + 1'b1;
                end
            end
            else begin
                tid_ptr <= (tid_ptr == TID_W'(MAX_THREADS-1)) ? '0 : tid_ptr + 1'b1;
            end

            if (cf_pc_adv.vld)   busy_tb[cf_pc_adv.tid] <= 1'b0;
            if (cf_redirect.vld) busy_tb[cf_redirect.tid] <= 1'b0;
        end
    end

    // Receive IMEM response
    imem_rsp_t rsp_data;
    fetch_out_t out_data;

    assign rsp_data = imem_rsp_if.data;

    assign imem_rsp_if.ready = buff_ready;
    assign out_data.tid   = rsp_data.tid;
    assign out_data.instr = rsp_data.instr;
    assign out_data.pc    = pc_tb[rsp_data.tid];

    // PC Registers
    always_ff @(posedge clk) begin
        if (rst) begin
            pc_tb <= '{default: '0};
        end
        else begin
            for(int t = 0; t < MAX_THREADS; t++) begin
                if (cf_redirect.vld && (cf_redirect.tid == TID_W'(t))) begin
                    pc_tb[t] <= cf_redirect.pc;
                end
                else if (cf_pc_adv.vld && (cf_pc_adv.tid == TID_W'(t))) begin
                    pc_tb[t] <= pc_tb[t] + 32'd4;
                end
            end
        end
    end

    skid_buffer #(
        .DATA_W($bits(fetch_out_t))
    ) fetch_pipe_buff (
        .clk(clk),
        .rst(rst),

        .valid_in(imem_rsp_if.valid),
        .ready_in(buff_ready),
        .data_in(out_data),

        .valid_out(fetch_if.valid),
        .ready_out(fetch_if.ready),
        .data_out(fetch_if.data)
    );

endmodule : fetch
