module fifo #(
    parameter int DATA_W = 32,
    parameter int DEPTH = 4
) (
    input logic clk,
    input logic rst,

    // Writer interface
    input  logic              wr_valid,
    output logic              wr_ready,
    input  logic [DATA_W-1:0] wr_data,

    // Reader interface
    output logic              rd_valid,
    input  logic              rd_ready,
    output logic [DATA_W-1:0] rd_data
);

    logic [DATA_W-1:0] mem [DEPTH];

    localparam int PTR_W = $clog2(DEPTH);
    localparam int CNT_W = $clog2(DEPTH + 1);

    logic [PTR_W-1:0] wr_ptr;
    logic [PTR_W-1:0] rd_ptr;
    logic [CNT_W-1:0] count;

    logic wr_en, rd_en;

    assign wr_ready = (count < CNT_W'(DEPTH));
    assign rd_valid = (count > 0);

    assign wr_en = wr_valid & wr_ready;
    assign rd_en = rd_valid & rd_ready;

    assign rd_data = mem[rd_ptr];

    always_ff @(posedge clk) begin
        if (rst) begin
            wr_ptr <= '0;
            rd_ptr <= '0;
            count  <= '0;
        end
        else begin

            if (wr_en) begin
                mem[wr_ptr] <= wr_data;

                if (wr_ptr == PTR_W'(DEPTH - 1)) begin
                    wr_ptr <= '0;
                end
                else begin
                    wr_ptr <= wr_ptr + 1;
                end
            end

            if (rd_en) begin
                if (rd_ptr == PTR_W'(DEPTH - 1)) begin
                    rd_ptr <= '0;
                end
                else begin
                    rd_ptr <= rd_ptr + 1;
                end
            end

            if (wr_en & ~rd_en) begin
                count <= count + 1;
            end
            else if (~wr_en & rd_en) begin
                count <= count -1;
            end

        end
    end

endmodule : fifo
