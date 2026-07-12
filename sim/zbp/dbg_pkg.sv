// synthesis translate_off
package dbg_pkg;

    localparam int MAX_THREADS = 32;

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

        function new(virtual dbg_fetch_trace_if vif, int update_period);
            this.vif = vif;
            this.update_period = update_period;
            this.cycle_count = 0;
        endfunction : new

        task run();
            forever begin
                @(vif.cb);

                if (~vif.rst) begin
                    cycle_count++;

                    if (cycle_count % 64'(update_period) == 0) begin
                        $display("\n========================================");
                        $display("   [SV Monitor] PC Status @ Cycle %0d", cycle_count);
                        $display("========================================");
                        for (int t = 0; t < MAX_THREADS; t++) begin
                            $display("  Thread[%2d] PC: 0x%08X", t, vif.cb.pc_tb[t]);
                        end
                        $display("========================================\n");
                    end
                end
            end
        endtask

    endclass : PcHeartbeatMonitor

endpackage : dbg_pkg
// synthesis translate_on
