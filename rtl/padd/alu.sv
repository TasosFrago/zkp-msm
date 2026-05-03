import instr_pkg::dest_op_t;
virtual class alu_metadata #(
    parameter int TID_BITS = 5
);
    typedef struct packed {
        logic [TID_BITS-1:0] tid;
        dest_op_t dest;
        logic vld_tag;
    } tag_t;
endclass : alu_metadata

module alu
    import instr_pkg::op_t;
    import instr_pkg::OP_ADD, instr_pkg::OP_SUB, instr_pkg::OP_MUL, instr_pkg::OP_NOOP;
    import instr_pkg::dest_op_t;
#(
    parameter int NUMBER_SIZE = 256,
    parameter int W = 32,
    parameter logic [NUMBER_SIZE-1:0] MODULUS = '0,
    parameter int THREAD_CNT = 6,

    localparam int CHUNKS = (NUMBER_SIZE / W),
    localparam int TID_BITS = $clog2(THREAD_CNT),
    localparam type tag_t = alu_metadata#(.TID_BITS(TID_BITS))::tag_t
) (
    input logic clk,
    input logic rst,

    input op_t op,

    input logic     [TID_BITS-1:0]        tid,   // tid that issues operation
    input dest_op_t                       dest,
    input logic     [  CHUNKS-1:0][W-1:0] in_a,
    input logic     [  CHUNKS-1:0][W-1:0] in_b,

    output logic [CHUNKS-1:0][W-1:0] add_out,
    output tag_t add_tag_out,
    output logic [CHUNKS-1:0][W-1:0] mul_out,
    output tag_t mul_tag_out
);
    localparam int MUL_LATENCY = 3 * CHUNKS;
    localparam int ADD_LATENCY = CHUNKS + 1;

    // ============== MUL SIDE ==============
    mmul #(
        .NUMBER_SIZE(NUMBER_SIZE),
        .W(W),
        .MODULUS(MODULUS),
        .CORE_TYPE("PIPE")
    ) multiplier (
        .clk(clk),
        .rst(rst),
        .a  ((op == OP_MUL) ? in_a : '0),
        .b  ((op == OP_MUL) ? in_b : '0),
        .res(mul_out)
    );

    tag_shiftreg #(
        .W(W),
        .DEPTH(MUL_LATENCY),
        .TID_BITS(TID_BITS)
    ) mul_tags_shiftreg (
        .clk(clk),
        .rst(rst),

        .tag_in ((op == OP_MUL) ? '{tid: tid, dest: dest, vld_tag: 1'b1} : '{default: '0}),
        .tag_out(mul_tag_out)
    );

    // ============== ADD SIDE ==============
    mod_add #(
        .W(W),
        .CHUNKS(CHUNKS),
        .MODULUS(MODULUS)
    ) adder (
        .clk(clk),
        .rst(rst),
        .op ((op == OP_ADD) ? 'b0 : 'b1),
        .a  ((op == OP_ADD || op == OP_SUB) ? in_a : '0),
        .b  ((op == OP_ADD || op == OP_SUB) ? in_b : '0),
        .res(add_out)
    );

    tag_shiftreg #(
        .W(W),
        .DEPTH(ADD_LATENCY),
        .TID_BITS(TID_BITS)
    ) add_tags_shiftreg (
        .clk(clk),
        .rst(rst),

        .tag_in((op == OP_ADD || op == OP_SUB) ?
        '{tid: tid, dest: dest, vld_tag: 1'b1}
        :
        '{default: '0}
        ),
        .tag_out(add_tag_out)
    );

endmodule : alu

module tag_shiftreg #(
    parameter int W = 32,
    parameter int DEPTH = 24,
    parameter int TID_BITS = 5,

    localparam type tag_t = alu_metadata#(.TID_BITS(TID_BITS))::tag_t
) (
    input logic clk,
    input logic rst,

    input  tag_t tag_in,
    output tag_t tag_out
);

    tag_t [DEPTH-1:0] shiftreg;

    always_ff @(posedge clk) begin
        if (rst) begin
            shiftreg <= '{default: '0};
        end
        else begin
            shiftreg[DEPTH-1:1] <= shiftreg[DEPTH-2:0];

            shiftreg[0] <= tag_in;
        end
    end

    assign tag_out = shiftreg[DEPTH-1];

endmodule : tag_shiftreg
