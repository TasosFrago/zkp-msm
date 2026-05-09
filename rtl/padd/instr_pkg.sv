package instr_pkg;
    typedef enum logic [1:0] {
        OP_ADD,  // 00
        OP_SUB,  // 01
        OP_MUL,  // 10
        OP_NOOP  // 11
    } op_t;

    typedef struct packed {
        logic       is_init;  // 1 = Init Regfile, 0 = Normal Regfile
        logic [1:0] bank;
        logic [2:0] idx;      // 3 bits covers up to MAX_DEPTH = 8
    } operand_t;

    typedef enum logic [2:0] {
        VAL_NORM,
        VAL_X3,
        VAL_Y3,
        VAL_ZZ3,
        VAL_ZZZ3
    } val_t;

    typedef struct packed {
        logic wb_en;
        val_t val_type;
        logic [1:0] bank;
        logic [2:0] idx;
    } dest_op_t;

    typedef struct packed {
        op_t      op;
        operand_t src_a;
        operand_t src_b;
        dest_op_t dest;
    } instr_t;

    // ==========================================
    // INITIAL VALUES
    // ==========================================
    // Bank 0
    localparam operand_t X1 = '{is_init: 1'b1, bank: 2'd0, idx: 3'd0};
    localparam operand_t Y2 = '{is_init: 1'b1, bank: 2'd0, idx: 3'd1};
    localparam operand_t ZZ1 = '{is_init: 1'b1, bank: 2'd0, idx: 3'd2};

    // Bank 1
    localparam operand_t X2 = '{is_init: 1'b1, bank: 2'd0, idx: 3'd3};
    localparam operand_t Y1 = '{is_init: 1'b1, bank: 2'd0, idx: 3'd4};
    localparam operand_t ZZZ1 = '{is_init: 1'b1, bank: 2'd0, idx: 3'd5};

    // // ==========================================
    // // COMPUTED VALUES
    // // ==========================================
    // // Bank 0 (8 Slots)
    // localparam operand_t PP = '{is_init: 1'b0, bank: 2'd0, idx: 3'd0};
    // localparam operand_t Q = '{is_init: 1'b0, bank: 2'd0, idx: 3'd1};
    // localparam operand_t RR = '{is_init: 1'b0, bank: 2'd0, idx: 3'd2};
    // localparam operand_t S2 = '{is_init: 1'b0, bank: 2'd0, idx: 3'd3};
    // localparam operand_t U2 = '{is_init: 1'b0, bank: 2'd0, idx: 3'd4};
    // localparam operand_t Y1P = '{is_init: 1'b0, bank: 2'd0, idx: 3'd5};
    // localparam operand_t ZZ3 = '{is_init: 1'b0, bank: 2'd0, idx: 3'd6};
    // localparam operand_t ZZZ3 = '{is_init: 1'b0, bank: 2'd0, idx: 3'd7};
    //
    // // Bank 1 (5 Slots)
    // localparam operand_t P = '{is_init: 1'b0, bank: 2'd1, idx: 3'd0};
    // localparam operand_t R = '{is_init: 1'b0, bank: 2'd1, idx: 3'd1};
    // localparam operand_t X3 = '{is_init: 1'b0, bank: 2'd1, idx: 3'd2};
    // localparam operand_t X3t1 = '{is_init: 1'b0, bank: 2'd1, idx: 3'd3};
    // localparam operand_t Y3 = '{is_init: 1'b0, bank: 2'd1, idx: 3'd4};
    //
    // // Bank 2 (2 Slots)
    // localparam operand_t PPP = '{is_init: 1'b0, bank: 2'd2, idx: 3'd0};
    // localparam operand_t RT = '{is_init: 1'b0, bank: 2'd2, idx: 3'd1};
    //
    // // Bank 3 (2 Slots)
    // localparam operand_t Q2 = '{is_init: 1'b0, bank: 2'd3, idx: 3'd0};
    // localparam operand_t QsX3 = '{is_init: 1'b0, bank: 2'd3, idx: 3'd1};

    localparam operand_t X3 = '{is_init: 1'b0, bank: 2'd1, idx: 3'd0};
    localparam operand_t Y3 = '{is_init: 1'b0, bank: 2'd0, idx: 3'd0};
    localparam operand_t ZZ3 = '{is_init: 1'b0, bank: 2'd0, idx: 3'd3};
    localparam operand_t ZZZ3 = '{is_init: 1'b0, bank: 2'd0, idx: 3'd5};

    // ==========================================
    // INSTRUCTIONS ROM
    // ==========================================


    // localparam instr_t instr_rom[NUM_OPS] = '{
    //     '{OP_MUL, X2, ZZ1, U2},  // U2   = X2 * ZZ1
    //     '{OP_MUL, Y2, ZZZ1, S2},  // S2   = Y2 * ZZZ1
    //     '{OP_SUB, U2, X1, P},  // P    = U2 - X1
    //     '{OP_SUB, S2, Y1, R},  // R    = S2 - Y1
    //     '{OP_MUL, P, P, PP},  // PP   = P  * P
    //     '{OP_MUL, R, R, RR},  // RR   = R  * R
    //     '{OP_MUL, P, PP, PPP},  // PPP  = P  * PP
    //     '{OP_MUL, X1, PP, Q},  // Q    = X1 * PP
    //     '{OP_MUL, ZZ1, PP, ZZ3},  // ZZ3  = ZZ1 * PP
    //     '{OP_SUB, RR, PPP, X3t1},  // X3t1 = RR - PPP
    //     '{OP_MUL, Y1, PPP, Y1P},  // Y1P  = Y1 * PPP
    //     '{OP_MUL, ZZZ1, PPP, ZZZ3},  // ZZZ3 = ZZZ1 * PPP
    //     '{OP_ADD, Q, Q, Q2},  // Q2   = Q + Q
    //     '{OP_SUB, X3t1, Q2, X3},  // X3   = X3t1 - Q2
    //     '{OP_SUB, Q, X3, QsX3},  // QsX3 = Q - X3
    //     '{OP_MUL, R, QsX3, RT},  // RT   = R * QsX3
    //     '{OP_SUB, RT, Y1P, Y3}  // Y3   = RT - Y1P
    // };


    // #########################
    // #   PADD INSTRUCTIONS   #
    // #########################
    // U2  = X2 * ZZ1      | 1
    // S2  = Y2 * ZZZ1     | 1
    // P   = U2 - X1       | 2
    // R   = S2 - Y1       | 2
    // PP  = P  * P        | 3
    // RR  = R  * R        | 1
    // PPP = P  * PP       | 3
    // Q = X1 * PP         | 2
    // ZZ3 = ZZ1 * PP      | 0
    // X3t1 = RR - PPP     | 1
    // Y1P = Y1 * PPP      | 1
    // ZZZ3 = ZZZ1 * PPP   | 1
    // Q2 = Q + Q          | 1
    // X3 = X3t1 - Q2      | 1
    // QsX3 = Q - X3       | 1
    // RT = R * QsX3       | 1
    // Y3 = RT - Y1P       | 0

endpackage : instr_pkg
