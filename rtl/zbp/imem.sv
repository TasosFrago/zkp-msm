module imem
    import zbp_pkg::imem_req_t;
    import zbp_pkg::imem_rsp_t;
#(
    parameter string INIT_FILE = "",
    parameter int MEM_SIZE_WORDS = 1024,
    parameter logic RANDOM_STALLS = 1'b0
) (
    input logic clk,
    input logic rst,

    pipeline_if.in  req_if,
    pipeline_if.out rsp_if
);

    logic [31:0] imem_arr[MEM_SIZE_WORDS];

    import zbp_pkg::NOOP_INSTR;

    initial begin
        if (INIT_FILE != "") begin
            $readmemh(INIT_FILE, imem_arr);
            $display("IMEM: Loaded %s", INIT_FILE);
        end
        else begin
            for(int i = 0; i < MEM_SIZE_WORDS; i++) imem_arr[i] = NOOP_INSTR;
        end
    end

    imem_req_t req;
    assign req = req_if.data;

    logic stage_valid;
    imem_rsp_t stage_rsp;

    logic random_ready;
    always_ff @(posedge clk) begin
        if (RANDOM_STALLS) random_ready <= ($urandom() % 100) > 30;
        else               random_ready <= 1'b1;
    end

    assign req_if.ready = random_ready & (~stage_valid | rsp_if.ready);

    always_ff @(posedge clk) begin
        if (rst) begin
            stage_valid <= 1'b0;
            stage_rsp   <= '{default: '0};
        end
        else begin

            if (rsp_if.ready & stage_valid) begin
                stage_valid <= 1'b0;
            end

            if (req_if.valid & req_if.ready) begin
                stage_valid <= 1'b1;
                stage_rsp.tid <= req.tid;

                if ((req.pc >> 2) < MEM_SIZE_WORDS) begin
                    stage_rsp.instr <= imem_arr[req.pc >> 2];
                end
                else begin
                    stage_rsp.instr <= NOOP_INSTR;
                    $display("IMEM WARNING: Fetch out of bounds at tid=%d, PC=%0h", req.tid, req.pc);
                end
            end

        end
    end

    assign rsp_if.valid = stage_valid;
    assign rsp_if.data = stage_rsp;

endmodule : imem
