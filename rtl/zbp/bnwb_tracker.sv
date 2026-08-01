module bnwb_tracker
    import zbp_pkg::*;
(
    input logic clk,
    input logic rst,

    input logic is_bn_write,
    input eu_tag_t eu_tag,
    input op_tag_t op_tag,
    input logic [1:0] bank_target,

    input logic valid_in,

    output logic write_collision,
    output logic [3:0] bank_tracker,
    output logic [1:0] port_tracker
);

    logic [31:0] wb_bank_tracker[4];
    logic [31:0] wb_port_tracker[2];
    logic [31:0] next_bank_tracker[4];
    logic [31:0] next_port_tracker[2];

    always_comb begin
        for(int b = 0; b < 4; b++) bank_tracker[b] = wb_bank_tracker[b][0];
        for(int p = 0; p < 2; p++) port_tracker[p] = wb_port_tracker[p][0];
    end

    logic [4:0] exec_lat;

    localparam int LAT_CORRECTION = 1;
    always_comb begin
        case (eu_tag)
            EU_BMADD: exec_lat = 'd9  - 5'(LAT_CORRECTION);
            EU_BMMUL: exec_lat = 'd24 - 5'(LAT_CORRECTION);
            EU_BALU: begin
                case (op_tag)
                    OP_BMV,
                    OP_BSHFL: exec_lat = 'd1  - 5'(LAT_CORRECTION);
                    default:  exec_lat = 'd1  - 5'(LAT_CORRECTION);
                endcase
            end
            EU_BCMP:  exec_lat = 'd2  - 5'(LAT_CORRECTION);
            default:  exec_lat = 0;
        endcase
    end

    assign write_collision = is_bn_write &
        (wb_bank_tracker[bank_target][exec_lat + 1] |
        (wb_port_tracker[0][exec_lat + 1] & wb_port_tracker[1][exec_lat + 1]));

    always_comb begin
        for(int b = 0; b < 4; b++) next_bank_tracker[b] = wb_bank_tracker[b] >> 1;
        for(int p = 0; p < 2; p++) next_port_tracker[p] = wb_port_tracker[p] >> 1;

        if (valid_in & (exec_lat > 0) &
            (is_bn_write)) begin

            next_bank_tracker[bank_target][exec_lat] = 1'b1;

            if (~next_port_tracker[0][exec_lat]) begin
                next_port_tracker[0][exec_lat] = 1'b1;
            end
            else begin
                assert(~next_port_tracker[1][exec_lat])
                else $fatal(1, "Both write ports already allocated. EXEC: %s, RD bank: (%0d), EU: %s, OP: %s",
                    (valid_in ? "VALID" : "NOT VALID"),
                    bank_target,
                    eu_tag.name(),
                    op_tag.name());
                next_port_tracker[1][exec_lat] = 1'b1;
            end
        end
    end

    always_ff @(posedge clk) begin
        if (rst) begin
            wb_bank_tracker <= '{default: '0};
            wb_port_tracker <= '{default: '0};
        end
        else begin
            wb_bank_tracker <= next_bank_tracker;
            wb_port_tracker <= next_port_tracker;
        end
    end

endmodule : bnwb_tracker
