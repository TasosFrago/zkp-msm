module decode
    import zbp_pkg::*;
    import isa_pkg::*;
(
    input logic clk,
    input logic rst,

    pipeline_if.in fetch_if,
    pipeline_if.out decode_if
);

    decode_out_t out_data;
    fetch_out_t in_data;

    assign in_data = fetch_if.data;

    logic [31:0] instr;
    logic [6:0] opcode;
    logic [2:0] funct3;
    logic [6:0] funct7;

    assign instr = in_data.instr;
    assign opcode = instr[6:0];
    assign funct3 = instr[14:12];
    assign funct7 = instr[31:25];

    logic [19:0] cimm_i, cimm_s, cimm_b, cimm_j, cimm_u;

    assign cimm_i = { {8{instr[31]}}, instr[31:20] };
    assign cimm_s = { {8{instr[31]}}, instr[31:25], instr[11:7] };
    assign cimm_b = { {9{instr[31]}}, instr[7], instr[30:25], instr[11:8] };
    assign cimm_j = { instr[31], instr[19:12], instr[20], instr[30:21] };
    assign cimm_u = { instr[31:12] };

    always_comb begin
        out_data = '{eu_tag: EU_NOOP, op_tag: OP_NONE, default: '0};
        out_data.tid      = in_data.tid;
        out_data.pc       = in_data.pc;
        out_data.eu_tag   = EU_NOOP;
        out_data.op_tag   = OP_NONE;
        out_data.rs1      = OP_ZERO_REG;
        out_data.rs2      = OP_RS2_ZERO_REG;
        out_data.rd       = OP_ZERO_REG;
        out_data.rd_is_rs = FALSE;

        case (opcode)
            OPCODE_OP: begin
                out_data.eu_tag = EU_SALU;

                out_data.rs1 = '{
                    en:   TRUE,
                    idx:  instr[19:15],
                    is_v: FALSE
                };

                out_data.rd = '{
                    en:   TRUE,
                    idx:  instr[11:7],
                    is_v: FALSE
                };

                out_data.rs2.is_imm        = FALSE;
                out_data.rs2.val.as_r.idx  = instr[24:20];
                out_data.rs2.val.as_r.is_v = FALSE;

                case (funct3)
                    F3_ADD_SUB: out_data.op_tag = (funct7 == F7_ALT) ? OP_SUB :
                                                  (funct7 == F7_MUL) ? OP_MUL :
                                                   OP_ADD;
                    F3_SLL:     out_data.op_tag = OP_SLL;
                    F3_SLT:     out_data.op_tag = OP_SLT;
                    F3_SLTU:    out_data.op_tag = OP_SLTU;
                    F3_XOR:     out_data.op_tag = OP_XOR;
                    F3_SRL_SRA: out_data.op_tag = (funct7 == F7_ALT) ? OP_SRA : OP_SRL;
                    F3_OR:      out_data.op_tag = OP_OR;
                    F3_AND:     out_data.op_tag = OP_AND;
                    default:    out_data.op_tag = OP_INVALID;
                endcase
            end

            OPCODE_OP_IMM: begin
                out_data.eu_tag = EU_SALU;

                out_data.rs1 = '{
                    en:   TRUE,
                    idx:  instr[19:15],
                    is_v: FALSE
                };

                out_data.rd = '{
                    en:   TRUE,
                    idx:  instr[11:7],
                    is_v: FALSE
                };

                out_data.rs2.is_imm      = TRUE;
                out_data.rs2.val.as_imm.fmt  = IMM_I_S;
                out_data.rs2.val.as_imm.bits = cimm_i;

                case (funct3)
                    F3_ADD_SUB: out_data.op_tag = OP_ADD;
                    F3_SLL: begin

                        case (instr[31:25])
                            F7_BASE: begin
                                out_data.op_tag = OP_SLL;
                                out_data.rs2.val.as_imm.bits = { 15'b0, instr[24:20] };
                            end
                            F7_ALT: begin
                                out_data.op_tag = (instr[24:20] == 5'h01) ? OP_CTZ : OP_INVALID;
                                out_data.rs2.val.as_imm.bits = '0;
                            end
                            default: out_data.op_tag = OP_INVALID;
                        endcase

                    end
                    F3_SLT:     out_data.op_tag = OP_SLT;
                    F3_SLTU:    out_data.op_tag = OP_SLTU;
                    F3_XOR:     out_data.op_tag = OP_XOR;
                    F3_SRL_SRA: begin
                        out_data.op_tag = (funct7 == F7_ALT) ? OP_SRA : OP_SRL;
                        out_data.rs2.val.as_imm.bits = { 15'b0, instr[24:20] };
                    end
                    F3_OR:      out_data.op_tag = OP_OR;
                    F3_AND:     out_data.op_tag = OP_AND;
                    default:    out_data.op_tag = OP_INVALID;
                endcase
            end

            OPCODE_LOAD: begin
                out_data.eu_tag = EU_LSU;

                out_data.rs1 = '{
                    en:   TRUE,
                    idx:  instr[19:15],
                    is_v: FALSE
                };

                out_data.rd = '{
                    en:   TRUE,
                    idx:  instr[11:7],
                    is_v: FALSE
                };

                out_data.rs2.is_imm          = TRUE;
                out_data.rs2.val.as_imm.fmt  = IMM_I_S;
                out_data.rs2.val.as_imm.bits = cimm_i;

                case (funct3)
                    F3_BYTE:  out_data.op_tag = OP_LB;
                    F3_HALF:  out_data.op_tag = OP_LH;
                    F3_WORD:  out_data.op_tag = OP_LW;
                    F3_BYTEU: out_data.op_tag = OP_LBU;
                    F3_HALFU: out_data.op_tag = OP_LHU;
                    default:  out_data.op_tag = OP_INVALID;
                endcase
            end

            OPCODE_STORE: begin
                out_data.eu_tag = EU_LSU;

                out_data.rs1 = '{
                    en:   TRUE,
                    idx:  instr[19:15],
                    is_v: FALSE
                };

                // On the STORE instrs we repurpose rd to rs2 because rs2 holds imm
                out_data.rd_is_rs = TRUE;
                out_data.rd = '{
                    en:   TRUE,
                    idx:  instr[24:20],
                    is_v: FALSE
                };

                out_data.rs2.is_imm          = TRUE;
                out_data.rs2.val.as_imm.fmt  = IMM_I_S;
                out_data.rs2.val.as_imm.bits = cimm_s;

                case (funct3)
                    F3_BYTE:  out_data.op_tag = OP_SB;
                    F3_HALF:  out_data.op_tag = OP_SH;
                    F3_WORD:  out_data.op_tag = OP_SW;
                    default:  out_data.op_tag = OP_INVALID;
                endcase
            end

            OPCODE_BRANCH: begin
                out_data.eu_tag = EU_CF;

                out_data.rs1 = '{
                    en:   TRUE,
                    idx:  instr[19:15],
                    is_v: FALSE
                };

                out_data.rd_is_rs = TRUE;
                out_data.rd = '{
                    en:   TRUE,
                    idx:  instr[24:20],
                    is_v: FALSE
                };

                out_data.rs2.is_imm      = TRUE;
                out_data.rs2.val.as_imm.fmt  = IMM_J_B;
                out_data.rs2.val.as_imm.bits = cimm_b;

                case (funct3)
                    F3_BEQ:  out_data.op_tag = OP_BEQ;
                    F3_BNE:  out_data.op_tag = OP_BNE;
                    F3_BLT:  out_data.op_tag = OP_BLT;
                    F3_BGE:  out_data.op_tag = OP_BGE;
                    F3_BLTU: out_data.op_tag = OP_BLTU;
                    F3_BGEU: out_data.op_tag = OP_BGEU;
                    default: out_data.op_tag = OP_INVALID;
                endcase
            end

            OPCODE_JAL: begin
                out_data.eu_tag = EU_CF;
                out_data.op_tag = OP_JAL;

                out_data.rs1.en = FALSE;
                out_data.rd = '{
                    en:   TRUE,
                    idx:  instr[11:7],
                    is_v: FALSE
                };

                out_data.rs2.is_imm          = TRUE;
                out_data.rs2.val.as_imm.fmt  = IMM_J_B;
                out_data.rs2.val.as_imm.bits = cimm_j;
            end

            OPCODE_JALR: begin
                out_data.eu_tag = EU_CF;
                out_data.op_tag = OP_JALR;

                out_data.rs1 = '{
                    en:   TRUE,
                    idx:  instr[19:15],
                    is_v: FALSE
                };

                out_data.rd = '{
                    en:   TRUE,
                    idx:  instr[11:7],
                    is_v: FALSE
                };

                out_data.rs2.is_imm          = TRUE;
                out_data.rs2.val.as_imm.fmt  = IMM_I_S;
                out_data.rs2.val.as_imm.bits = cimm_i;
            end

            OPCODE_LUI: begin
                out_data.eu_tag = EU_SALU;
                out_data.op_tag = OP_LUI;

                out_data.rs1.en = FALSE;

                out_data.rd = '{
                    en:   TRUE,
                    idx:  instr[11:7],
                    is_v: FALSE
                };

                out_data.rs2.is_imm          = TRUE;
                out_data.rs2.val.as_imm.fmt  = IMM_U;
                out_data.rs2.val.as_imm.bits = cimm_u;
            end

            OPCODE_AUIPC: begin
                out_data.eu_tag = EU_SALU;
                out_data.op_tag = OP_AUIPC;

                out_data.rs1.en = FALSE;

                out_data.rd = '{
                    en:   TRUE,
                    idx:  instr[11:7],
                    is_v: FALSE
                };

                out_data.rs2.is_imm          = TRUE;
                out_data.rs2.val.as_imm.fmt  = IMM_U;
                out_data.rs2.val.as_imm.bits = cimm_u;
            end

            OPCODE_SYSTEM: begin
                out_data.eu_tag = EU_SYNC;
                out_data.op_tag = OP_NONE;
            end

            default: begin
                out_data.eu_tag = EU_NOOP;
                out_data.op_tag = OP_INVALID;
            end

        endcase
    end

    skid_buffer #(
        .DATA_W($bits(decode_out_t))
    ) decode_pipe_buffer (
        .clk(clk),
        .rst(rst),

        .valid_in(fetch_if.valid),
        .ready_in(fetch_if.ready),
        .data_in(out_data),

        .valid_out(decode_if.valid),
        .ready_out(decode_if.ready),
        .data_out(decode_if.data)
    );

endmodule : decode
