// ==================================================
//  Testbench Gantt Extraction (Exact Tagging)
// ==================================================
logic tb_vld_issue  /* verilator public */;
logic [31:0] tb_issue_tid  /* verilator public */;
logic [31:0] tb_issue_bank  /* verilator public */;
logic [31:0] tb_issue_idx  /* verilator public */;

logic tb_add_wb_vld  /* verilator public */;
logic [31:0] tb_add_wb_tid  /* verilator public */;
logic [31:0] tb_add_wb_bank  /* verilator public */;
logic [31:0] tb_add_wb_idx  /* verilator public */;

logic tb_mul_wb_vld  /* verilator public */;
logic [31:0] tb_mul_wb_tid  /* verilator public */;
logic [31:0] tb_mul_wb_bank  /* verilator public */;
logic [31:0] tb_mul_wb_idx  /* verilator public */;

logic tb_load_en  /* verilator public */;
logic [31:0] tb_load_tid  /* verilator public */;
logic [31:0] tb_load_side  /* verilator public */;
logic [31:0] tb_load_step  /* verilator public */;

assign tb_vld_issue = vld_issue;
assign tb_issue_tid = 32'(s3_reg.tid);
assign tb_issue_bank = 32'(s3_reg.instr.dest.bank);
assign tb_issue_idx = 32'(s3_reg.instr.dest.idx);

assign tb_add_wb_vld = add_tag_out.vld_tag;
assign tb_add_wb_tid = 32'(add_tag_out.tid);
assign tb_add_wb_bank = 32'(add_tag_out.dest.bank);
assign tb_add_wb_idx = 32'(add_tag_out.dest.idx);

assign tb_mul_wb_vld = mul_tag_out.vld_tag;
assign tb_mul_wb_tid = 32'(mul_tag_out.tid);
assign tb_mul_wb_bank = 32'(mul_tag_out.dest.bank);
assign tb_mul_wb_idx = 32'(mul_tag_out.dest.idx);

// Track the true background loader state
assign tb_load_en = vld_in & rdy_in & ~init_flush;
assign tb_load_tid = 32'(load_tid);
assign tb_load_side = 32'(load_side);
assign tb_load_step = 32'(load_step);
