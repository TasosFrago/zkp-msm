module shiftreg #(
    parameter int DATA_W = 32,
    parameter int DEPTH = 4
) (
    input logic clk,
    input logic rst,

    input  logic              valid_in,
    input  logic [DATA_W-1:0] data_in,
    output logic [DATA_W-1:0] data_out
);

    logic [DATA_W-1:0] shiftreg [DEPTH];

    always_ff @(posedge clk) begin
        if (rst) begin
            shiftreg <= '{default: '0};
        end
        else begin
            for(int i = 1; i < DEPTH; i++) begin
                shiftreg[i] <= shiftreg[i-1];
            end

            if (valid_in) shiftreg[0] <= data_in;
            else shiftreg[0] <= '0;
        end
    end

    assign data_out = shiftreg[DEPTH-1];

endmodule : shiftreg
