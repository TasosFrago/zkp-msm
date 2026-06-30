package isa_pkg;

    typedef enum logic [7-1:0] {
        OPCODE_OP       = 7'b0110011, // R-type arithmetic
        OPCODE_OP_IMM   = 7'b0010011, // I-type arithmetic
        OPCODE_LOAD     = 7'b0000011, // I-type loads
        OPCODE_STORE    = 7'b0100011, // S-type stores
        OPCODE_BRANCH   = 7'b1100011, // B-type branches
        OPCODE_JAL      = 7'b1101111, // J-type jump and link
        OPCODE_JALR     = 7'b1100111, // I-type jump and link reg
        OPCODE_LUI      = 7'b0110111, // U-type load upper imm
        OPCODE_AUIPC    = 7'b0010111, // U-type add upper imm to PC
        OPCODE_SYSTEM   = 7'b1110011  // I-type system/CSRs
    } opcode_t;

    typedef enum logic [3-1:0] {
        F3_ADD_SUB = 3'h0,
        F3_SLL     = 3'h1,
        F3_SLT     = 3'h2,
        F3_SLTU    = 3'h3,
        F3_XOR     = 3'h4,
        F3_SRL_SRA = 3'h5,
        F3_OR      = 3'h6,
        F3_AND     = 3'h7
    } funct3_alu_t;

    typedef enum logic [3-1:0] {
        F3_BEQ  = 3'h0,
        F3_BNE  = 3'h1,
        F3_BLT  = 3'h4,
        F3_BGE  = 3'h5,
        F3_BLTU = 3'h6,
        F3_BGEU = 3'h7
    } funct3_branch_t;

    typedef enum logic [3-1:0] {
        F3_BYTE  = 3'h0, // LB / SB
        F3_HALF  = 3'h1, // LH / SH
        F3_WORD  = 3'h2, // LW / SW
        F3_BYTEU = 3'h4, // LBU
        F3_HALFU = 3'h5  // LHU
    } funct3_mem_t;

    typedef enum logic [7-1:0] {
        F7_BASE = 7'h00,
        F7_ALT  = 7'h20,
        F7_MUL  = 7'h01
    } funct7_alu_t;

endpackage : isa_pkg
