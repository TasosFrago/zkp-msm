module salu
    import zbp_pkg::*;
(
    input logic clk,
    input logic rst,

    input logic               valid_in,
    input logic [  TID_W-1:0] tid,
    input logic [R_IDX_W-1:0] rd,

    input  logic [W-1:0] opa,
    input  logic [W-1:0] opb,
    output swb_t         res,

    input op_tag_t op_tag
);

    logic [W-1:0] res_out;

    logic [5:0] ctz_cnt;
    always_comb begin
        ctz_cnt = '0;
        for(int i = 0; i < W; i++) begin
            if (opa[i]) begin
                ctz_cnt = 6'(i);
                break;
            end
        end
    end

    always_comb begin
        case (op_tag)
            OP_ADD,
            OP_AUIPC: res_out = opa + opb;
            OP_SUB:   res_out = opa - opb;
            OP_XOR:   res_out = opa ^ opb;
            OP_OR:    res_out = opa | opb;
            OP_AND:   res_out = opa & opb;
            OP_SLL:   res_out = opa << opb[4:0];
            OP_SRL:   res_out = opa >> opb[4:0];
            OP_SRA:   res_out = $signed(opa) >>> opb[4:0];
            OP_SLT:   res_out = { 31'd0, ($signed(opa) < $signed(opb)) };
            OP_SLTU:  res_out = { 31'd0, (opa < opb) };
            OP_MUL:   res_out = opa * opb;
            OP_LUI:   res_out = opb;
            OP_CTZ:   res_out = { {(W-6){1'b0}}, ctz_cnt };
            default:  res_out = 32'hbaadbeef;
        endcase
    end

    always_ff @(posedge clk) begin
        if (rst) begin
            res <= '{default: '0};
        end
        else begin
            res <= '{
                tag: '{
                    en:  valid_in,
                    tid: tid,
                    rd:  rd
                },
                data: res_out
            };
        end
    end

endmodule : salu
