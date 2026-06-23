package regfile_pkg;

    localparam int V_L_REGS = 20;
    localparam int V_G_REGS = 3;
    localparam int V_TOTAL_REGS = V_L_REGS + V_G_REGS + 1;
    localparam int VR_IDX_W = $clog2(V_TOTAL_REGS);

    localparam int S_L_REGS = 27;
    localparam int S_G_REGS = 3;
    localparam int S_TOTAL_REGS = S_L_REGS + S_G_REGS + 2;
    localparam int SR_IDX_W = $clog2(S_TOTAL_REGS);

    localparam logic [VR_IDX_W-1:0] VZERO_REG = 0;
    localparam logic [VR_IDX_W-1:0] VG_REG_START = VZERO_REG + 1;
    localparam logic [VR_IDX_W-1:0] VG_REG_END = VG_REG_START + V_G_REGS - 1;
    localparam logic [VR_IDX_W-1:0] VL_REG_START = VG_REG_END + 1;
    localparam logic [VR_IDX_W-1:0] VL_REG_END = VL_REG_START + V_L_REGS - 1;

    localparam logic [VR_IDX_W-1:0] SZERO_REG = 0;
    localparam logic [VR_IDX_W-1:0] SG_REG_START = SZERO_REG + 1;
    localparam logic [VR_IDX_W-1:0] SG_REG_END = SG_REG_START + S_G_REGS - 1;
    localparam logic [VR_IDX_W-1:0] STID_REG = SG_REG_END + 1;
    localparam logic [VR_IDX_W-1:0] SL_REG_START = STID_REG + 1;
    localparam logic [VR_IDX_W-1:0] SL_REG_END = SL_REG_START + S_L_REGS - 1;

    localparam logic [VR_IDX_W-1:0] VZERO_REG = V_L_REGS;
    localparam logic [VR_IDX_W-1:0] VG_REG_START = V_L_REGS + 1;

    localparam logic [SR_IDX_W-1:0] SZERO_REG = S_G_REGS;
    localparam logic [SR_IDX_W-1:0] STID_REG = S_G_REGS + 1;
    localparam logic [SR_IDX_W-1:0] SL_REG_START = S_G_REGS + 2;

    // Execution unit tag
    typedef enum logic [2:0] {
        EU_ADD,
        EU_MUL
    } eu_tag_t;

endpackage : regfile_pkg
