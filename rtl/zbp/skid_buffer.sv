module skid_buffer #(
    parameter int DATA_W = 32
) (
    input logic clk,
    input logic rst,

    // Incoming data interface
    input  logic              valid_in,
    output logic              ready_in,
    input  logic [DATA_W-1:0] data_in,

    // Outgoing data interface
    output logic              valid_out,
    input  logic              ready_out,
    output logic [DATA_W-1:0] data_out
);

    logic              skid_valid;
    logic [DATA_W-1:0] skid_data;

    assign ready_in = ~skid_valid;

    always_ff @(posedge clk) begin
        if (rst) begin
            skid_valid <= 1'b0;
        end
        else if ((valid_in & ready_in) && (valid_out & ~ready_out)) begin
            skid_valid <= 1'b1;
        end
        else if (ready_out) begin
            skid_valid <= 1'b0;
        end
    end

    always_ff @(posedge clk) begin
        if (rst) begin
            skid_data <= '0;
        end
        else if (valid_in & ready_in) begin
            skid_data <= data_in;
        end
    end

    always_ff @(posedge clk) begin
        if (rst) begin
            valid_out <= 1'b0;
        end
        else if (~valid_out | ready_out) begin
            valid_out <= (valid_in | skid_valid);
        end
    end

    always_ff @(posedge clk) begin
        if (rst) begin
            data_out <= '0;
        end
        else if (~valid_out | ready_out) begin
            if (skid_valid) begin
                data_out <= skid_data;
            end
            else begin
                data_out <= data_in;
            end
        end
    end

endmodule : skid_buffer
