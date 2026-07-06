module dmem #(
    parameter string INIT_FILE = "",
    parameter int MEM_SIZE_BYTES = 65536,
    parameter int MIN_LATENCY = 1,
    parameter int MAX_RANDOM_DELAY = 5,
    parameter int RANDOM_STALL_PROB = 20,
    parameter int MAX_PENDING_REQS = 8
) (
    input logic clk,
    input logic rst,

    pipeline_if.in  req_if,
    pipeline_if.out rsp_if
);

    logic [31:0] mem [(MEM_SIZE_BYTES / 4)];

    initial begin
        for (int i = 0; i < (MEM_SIZE_BYTES/4); i++) mem[i] = 32'hXXXXXXXX;

        if (INIT_FILE != "") begin
            $readmemh(INIT_FILE, mem);
            $display("DMEM: Loaded %s", INIT_FILE);
        end
    end

    import zbp_pkg::dmem_size_t;
    import zbp_pkg::NUMBER_SIZE;

    typedef struct {
        dmem_size_t size;
        logic [NUMBER_SIZE-1:0] data;
        int delay_cnt;
    } pending_rsp_t;

    pending_rsp_t rsp_queue[$];

    logic rand_ready;
    always_ff @(posedge clk) begin
        if (rst) rand_ready <= 1'b0;
        else if (RANDOM_STALL_PROB > 0) rand_ready <= ($urandom() % 100) >= RANDOM_STALL_PROB;
        else rand_ready <= 1'b1;
    end

    assign req_if.ready = rand_ready & (rsp_queue.size() < MAX_PENDING_REQS);

    import zbp_pkg::dmem_req_t;
    dmem_req_t req;
    assign req = req_if.data;

    import zbp_pkg::DMEM_B;
    import zbp_pkg::DMEM_H;
    import zbp_pkg::DMEM_W;
    import zbp_pkg::DMEM_V;

    int bytes_len;
    always_comb begin
        case (req.size)
            DMEM_B: bytes_len = 1;
            DMEM_H: bytes_len = 2;
            DMEM_W: bytes_len = 4;
            DMEM_V: bytes_len = $bits(req.data);
            default: bytes_len = 4;
        endcase
    end

    always_ff @(posedge clk) begin
        if (rst) begin
            rsp_queue.delete();
        end
        else begin

            if (req_if.valid & req_if.ready) begin
                int current_addr;
                int word_idx;
                int byte_offset;

                if (req.send_data == 1'b1) begin
                    for(int i = 0; i < bytes_len; i++) begin
                        current_addr = req.addr + i;
                        word_idx = current_addr / 4;
                        byte_offset = current_addr % 4;

                        if (word_idx < (MEM_SIZE_BYTES / 4)) begin
                            mem[word_idx][8*byte_offset+:8] = req.data[8*i +: 8];
                        end
                    end
                end
                else begin
                    pending_rsp_t new_rsp;
                    new_rsp.size = req.size;
                    new_rsp.data = '0;

                    for(int i = 0; i < bytes_len; i++) begin
                        current_addr = req.addr + i;
                        word_idx = current_addr / 4;
                        byte_offset = current_addr % 4;

                        if (word_idx < (MEM_SIZE_BYTES / 4)) begin
                            new_rsp.data[8*i +: 8] = mem[word_idx][8*byte_offset +: 8];

                            if (mem[word_idx][8*byte_offset +: 8] === 8'hXX) begin
                                $warning("DMEM [t=%0t]: Read from UNINITIALIZED memory at addr 0x%0h", $time, current_addr);
                            end
                        end
                        else begin
                            $error("DMEM [t=%0t]: Out-of-bounds memory read at addr 0x%0h", $time, current_addr);
                        end
                    end

                    new_rsp.delay_cnt = MIN_LATENCY + ($urandom() % (MAX_RANDOM_DELAY + 1));
                    rsp_queue.push_back(new_rsp);
                end
            end

            foreach(rsp_queue[i]) begin
                if(rsp_queue[i].delay_cnt > 0) begin
                    rsp_queue[i].delay_cnt--;
                end
            end

            if (rsp_if.valid & rsp_if.ready) begin
                rsp_queue.pop_front();
            end

        end
    end

    import zbp_pkg::dmem_rsp_t;
    dmem_rsp_t rsp_out;

    always_comb begin
        rsp_if.valid = 1'b0;
        rsp_out = '{ size: DMEM_B, default: '0 };

        if (rsp_queue.size() > 0) begin
            if (rsp_queue[0].delay_cnt == 0) begin
                rsp_if.valid = 1'b1;
                rsp_out.data = rsp_queue[0].data;
                rsp_out.size = rsp_queue[0].size;
            end
        end
    end

    assign rsp_if.data = rsp_out;

endmodule : dmem
