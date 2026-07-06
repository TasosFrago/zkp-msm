set ROOT_DIR [pwd]
set SIM_DIR $ROOT_DIR/sim/zbp/
set BUILD_DIR "$ROOT_DIR/build/sim/zbp_questa"
set RTL_DIR "$ROOT_DIR/rtl"
set FW_DIR "$ROOT_DIR/fw"
set FW_TARGET "$ROOT_DIR/build/fw/output.vh"
set TOP_MOD "zbp_tb"

# Compile Firmware
puts "$ROOT_DIR"
puts "\n\[INFO\] Compiling Firmware..."
catch {exec make -C $FW_DIR} fw_output
puts $fw_output


puts "\n\[INFO\] Setting up Build Directory: $BUILD_DIR"
file mkdir $BUILD_DIR

cd $BUILD_DIR


vlib work
vmap work work

puts "\n\[INFO\] Compiling SystemVerilog Sources..."

vlog -work work -sv "$RTL_DIR/zbp/zbp_pkg.sv"

set rtl_files [glob -nocomplain "$RTL_DIR/zbp/*.sv"]
foreach f $rtl_files {
    if {[file tail $f] ne "zbp_pkg.sv"} {
        vlog -work work -sv $f
    }
}

vlog -work work -sv "$SIM_DIR/tb_top.sv"
vlog -work work -sv "$SIM_DIR/zbp_tb.sv"


puts "\n\[INFO\] Starting Simulation..."

vsim -work work $TOP_MOD +MEM_FILE=$FW_TARGET

vcd file dump.vcd

add wave -r /*

run -all

vcd flush

quit
