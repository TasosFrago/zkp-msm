module lsu
    import zbp_pkg::*;
#(
    parameter int INTERMEDIATE_STORAGE = 2,
    parameter int PENDING_REQS = 4
) (
    input logic clk,
    input logic rst,

    input logic valid_in,
    input op_tag_t op_tag,
    input logic [TID_W-1:0] tid,

    input logic [W-1:0] rs1,
    input logic [NUMBER_SIZE-1:0] rs2,
    input logic [W-1:0] imm,
    input op_info_t rd,

    input logic wbS_fifo_full,

    // MMIO Interface
    pipeline_if.out mmio_req_if,
    pipeline_if.in  mmio_rsp_if,
    input logic     mmio_intercept,

    // Dmem Interface
    pipeline_if.out dmem_req_if,
    pipeline_if.in  dmem_rsp_if,

    input logic [4-1:0] bank_tracker,
    input logic [2-1:0] port_tracker,

    output logic mem_stall,
    output swb_t wbS_out,
    output vwb_t wbV_out,
    output logic vp_sel
);

    // ============ MEM Request Decoding ============
    dmem_req_t req_p;
    logic is_load, is_store;
    dmem_size_t data_size;

    always_comb begin
        is_load = FALSE;
        is_store = FALSE;
        data_size = DMEM_B;

        if (valid_in) begin
            case (op_tag)
                OP_LB, OP_LBU: begin
                    is_load = TRUE; data_size = DMEM_B;
                end
                OP_LH, OP_LHU: begin
                    is_load = TRUE; data_size = DMEM_H;
                end
                OP_LW: begin
                    is_load = TRUE; data_size = DMEM_W;
                end

                OP_SB: begin
                    is_store = TRUE; data_size = DMEM_B;
                end
                OP_SH: begin
                    is_store = TRUE; data_size = DMEM_H;
                end
                OP_SW: begin
                    is_store = TRUE; data_size = DMEM_W;
                end

                OP_LV: begin
                    is_load = TRUE; data_size = DMEM_V;
                end
                OP_SV: begin
                    is_store = TRUE; data_size = DMEM_V;
                end
                default: begin
                    is_load = FALSE;
                    is_store = FALSE;
                    data_size = DMEM_B;
                end
            endcase
        end
    end

    assign req_p.addr = rs1 + imm;
    assign req_p.send_data = is_store;
    assign req_p.data = rs2;
    assign req_p.size = data_size;


    logic skid_valid, skid_ready;
    logic fifo_valid, fifo_ready;

    assign skid_valid = mmio_intercept ? FALSE :
                        is_store ? TRUE :
                        is_load  ? fifo_ready : FALSE;

    assign fifo_valid = is_load & (mmio_intercept ?
        (mmio_req_if.ready & fifo_ready) :
        (skid_ready & fifo_ready));

    assign mem_stall  = mmio_intercept ?
        (is_store ? ~mmio_req_if.ready :
         is_load  ? ~(fifo_ready & mmio_req_if.ready) : FALSE) :
        (is_store ? ~skid_ready :
         is_load  ? ~(skid_ready & fifo_ready) : FALSE);

    assign mmio_req_if.valid = (is_load & fifo_ready) | is_store;
    assign mmio_req_if.data  = req_p;

    skid_buffer #(
        .DATA_W($bits(dmem_req_t))
    ) dmem_pipe_buff (
        .clk(clk),
        .rst(rst),

        .valid_in(skid_valid),
        .ready_in(skid_ready),
        .data_in (req_p),

        .valid_out(dmem_req_if.valid),
        .ready_out(dmem_req_if.ready),
        .data_out (dmem_req_if.data)
    );

    // =============== WB Tag tracking ===============
    wb_tag_t wb_tag;
    assign wb_tag = '{
        en: TRUE,
        tid: tid,
        rd: rd.idx
    };

    logic [4-1:0] sfifo_full, sfifo_empty;
    logic fifo_rd_valid, fifo_wb_pop;
    wb_tag_t fifo_wb_tag;
    logic fifo_wb_is_v, fifo_wb_is_mmio;

    logic      rsp_valid;
    dmem_rsp_t rsp_data;

    assign rsp_valid = fifo_rd_valid &
        (fifo_wb_is_mmio ? mmio_rsp_if.valid : dmem_rsp_if.valid);
    assign rsp_data = fifo_wb_is_mmio ? mmio_rsp_if.data : dmem_rsp_if.data;

    logic incoming_is_v;
    assign incoming_is_v = (rsp_data.size == DMEM_V);

    logic target_fifo_full;
    assign target_fifo_full = incoming_is_v & sfifo_full[fifo_wb_tag.rd[1:0]];

    logic push_to_sfifo;
    assign push_to_sfifo = rsp_valid & fifo_rd_valid & ~target_fifo_full;

    assign fifo_wb_pop = (incoming_is_v ? push_to_sfifo : (~wbS_fifo_full & rsp_valid & fifo_rd_valid));

    assign dmem_rsp_if.ready = fifo_wb_pop & ~fifo_wb_is_mmio;
    assign mmio_rsp_if.ready = fifo_wb_pop &  fifo_wb_is_mmio;

    fifo #(
        .DATA_W($bits(wb_tag_t) + 2),
        .DEPTH(PENDING_REQS)
    ) wb_tag_fifo (
        .clk(clk),
        .rst(rst),

        .wr_valid(fifo_valid),
        .wr_ready(fifo_ready),
        .wr_data({wb_tag, rd.is_v, mmio_intercept}),

        .rd_valid(fifo_rd_valid),
        .rd_ready(fifo_wb_pop),
        .rd_data({fifo_wb_tag, fifo_wb_is_v, fifo_wb_is_mmio})
    );


    typedef struct packed {
        wb_tag_t tag;
        logic [NUMBER_SIZE-1:0] data;
    } vwb_ret_t;

    logic [4-1:0] sfifo_rd_en;

    logic [NUMBER_SIZE-1:0] sfifo_d_out[4];
    wb_tag_t sfifo_tag_out[4];

    generate
    for (genvar i = 0; i < 4; i++) begin : gen_wb_fifos
        sync_fifo #(
            .DATA_W($bits(wb_tag_t) + NUMBER_SIZE),
            .DEPTH(INTERMEDIATE_STORAGE)
        ) f (
            .clk(clk),
            .rst(rst),

            .wdata_in({ fifo_wb_tag, rsp_data.data }),
            .wr_en(push_to_sfifo &
                   incoming_is_v &
                   (fifo_wb_tag.rd[1:0] == 2'(i))),
            .full(sfifo_full[i]),

            .rdata_out({ sfifo_tag_out[i], sfifo_d_out[i] }),
            .rd_en(sfifo_rd_en[i]),
            .empty(sfifo_empty[i])
        );
    end
    endgenerate


    // ============== Vector Port Selection ==============
    logic available_port;
    assign available_port = |(~port_tracker);

    always_comb begin
        casez (port_tracker)
            2'b?0:   vp_sel = 1'b0;
            2'b01:   vp_sel = 1'b1;
            default: vp_sel = 1'b0;
        endcase
    end

    // ============== Writeback Arbitration ==============
    always_comb begin
        wbS_out     = '{default: '0};
        wbV_out     = '{default: '0};
        sfifo_rd_en = '0;

        if (fifo_wb_pop & ~incoming_is_v) begin
            wbS_out = '{
                tag: '{
                    en: TRUE,
                    tid: fifo_wb_tag.tid,
                    rd: fifo_wb_tag.rd
                },
                data: rsp_data.data[31:0]
            };
        end

        if (~sfifo_empty[0] & (available_port & ~bank_tracker[0])) begin
            sfifo_rd_en[0] = TRUE;
            wbV_out = '{ tag:  sfifo_tag_out[0], data: sfifo_d_out[0] };
        end

        else if (~sfifo_empty[1] & (available_port & ~bank_tracker[1])) begin
            sfifo_rd_en[1] = TRUE;
            wbV_out = '{ tag:  sfifo_tag_out[1], data: sfifo_d_out[1] };
        end

        else if (~sfifo_empty[2] & (available_port & ~bank_tracker[2])) begin
            sfifo_rd_en[2] = TRUE;
            wbV_out = '{ tag:  sfifo_tag_out[2], data: sfifo_d_out[2] };
        end

        else if (~sfifo_empty[3] & (available_port & ~bank_tracker[3])) begin
            sfifo_rd_en[3] = TRUE;
            wbV_out = '{ tag:  sfifo_tag_out[3], data: sfifo_d_out[3] };
        end

    end


    // synthesis translate_off
    /*
    always_ff @(posedge clk) begin
        if (rst) begin end
        else if (fifo_valid & fifo_ready & (wb_tag.tid == 4 || wb_tag.tid == 5) & (wb_tag.rd == 13)) begin
            $info("Pushing to wb_tag_fifo { en: %d, tid: %0d, rd: %0d, is_v: %d, mmio_intercept: %d }",
                wb_tag.en, wb_tag.tid, wb_tag.rd, rd.is_v, mmio_intercept);
        end
    end

    always_ff @(posedge clk) begin
        if (rst) begin end
        else if (fifo_rd_valid & fifo_wb_pop & (fifo_wb_tag.tid == 4 || fifo_wb_tag.tid == 5) & (fifo_wb_tag.rd == 13)) begin
            $info("Pulling from wb_tag_fifo { en: %d, tid: %0d, rd: %0d, is_v: %d, mmio_intercept: %d }",
                fifo_wb_tag.en, fifo_wb_tag.tid, fifo_wb_tag.rd, fifo_wb_is_v, fifo_wb_is_mmio);
        end
    end
    */

    property no_dmem_req_when_mmio_intercepted;
        @(posedge clk) disable iff (rst)
        (skid_valid & mmio_intercept) |-> FALSE
    endproperty

    assert property (no_dmem_req_when_mmio_intercepted) else
    $error("LSU: skid_valid asserted while mmio_intercept is true, pushing false request to dmem");
    // synthesis translate_on

endmodule : lsu
