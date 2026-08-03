module sbu
    import zbp_pkg::*;
(
    input logic clk,
    input logic rst,

    input logic             valid_in,
    input logic [TID_W-1:0] tid,

    output logic sync_finished
);

    typedef enum logic [1:0] {
        INACTIVE,
        WAITING,
        FINISHED
    } state_t;

    state_t state;

    logic [MAX_THREADS-1:0] thread_map, next_map;

    assign next_map = thread_map | (1 << tid);

    always_ff @(posedge clk) begin
        if (rst) begin
            state <= INACTIVE;
            thread_map <= '0;
        end
        else begin
            unique case (state)
                INACTIVE: begin
                    if (valid_in) begin
                        thread_map <= next_map;
                        state <= WAITING;
                    end
                end
                WAITING: begin
                    if (valid_in) begin
                        thread_map <= next_map;

                        if (next_map == 32'hFFFF_FFFF) begin
                            state <= FINISHED;
                        end
                    end
                end
                FINISHED: begin
                    thread_map <= '0;
                    state <= INACTIVE;
                end
            endcase
        end
    end

    assign sync_finished = (state == FINISHED);

endmodule : sbu
