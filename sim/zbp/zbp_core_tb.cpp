#include <print>
#include <verilated.h>
#include <verilated_fst_c.h>

#include "Vtb_top.h"


int main(int argc, char **argv)
{
	auto ctx = std::make_shared<VerilatedContext>();
	ctx->commandArgs(argc, argv);
	ctx->traceEverOn(true);

	auto dut = std::make_unique<Vtb_top>(ctx.get(), "dut");

	auto fst_deleter = [](VerilatedFstC *p) { if(p) { p->close(); delete p; }};
	std::unique_ptr<VerilatedFstC, decltype(fst_deleter)> tfp{ new VerilatedFstC, fst_deleter };

	uint64_t cycles = 0;

	dut->trace(tfp.get(), 99);
	tfp->open("dump.fst");

	auto tick = [&]() {
		dut->clk = 1;
		dut->eval();
		tfp->dump(ctx->time());
		ctx->timeInc(5000);
		dut->clk = 0;
		dut->eval();
		tfp->dump(ctx->time());
		ctx->timeInc(5000);
		cycles++;
	};

	std::println("Starting CPU Simulation...");

	dut->rst = 1;
	tick();
	tick();
	dut->rst = 0;

	constexpr int MAX_CYCLES = 100000;
	for(int cycle = 0; cycle < MAX_CYCLES; cycle++) {
		tick();
		if(dut->program_done) {
			std::println("PROGRAM DONE FLAG TRIGGERED!");
			break;
		}
		// if (ctx->gotFinish()) {
		// 	break;
		// }
	}

	std::println("Simulation Complete after {} clk cycles", cycles);

	tfp.reset();

	dut->final();
	return 0;
}
