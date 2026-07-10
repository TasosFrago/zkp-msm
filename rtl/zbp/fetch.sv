module fetch
    import zbp_pkg::*;
(
    input logic clk,
    input logic rst,

    pipeline_if.out imem_req_if,
    pipeline_if.in  imem_rsp_if,

    input cf_redirect_t cf_redirect,
    input cf_pc_adv_t   cf_pc_adv,

    pipeline_if.in iss_back_if,
    pipeline_if.out fetch_if
);

    logic [31:0] pc_tb[MAX_THREADS];
    logic [MAX_THREADS-1:0] busy_tb;
    logic [TID_W-1:0] tid_ptr;
    logic buff_ready;
    logic buff_rdy;

    iss_back_t iss_back_d;
    assign iss_back_d = iss_back_if.data;
    assign iss_back_if.ready = TRUE;

    // Issue Insruction fetch from IMEM
    imem_req_t req_data;

    assign req_data = '{
        tid: tid_ptr,
        pc:  pc_tb[tid_ptr]
    };

    always_ff @(posedge clk) begin
        if (rst) begin
            tid_ptr <= '0;
            busy_tb <= '0;
        end
        else begin
            if (~busy_tb[tid_ptr]) begin
                if (buff_rdy) begin
                    busy_tb[tid_ptr] <= TRUE;
                    tid_ptr <= (tid_ptr == TID_W'(MAX_THREADS-1)) ? '0 : tid_ptr + 1'b1;
                end
            end
            else begin
                tid_ptr <= (tid_ptr == TID_W'(MAX_THREADS-1)) ? '0 : tid_ptr + 1'b1;
            end

            // if (iss_back_if.valid & ~iss_back_d.issued) begin
            //     busy_tb[iss_back_d.tid] <= FALSE;
            // end

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

    skid_buffer #(
        .DATA_W($bits(imem_req_t))
    ) imem_pipe_buff (
        .clk(clk),
        .rst(rst),

        .valid_in(~busy_tb[tid_ptr]),
        .ready_in(buff_rdy),
        .data_in(req_data),

        .valid_out(imem_req_if.valid),
        .ready_out(imem_req_if.ready),
        .data_out(imem_req_if.data)
    );

endmodule : fetch
