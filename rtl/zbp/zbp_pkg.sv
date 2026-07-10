`define DEBUG

package zbp_pkg;
    typedef enum logic {
        FALSE = 1'b0,
        TRUE  = 1'b1
    } bool_t;

    localparam int NUMBER_SIZE = 256;
    localparam int W = 32;
    localparam int CHUNKS = (NUMBER_SIZE / W);

    localparam int MAX_THREADS = 32;
    localparam int TID_W = $clog2(MAX_THREADS);

    localparam int REGISTERS = 32;
    localparam int R_IDX_W = $clog2(REGISTERS);
    localparam logic [4:0] VREGISTERS = 20;

    typedef enum logic [R_IDX_W-1:0] {
        ZERO_REG = 'd0,
        TID_REG  = 'd4
    } reg_idx_e;

    typedef struct packed {
        logic [TID_W-1:0] tid;
        logic [     31:0] pc;
    } imem_req_t;

    typedef struct packed {
        logic [TID_W-1:0] tid;
        logic [     31:0] instr;
    } imem_rsp_t;

    typedef struct packed {
        logic vld;
        logic [31:0] pc;
        logic [TID_W-1:0] tid;
    } cf_redirect_t;

    typedef struct packed {
        logic vld;
        logic [TID_W-1:0] tid;
    } cf_pc_adv_t;

    typedef struct packed {
        logic issued;
        logic [TID_W-1:0] tid;
    } iss_back_t;

    typedef struct packed {
        logic [TID_W-1:0] tid;
        logic [31:0] instr;
        logic [31:0] pc;
    } fetch_out_t;

    // Execution unit tag
    typedef enum logic [3:0] {
        EU_NOOP,
        EU_VMADD,
        EU_VMMUL,
        EU_VCMP,
        EU_LSU,
        EU_SALU,
        EU_CF, // Control flow
        EU_SYNC
    } eu_tag_t;

    // Enum to differentiate the operations inside each selected exec unit
    typedef enum logic [5:0] {
        // Scalar ALU
        OP_ADD,
        OP_SUB,
        OP_XOR,
        OP_OR,
        OP_AND,
        OP_SLL,
        OP_SRL,
        OP_SRA,
        OP_SLT,
        OP_SLTU,
        OP_MUL,
        OP_LUI,
        OP_AUIPC,
        OP_CTZ,

        OP_MATCH,
        OP_SHFL,

        // Control flow
        OP_BEQ,
        OP_BNE,
        OP_BLT,
        OP_BGE,
        OP_BLTU,
        OP_BGEU,
        OP_JAL,
        OP_JALR,

        // LSU
        OP_LB,
        OP_LH,
        OP_LW,
        OP_LBU,
        OP_LHU,
        OP_SB,
        OP_SH,
        OP_SW,
        // My custom LOAD/STORE vector instrs, still part of LSU
        OP_LV,
        OP_SV,

        // Mod Adder
        OP_VMADD,
        OP_VMSUB,

        // Mod Mull
        OP_VMMUL,

        // Vector ALU
        OP_VAND,
        OP_VXOR,
        OP_VOR,
        OP_VSLL,
        OP_VSRL,
        OP_VMV,
        OP_VSHFL,

        OP_MV_V_S, // Move chunk of VR to SR
        OP_MV_S_V, // Move SR to chunk of VR

        OP_NONE,
        OP_INVALID
    } op_tag_t;

    typedef enum logic [1:0] {
        IMM_U,   // imm = cimm << 12
        IMM_J_B, // imm = sign_extension cimm << 1
        IMM_I_S  // imm = sign_extension cimm
    } imm_fmt_t;

    typedef struct packed {
        imm_fmt_t fmt;
        logic [20-1:0] bits;
    } imm_t;

    typedef struct packed {
        logic [$bits(imm_t) - R_IDX_W - 1 - 1:0] _pad;
        logic [R_IDX_W-1:0] idx;
        logic is_v;
    } rs2_op_t;

    typedef union packed {
        imm_t as_imm;
        rs2_op_t as_r;
    } rs2_val_t;

    typedef struct packed {
        logic is_imm;
        rs2_val_t val;
    } rs2_t;

    typedef struct packed {
        logic en;
        logic [R_IDX_W-1:0] idx;
        logic is_v;
    } op_info_t;

    typedef struct packed {
        logic [TID_W-1:0] tid;
        logic [31:0]      pc;
        eu_tag_t eu_tag;
        op_tag_t op_tag;

        op_info_t rs1;
        rs2_t     rs2;
        op_info_t rd;
        logic     rd_is_rs;

        `ifdef DEBUG
        logic [31:0] instr;
        `endif
    } decode_out_t;

    typedef struct packed {
        logic               en;
        logic [  TID_W-1:0] tid;
        logic [R_IDX_W-1:0] rd;
    } wb_tag_t;

    typedef struct packed {
        logic [TID_W-1:0] tid;
        logic [31:0]      pc;
        eu_tag_t eu_tag;
        op_tag_t op_tag;

        op_info_t rs1;
        rs2_t     rs2;
        op_info_t rd;
        logic     rd_is_rs;

        logic read_stall;
        `ifdef DEBUG
        logic [31:0] instr;
        `endif
    } scoreboard_out_t;

    // REGFILE_STAGE

    typedef struct packed {
        wb_tag_t tag;
        logic [NUMBER_SIZE-1:0] data;
    } vwb_t;

    typedef struct packed {
        wb_tag_t tag;
        logic [W-1:0] data;
    } swb_t;

    typedef enum logic [1:0] {
        DMEM_B,
        DMEM_H,
        DMEM_W,
        DMEM_V
    } dmem_size_t;

    typedef struct packed {
        logic [          W-1:0] addr;
        logic                   send_data;
        dmem_size_t             size;
        logic [NUMBER_SIZE-1:0] data;
    } dmem_req_t;

    typedef struct packed {
        dmem_size_t             size;
        logic [NUMBER_SIZE-1:0] data;
    } dmem_rsp_t;

    typedef struct packed {
        logic [TID_W-1:0] tid;
        logic [31:0] pc;
        eu_tag_t eu_tag;
        op_tag_t op_tag;

        op_info_t rd;

        logic rs2_is_imm;
        imm_t imm;

        logic [NUMBER_SIZE-1:0] rs1;
        logic [NUMBER_SIZE-1:0] rs2;
        `ifdef DEBUG
        logic [31:0] instr;
        `endif
    } exec_in_t;

    typedef struct packed {
        cf_redirect_t cf_redirect_p;
        cf_pc_adv_t   cf_pc_adv_p;

        swb_t wbS;
        vwb_t wbA;
        vwb_t wbB;

        `ifdef DEBUG
        logic [31:0] instr;
        `endif
    } wb_out_t;

    localparam op_info_t OP_ZERO_REG = '{
        en: TRUE,
        idx: '0,
        is_v: FALSE
    };
    localparam rs2_t OP_RS2_ZERO_REG = '{
        is_imm: FALSE,
        val: rs2_val_t'(rs2_op_t'{
                _pad: '0,
                idx: '0,
                is_v: FALSE
            })
    };
    localparam logic [31:0] NOOP_INSTR = 32'h00000013;

endpackage : zbp_pkg
