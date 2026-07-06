module sync_fifo #(
    parameter int DATA_W = 32,
    parameter int DEPTH = 4
) (
    input  logic              clk,
    input  logic              rst,

    input  logic [DATA_W-1:0] wdata_in,
    input  logic              wr_en,
    output logic              full,

    output logic [DATA_W-1:0] rdata_out,
    input  logic              rd_en,
    output logic              empty
);

    localparam int ADDR_W = $clog2(DEPTH);

    logic [ADDR_W:0] wptr, rptr;

    logic [DATA_W-1:0] mem[DEPTH];

    // Write Logic
    always_ff @(posedge clk) begin
        if (rst) begin
            wptr <= '0;
        end
        else if (wr_en && !full) begin
            mem[wptr[ADDR_W-1:0]] <= wdata_in;
            wptr                  <= wptr + 1;
        end
    end

    // Read Logic
    always_ff @(posedge clk) begin
        if (rst) begin
            rptr      <= '0;
            rdata_out <= '0;
        end
        else if (rd_en && !empty) begin
            rdata_out <= mem[rptr[ADDR_W-1:0]];
            rptr      <= rptr + 1;
        end
    end

    assign empty = (wptr == rptr);

    assign full  = (wptr[ADDR_W] != rptr[ADDR_W]) &&
                   (wptr[ADDR_W-1:0] == rptr[ADDR_W-1:0]);

endmodule : sync_fifo
