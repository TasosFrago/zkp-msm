module balu
    import zbp_pkg::*;
(
    input logic clk,
    input logic rst,

    input logic                   valid_in,
    input logic [      TID_W-1:0] tid,
    input logic [    R_IDX_W-1:0] rd,
    input op_tag_t                op_tag,
    input logic [NUMBER_SIZE-1:0] opa,
    input logic [NUMBER_SIZE-1:0] opb,

    output logic valid_out,
    output bwb_t wbBN
);


    always_ff @(posedge clk) begin
        if (rst) begin
            valid_out <= FALSE;
            wbBN <= '{default: '0};
        end
        else begin
            case (op_tag)
                OP_BMV: begin
                    valid_out <= valid_in;
                    wbBN <= '{
                        tag: '{en: valid_in, tid: tid, rd: rd},
                        data: opa
                    };
                end
                default: begin
                    valid_out <= valid_in;
                    wbBN <= '{
                        tag: '{en: valid_in, tid: tid, rd: rd},
                        data: { CHUNKS{32'haadbeef} }
                    };
                end
            endcase
        end
    end

endmodule : balu
