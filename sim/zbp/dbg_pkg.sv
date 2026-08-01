// synthesis translate_off
package dbg_pkg;

    localparam int MAX_THREADS = 32;

    typedef struct {
        bit pc_heartbeat_en;
        bit fetch_monitor_en;
        bit dbg_probe_en;

        int heartbeat_period;
        int stuck_threashold;
    } dbg_config_t;

    function automatic dbg_config_t dbg_get_config();
        dbg_config_t cfg;

        cfg.pc_heartbeat_en = 1;
        cfg.fetch_monitor_en = 1;
        cfg.dbg_probe_en = 0;

        cfg.heartbeat_period = 100_000;
        cfg.stuck_threashold = 32;

        if ($test$plusargs("NO_HEARTBEAT"))   cfg.pc_heartbeat_en = 0;
        if ($test$plusargs("NO_FETCH_TRACE")) cfg.fetch_monitor_en = 0;
        if ($test$plusargs("DBG_PROBE"))      cfg.dbg_probe_en = 1;

        void'($value$plusargs("HEARTBEAT_PERIOD=%d", cfg.heartbeat_period));
        void'($value$plusargs("STUCK_THREASHOLD=%d", cfg.stuck_threashold));

        return cfg;
    endfunction : dbg_get_config

    function automatic void dump_sregs(input string filename, const ref logic [31:0] regs [1024]);
        int fd;
        int idx;
        static string abi_names[32] = '{
            "zero", "ra", "sp", "gp", "tp", "t0", "t1", "t2",
            "s0",   "s1", "a0", "a1", "a2", "a3", "a4", "a5",
            "a6",   "a7", "s2", "s3", "s4", "s5", "s6", "s7",
            "s8",   "s9", "s10","s11","t3", "t4", "t5", "t6"
        };

        fd = $fopen(filename, "w");

        if (fd != 0) begin
            for (int t = 0; t < 32; t++) begin
                $fdisplay(fd, "========================================");
                $fdisplay(fd, "             THREAD ID: %0d             ", t);
                $fdisplay(fd, "========================================");

                for (int i = 0; i < 32; i++) begin
                    idx = (32 * t) + i;

                    $fdisplay(fd, "%-4s (x%0d)\t= 0x%08h | %0d",
                              abi_names[i],
                              i,
                              ((i == 0) ? 0 : (i == 4) ? t : regs[idx]),
                              ((i == 0) ? 0 : (i == 4) ? t : regs[idx])
                    );
                end
                $fdisplay(fd, "\n");
            end
            $fclose(fd);
            $display("Dump saved to scalar_regs_dump.txt");
        end else begin
            $error("Failed to open dump file!");
        end
    endfunction : dump_sregs

    function automatic string fcommas(longint unsigned val);
        string str_in;
        string str_out = "";

        str_in = $sformatf("%0d", val);

        for (int i = 0; i < str_in.len(); i++) begin
            str_out = {str_out, str_in[i]};

            if ((str_in.len() - 1 - i) % 3 == 0 && i != (str_in.len() - 1)) begin
                str_out = {str_out, ","};
            end
        end

        return str_out;
    endfunction : fcommas

    import "DPI-C" context function string decode_riscv(int instr);

    class FetchMonitor;
        virtual dbg_fetch_trace_if vif;

        local int fd_trace[MAX_THREADS];
        local logic [31:0] in_flight_instr[MAX_THREADS];

        function new(virtual dbg_fetch_trace_if vif, string dir_path = ".");
            this.vif = vif;

            if (dir_path.len() > 0 && dir_path[dir_path.len()-1] != "/") begin
                dir_path = { dir_path, "/" };
            end

            $display("FetchMonitor: Opening trace files at dir %s", dir_path);
            for(int i = 0; i < MAX_THREADS; i++) begin
                string filename = $sformatf("%strace_tid_%0d.txt", dir_path, i);
                fd_trace[i] = $fopen(filename, "w");

                if (fd_trace[i] != 0) begin
                    $fdisplay(fd_trace[i], "    PC     |   INSTR  |  DISASSEMBLY ");
                    $fdisplay(fd_trace[i], "-------------------------------------");
                end
                else begin
                    $warning("FetchMonitor: Failed to open trace file for tid:%0d", i);
                end

                in_flight_instr[i] = '0;
            end
        endfunction : new

        task run();
            forever begin
                @(vif.cb);

                if (~vif.cb.rst) begin

                    if (vif.cb.fetch_valid && vif.cb.fetch_ready) begin
                        in_flight_instr[vif.cb.fetch_tid] = vif.cb.fetch_instr;
                    end

                    for(int t = 0; t < MAX_THREADS; t++) begin
                        /* verilator lint_off WIDTHEXPAND */
                        if (vif.cb.cf_redirect.vld && (vif.cb.cf_redirect.tid == t)) begin
                            $fdisplay(fd_trace[t], "0x%08X | 0x%08X  <--- [REDIRECT to 0x%08X] | %s  // @[%t]",
                                vif.cb.pc_tb[t], in_flight_instr[t], vif.cb.cf_redirect.pc,
                                decode_riscv(in_flight_instr[t]), $time);
                        end
                        else if (vif.cb.cf_pc_adv.vld && (vif.cb.cf_pc_adv.tid == t)) begin
                            $fdisplay(fd_trace[t], "0x%08X | 0x%08X  | %s  // @[%t]",
                                vif.cb.pc_tb[t], in_flight_instr[t],
                                decode_riscv(in_flight_instr[t]), $time);
                        end
                        /* verilator lint_on WIDTHEXPAND */
                    end
                end
            end
        endtask : run

        function void close_files();
            for(int i = 0; i < MAX_THREADS; i++) begin
                if (fd_trace[i] != 0) $fclose(fd_trace[i]);
            end
        endfunction : close_files

    endclass : FetchMonitor

    class PcHeartbeatMonitor;
        virtual dbg_fetch_trace_if vif;
        int update_period;
        longint cycle_count;

        event finish_ev;

        int stuck_threashold;
        int excluded_pc;

        local logic [31:0] last_pc[MAX_THREADS];
        local int stuck_cnt[MAX_THREADS];

        function new(virtual dbg_fetch_trace_if vif, int update_period,
                     event finish_ev, int stuck_threashold,
                     int excluded_pc = -1);
            this.vif = vif;
            this.update_period = update_period;
            this.cycle_count = 0;
            this.stuck_threashold = stuck_threashold;
            this.finish_ev = finish_ev;
            this.excluded_pc = excluded_pc;

            for(int i = 0; i < MAX_THREADS; i++) begin
                last_pc[i] = 32'hFFFF_FFFF;
                stuck_cnt[i] = 0;
            end
        endfunction : new

        local function void check_stuck();
            int tid;
            if (this.vif.cb.fetch_valid & this.vif.cb.fetch_ready) begin
                tid = int'(this.vif.cb.fetch_tid);

                if (this.vif.cb.pc_tb[tid] == last_pc[tid]) begin

                    if (this.vif.cb.pc_tb[tid] != this.excluded_pc) begin
                        this.stuck_cnt[tid]++;

                        if (this.stuck_cnt[tid] >= this.stuck_threashold) begin
                            $display("%s", {111{"="}});
                            $display("[PcHeartbeatMonitor] Thread %0d STUCK at PC 0x%08X",
                            tid, this.vif.cb.pc_tb[tid]);
                            $display(" No PC movement for %0d cycles (sim cycle %0d)",
                            stuck_cnt[tid], cycle_count);
                            $display("%s", {111{"="}});

                            $display("[PcHeartbeatMonitor]: Stuck counters at the moment of trigger.");
                            for(int i = 0; i < 10; i++) begin
                                int t1 = i;
                                int t2 = i + 10;
                                int t3 = i + 20;
                                int t4 = i + 30;

                                string col4 = (t4 < MAX_THREADS) ?
                                    $sformatf("Thread[%2d] Cnt: %4d", t4, stuck_cnt[t4]) :
                                    "";

                                $display("  Thread[%2d] Cnt: %4d  Thread[%2d] Cnt: %4d  Thread[%2d] Cnt: %4d  %s",
                                    t1, stuck_cnt[t1],
                                    t2, stuck_cnt[t2],
                                    t3, stuck_cnt[t3],
                                    col4);
                            end
                            ->this.finish_ev;
                            return;
                        end
                    end
                end
                else begin
                    last_pc[tid] = this.vif.cb.pc_tb[tid];
                    stuck_cnt[tid] = 0;
                end
            end
        endfunction : check_stuck

        task run();
            forever begin
                @(vif.cb);

                if (~vif.cb.rst) begin
                    cycle_count++;

                    check_stuck();

                    if (cycle_count % 64'(update_period) == 0) begin
                        $display("\n%s", {111{"="}});
                        $display("   [SV Monitor] PC Status @ Cycle %s", fcommas(cycle_count));
                        $display("%s", {111{"="}});

                        for (int i = 0; i < 10; i++) begin
                            int t1 = i;
                            int t2 = i + 10;
                            int t3 = i + 20;
                            int t4 = i + 30;

                            string col4 = (t4 < MAX_THREADS) ?
                                $sformatf("Thread[%2d] PC: 0x%08X", t4, vif.cb.pc_tb[t4]) :
                                "";

                            $display("  Thread[%2d] PC: 0x%08X   Thread[%2d] PC: 0x%08X   Thread[%2d] PC: 0x%08X   %s",
                                t1, vif.cb.pc_tb[t1], t2, vif.cb.pc_tb[t2],
                                t3, vif.cb.pc_tb[t3], col4);
                        end

                        $display("%s\n", {111{"="}});
                    end
                end
            end
        endtask

    endclass : PcHeartbeatMonitor

    import zbp_pkg::*;

    class DbgProbe;
        virtual dbg_core_probe_if vif;

        local int log_fd[MAX_THREADS];
        local longint cycles;


        typedef struct {
            logic active;
            string fetch;
            string fetch_to_dec;
            string dec_to_scb;
            string scb_to_rf;
            string rf_to_exu;
            string cf_redirect;
            string cf_pc_adv;
            string wb;
        } thread_log_t;

        local thread_log_t t_logs[MAX_THREADS];
        local int curr_pc[MAX_THREADS];

        function new(
            virtual dbg_core_probe_if vif,
            int cycles_start = 0,
            string dir_path = "."
        );
            this.vif = vif;
            this.cycles = longint'(cycles_start);

            if (dir_path.len() > 0 && dir_path[dir_path.len()-1] != "/") begin
                dir_path = { dir_path, "/" };
            end

            $display("DbgProbe: Opening Log files for debug probe at dir %s", dir_path);

            for(int i = 0; i < MAX_THREADS; i++) begin
                string filename = $sformatf("%sdbg_log_tid_%0d.txt", dir_path, i);
                log_fd[i] = $fopen(filename, "w");

                if (log_fd[i] != 0) begin
                    $fdisplay(log_fd[i], "[%t] Starting Simulation at cycle %0d",
                        $time, this.cycles);
                end
                else begin
                    $warning("DbgProbe: Failed to open the dbg log file for tid: %0d", i);
                end

                t_logs[i].active = 0;
                t_logs[i].fetch = "";
                t_logs[i].fetch_to_dec = "";
                t_logs[i].dec_to_scb = "";
                t_logs[i].scb_to_rf = "";
                t_logs[i].rf_to_exu = "";
                t_logs[i].cf_redirect = "";
                t_logs[i].cf_pc_adv = "";
                t_logs[i].wb = "";

                curr_pc[i] = 0;
            end
        endfunction : new

        function void close_files();
            for(int i = 0; i < MAX_THREADS; i++) begin
                if (log_fd[i] != 0) $fclose(log_fd[i]);
            end
        endfunction : close_files

        task run();
            forever begin
                @(vif.cb);

                if (vif.cb.rst) begin
                    this.cycles = this.cycles + 1;
                    continue;
                end

                process_imem_rsp();
                process_fetch_to_dec();
                process_dec_to_scb();
                process_scb_to_rf();
                process_rf_to_exu();
                process_cf();
                process_wb();

                flush_msgs();

                this.cycles = this.cycles + 1;
            end
        endtask : run

        local function void process_imem_rsp();
            imem_rsp_t imem_rsp = this.vif.cb.imem_rsp_data;

            if (!this.vif.cb.imem_rsp_fire) return;

            curr_pc[imem_rsp.tid] = this.vif.cb.imem_rsp_pc;

            t_logs[imem_rsp.tid].active = 1;

            t_logs[imem_rsp.tid].fetch = $sformatf(
                "FETCH: IMEM responed with instruction: %s",
                decode_riscv(imem_rsp.instr)
            );
        endfunction : process_imem_rsp

        local function void process_cf();
            cf_redirect_t cf_redirect = this.vif.cb.exu_to_wb_data.cf_redirect_p;
            cf_pc_adv_t cf_pc_adv = this.vif.cb.exu_to_wb_data.cf_pc_adv_p;

            if (cf_redirect.vld) begin
                t_logs[cf_redirect.tid].active = 1;
                t_logs[cf_redirect.tid].cf_redirect = $sformatf(
                    "CF: REDIRECT from PC: 0x%05X -> 0x%05x\n",
                    curr_pc[cf_redirect.tid], cf_redirect.pc
                );
            end

            if (cf_pc_adv.vld) begin
                t_logs[cf_pc_adv.tid].active = 1;
                t_logs[cf_pc_adv.tid].cf_pc_adv = $sformatf(
                    "CF: Normal PC Advancement to 0x%05X\n",
                    curr_pc[cf_pc_adv.tid] + 4
                );
            end

        endfunction : process_cf

        local function void process_fetch_to_dec();
            fetch_out_t fetch_to_dec = this.vif.cb.fetch_to_dec_data;

            if (!this.vif.cb.fetch_to_dec_fire) begin
                t_logs[fetch_to_dec.tid].active = 1;
                t_logs[fetch_to_dec.tid].fetch_to_dec = $sformatf(
                    "FETCH->DECODE:  %s, %s\n",
                    (this.vif.cb.fetch_to_dec_vld ? "[VALID]" : "[NOT VALID]"),
                    (this.vif.cb.fetch_to_dec_rdy ? "[READY]" : "[NOT READY]")
                );
                return;
            end

            t_logs[fetch_to_dec.tid].active = 1;

            t_logs[fetch_to_dec.tid].fetch_to_dec = $sformatf(
                "FETCH->DECODE: Incoming instr PC: [0x%08X] %s\n",
                fetch_to_dec.pc, decode_riscv(fetch_to_dec.instr)
            );
        endfunction : process_fetch_to_dec

        local function void process_dec_to_scb();
            decode_out_t dec_to_scb = this.vif.cb.dec_to_scb_data;
            logic scb_operand1_rdy = this.vif.cb.scb_operand1_rdy;
            logic scb_operand2_rdy = this.vif.cb.scb_operand2_rdy;
            logic scb_read_stall = this.vif.cb.scb_read_stall;
            logic scb_bshfl_stall = this.vif.cb.scb_bshfl_stall;
            logic scb_can_issue = this.vif.cb.scb_can_issue;
            logic scb_buff_rdy = this.vif.cb.scb_buff_rdy;

            string rs1 = dec_to_scb.rs1.en ? $sformatf(
                    "             RS1: idx( %s%0d ), %s\n",
                    (dec_to_scb.rs1.is_bn ? "b" : "x"),
                    dec_to_scb.rs1.idx,
                    (scb_operand1_rdy ? "READY" : "NOT READY")
                ) : "";

            string rs2 =
                (dec_to_scb.rd_is_rs && dec_to_scb.rd.en) ? $sformatf(
                    "             RS2: idx( %s%0d ), %s\n",
                    (dec_to_scb.rd.is_bn ? "b" : "x"),
                    dec_to_scb.rd.idx,
                    (scb_operand2_rdy ? "READY" : "NOT READY")
                ) :
                !dec_to_scb.rs2.is_imm ? $sformatf(
                    "             RS2: idx( %s%0d ), %s\n",
                    (dec_to_scb.rs2.val.as_r.is_bn ? "b" : "x"),
                    dec_to_scb.rs2.val.as_r.idx,
                    (scb_operand2_rdy ? "READY" : "NOT READY")
                ) :
                $sformatf(
                    "             IMM: %s, 0x%0X\n",
                    dec_to_scb.rs2.val.as_imm.fmt.name(),
                    dec_to_scb.rs2.val.as_imm.bits,
                );

            string rd = (!dec_to_scb.rd_is_rs && dec_to_scb.rd.en) ? $sformatf(
                    "             RD: idx( %s%0d )\n",
                    (dec_to_scb.rd.is_bn ? "b" : "x"),
                    dec_to_scb.rd.idx
                ) : "";

            string read_stall = (scb_read_stall ?
                "             READ_STALL DETECTED\n" : "");

            string bshfl_stall = (scb_bshfl_stall ?
                "             BSHFL_STALL DETECTED\n" : "");

            string can_issue = (scb_can_issue ?
                "             INSTR ISSUES\n" :
                "             DID NOT ISSUE\n");

            string rejected_cant_issue = !scb_can_issue ?
                "CANT ISSUE" : "";
            string rejected_buff_nrdy = !scb_buff_rdy ?
                "BUFF NOT READY" : "";

            string rejected = !(scb_can_issue & scb_buff_rdy) ?
                $sformatf("!!!INSTRUCTION REJECTED. %s %s!!!\n",
                rejected_cant_issue, rejected_buff_nrdy) : "";

            if (!this.vif.cb.dec_to_scb_fire) begin
                t_logs[dec_to_scb.tid].active = 1;
                t_logs[dec_to_scb.tid].dec_to_scb = $sformatf(
                    "DECODE->SCB:  %s, %s\n",
                    (this.vif.cb.dec_to_scb_vld ? "[VALID]" : "[NOT VALID]"),
                    (this.vif.cb.dec_to_scb_rdy ? "[READY]" : "[NOT READY]")
                );
                return;
            end

            t_logs[dec_to_scb.tid].active = 1;

            t_logs[dec_to_scb.tid].dec_to_scb = $sformatf({
                "DECODE->SCB: PC: 0x%05X, (%s)\n",
                "             EU: %s, OP: %s\n",
                "%s", "%s", "%s", "%s", "%s", "%s", "%s"
            },
                dec_to_scb.pc, decode_riscv(dec_to_scb.instr),
                dec_to_scb.eu_tag.name(), dec_to_scb.op_tag.name(),
                rs1, rs2, rd, read_stall, bshfl_stall, can_issue, rejected
            );
        endfunction : process_dec_to_scb

        local function void process_scb_to_rf();
            scoreboard_out_t scb_to_rf = vif.cb.scb_to_rf_data;

            string rs1 = scb_to_rf.rs1.en ? $sformatf(
                    "         RS1: idx( %s%0d ),  ",
                    (scb_to_rf.rs1.is_bn ? "b" : "x"),
                    scb_to_rf.rs1.idx
                ) : "";

            string rs2 =
                (scb_to_rf.rd_is_rs && scb_to_rf.rd.en) ? $sformatf(
                    "         RS2: idx( %s%0d ),  ",
                    (scb_to_rf.rd.is_bn ? "b" : "x"),
                    scb_to_rf.rd.idx
                ) :
                !scb_to_rf.rs2.is_imm ? $sformatf(
                    "         RS2: idx( %s%0d ),  ",
                    (scb_to_rf.rs2.val.as_r.is_bn ? "b" : "x"),
                    scb_to_rf.rs2.val.as_r.idx
                ) :
                $sformatf(
                    "         IMM: %s, 0x%0X,  ",
                    scb_to_rf.rs2.val.as_imm.fmt.name(),
                    scb_to_rf.rs2.val.as_imm.bits,
                );

            string rd = (!scb_to_rf.rd_is_rs && scb_to_rf.rd.en) ? $sformatf(
                    "         RD: idx( %s%0d )\n",
                    (scb_to_rf.rd.is_bn ? "b" : "x"),
                    scb_to_rf.rd.idx
                ) : "";

            if (!this.vif.cb.scb_to_rf_fire) begin
                t_logs[scb_to_rf.tid].active = 1;
                t_logs[scb_to_rf.tid].scb_to_rf = $sformatf(
                    "SCB->RF:  %s, %s\n",
                    (this.vif.cb.scb_to_rf_vld ? "[VALID]" : "[NOT VALID]"),
                    (this.vif.cb.scb_to_rf_rdy ? "[READY]" : "[NOT READY]"),
                );
                return;
            end

            t_logs[scb_to_rf.tid].active = 1;

            t_logs[scb_to_rf.tid].scb_to_rf = $sformatf({
                "SCB->RF: PC: 0x%05X, (%s)\n",
                "         EU: %s, OP: %s\n",
                "%s", "%s", "%s"
            },
                scb_to_rf.pc, decode_riscv(scb_to_rf.instr),
                scb_to_rf.eu_tag.name(), scb_to_rf.op_tag.name(),
                rs1, rs2, rd
            );
        endfunction : process_scb_to_rf

        local function void process_rf_to_exu();
            exec_in_t rf_to_exu = this.vif.cb.rf_to_exu_data;
            logic [31:0] wb_bank_tracker[4] = this.vif.cb.ex_wb_bank_tracker;
            logic [31:0] wb_port_tracker[2] = this.vif.cb.ex_wb_port_tracker;
            logic [4:0] exec_lat = this.vif.cb.ex_exec_lat;

            string rd = (rf_to_exu.rd.en) ? $sformatf(
                    "           RD: idx( %s%0d )\n",
                    (rf_to_exu.rd.is_bn ? "b" : "x"),
                    rf_to_exu.rd.idx
                ) : "";

            string write_bank_collision = wb_bank_tracker[rf_to_exu.rd.idx[1:0]][exec_lat] ?
                $sformatf("Bank collides on pos %0d",
                exec_lat) : "";

            string write_port_collision = (wb_port_tracker[0][exec_lat] &
                wb_port_tracker[1][exec_lat]) ?
                    $sformatf("Port collides on pos %0d, Ports: [0]: %0d, [1]: %0d",
                    exec_lat, wb_port_tracker[0][exec_lat], wb_port_tracker[1][exec_lat]) : "";

            string write_collision = this.vif.cb.ex_bn_write_collision ?
                    $sformatf("           WRITE COLLISION %s,  %s\n",
                    write_bank_collision, write_port_collision) :
                (this.vif.cb.rf_to_exu_vld && this.vif.cb.rf_to_exu_rdy &&
                (exec_lat > 0)) ?
                    $sformatf({
                        "           Writing to BankTR[%0d] @ %0d, state:[%b]\n",
                        "           Writing to PortTR[%0d] @ %0d, state:[%b]\n"
                        },
                        rf_to_exu.rd.idx[1:0], exec_lat,
                        wb_bank_tracker[rf_to_exu.rd.idx[1:0]],
                        (~wb_port_tracker[0][exec_lat] ? 0 : 1), exec_lat,
                        (~wb_port_tracker[0][exec_lat] ?
                            wb_port_tracker[0] :
                            wb_port_tracker[1])
                    ) : "";

            if(!this.vif.cb.rf_to_exu_fire) begin
                t_logs[rf_to_exu.tid].active = 1;
                t_logs[rf_to_exu.tid].rf_to_exu = $sformatf(
                    "RF->EXU:  %s, %s\n",
                    (this.vif.cb.rf_to_exu_vld ? "[VALID]" : "[NOT VALID]"),
                    (this.vif.cb.rf_to_exu_rdy ? "[READY]" : "[NOT READY]")
                );
                return;
            end

            t_logs[rf_to_exu.tid].active = 1;

            t_logs[rf_to_exu.tid].rf_to_exu = $sformatf({
                "RF->EXU: PC: 0x%05X, (%s)\n",
                "         EU: %s, OP: %s\n",
                "         RS1: 0x%0X,  RS2: 0x%0X\n",
                "%s", "%s"
            },
                rf_to_exu.pc, decode_riscv(rf_to_exu.instr),
                rf_to_exu.eu_tag.name(), rf_to_exu.op_tag.name(),
                rf_to_exu.rs1, rf_to_exu.rs2, rd,
                write_collision
            );

        endfunction : process_rf_to_exu

        local function void process_wb();
            wb_out_t wb = this.vif.cb.exu_to_wb_data;

            if (wb.wbS.tag.en) begin
                t_logs[wb.wbS.tag.tid].active = 1;
                t_logs[wb.wbS.tag.tid].wb = {
                    t_logs[wb.wbS.tag.tid].wb,
                    $sformatf(
                    "Writeback to scalar Port: TID[%0d], (x%0d), d: 0x%0X\n",
                    wb.wbS.tag.tid, wb.wbS.tag.rd, wb.wbS.data
                    )};
            end

            if (wb.wbA.tag.en) begin
                t_logs[wb.wbA.tag.tid].active = 1;
                t_logs[wb.wbA.tag.tid].wb = {
                    t_logs[wb.wbA.tag.tid].wb,
                    $sformatf(
                        "Writeback to BN Port A: TID[%0d], (b%0d), d: 0x%0X\n",
                        wb.wbA.tag.tid, wb.wbA.tag.rd, wb.wbA.data
                    )};
            end

            if (wb.wbB.tag.en) begin
                t_logs[wb.wbB.tag.tid].active = 1;
                t_logs[wb.wbB.tag.tid].wb = {
                    t_logs[wb.wbB.tag.tid].wb,
                    $sformatf(
                        "Writeback to BN Port B: TID[%0d], (b%0d), d: 0x%0X\n",
                        wb.wbB.tag.tid, wb.wbB.tag.rd, wb.wbB.data
                    )};
            end

        endfunction : process_wb

        local function void flush_msgs();
            for(int i = 0; i < MAX_THREADS; i++) begin
                if (t_logs[i].active == 1) begin

                    $fdisplay(log_fd[i], "[CYCLE]: %s, @ [%tps]", fcommas(this.cycles), $time);

                    if (t_logs[i].fetch.len() > 0)         $fwrite(log_fd[i], " %s", t_logs[i].fetch);
                    if (t_logs[i].fetch_to_dec.len() > 0)  $fwrite(log_fd[i], " %s", t_logs[i].fetch_to_dec);
                    if (t_logs[i].dec_to_scb.len() > 0)    $fwrite(log_fd[i], " %s", t_logs[i].dec_to_scb);
                    if (t_logs[i].scb_to_rf.len() > 0)     $fwrite(log_fd[i], " %s", t_logs[i].scb_to_rf);
                    if (t_logs[i].rf_to_exu.len() > 0)     $fwrite(log_fd[i], " %s", t_logs[i].rf_to_exu);
                    if (t_logs[i].cf_redirect.len() > 0)   $fwrite(log_fd[i], " %s", t_logs[i].cf_redirect);
                    else if(t_logs[i].cf_pc_adv.len() > 0) $fwrite(log_fd[i], " %s", t_logs[i].cf_pc_adv);
                    if (t_logs[i].wb.len() > 0)            $fwrite(log_fd[i], " %s", t_logs[i].wb);

                    $fdisplay(log_fd[i], "");

                    t_logs[i].active = 0;
                    t_logs[i].fetch = "";
                    t_logs[i].fetch_to_dec = "";
                    t_logs[i].dec_to_scb = "";
                    t_logs[i].scb_to_rf = "";
                    t_logs[i].rf_to_exu = "";
                    t_logs[i].cf_redirect = "";
                    t_logs[i].cf_pc_adv = "";
                    t_logs[i].wb = "";
                end
            end
        endfunction : flush_msgs

    endclass : DbgProbe

endpackage : dbg_pkg
// synthesis translate_on
