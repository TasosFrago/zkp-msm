#pragma clang diagnostic warning "-Wunused-variable"

#include <memory>
#include <print>
#include <random>

#include <verilated.h>
#include <verilated_fst_c.h>

#include "Vmmul_top_tb.h"

#include "big_arth.hpp"
#include "bigint.hpp"
#include "mod_arth.hpp"
#include "tests/utils/helpers.hpp"

#include "utils.hpp"

#define STR(x) #x
#define XSTR(x) STR(x)

constexpr size_t NUMBER_SIZE = HW_NUMBER_SIZE;
constexpr size_t W = HW_W;
constexpr const char *MOD = "0x" XSTR(HW_MODULUS);

int main(int argc, char **argv)
{
	std::println("W: {}", W);
	std::println("CHUNKS: {}", NUMBER_SIZE / W);
	std::println("MOD: {}", MOD);

	constexpr unsigned int seed = 123;

	std::mt19937 rng(seed);

	auto gen = genRandBgN(42, rng);

	bga::bgint<W> A(gen());
	bga::bgint<W> B(gen());
	// bga::bgint<W> A("2");
	// bga::bgint<W> B("1");

	mda::Montgomery<W, mda::MONT_ALGO::FIOS> mont(MOD);
	std::println("n_prime {}", mont.n_p);

	auto A_m = mont.init(A);
	auto B_m = mont.init(B);

	auto gold_res = mont.mul(A_m, B_m);

	std::println("A: {}, A_m: {}", A, A_m);
	std::println("B: {}, B_m: {}", B, B_m);
	std::println("Expected res: \t{}", gold_res);
	auto bits = std::format("{:b}", gold_res);
	std::println("Res BITS {}", bits.size());

	Verilated::commandArgs(argc, argv);

	auto ctx = std::make_shared<VerilatedContext>();
	ctx->traceEverOn(true);

	auto dut = std::make_unique<Vmmul_top_tb>(ctx.get(), "dut");

	auto fst_deleter = [](VerilatedFstC *p) {
		if(p) {
			p->close();
			delete p;
		}
	};
	std::unique_ptr<VerilatedFstC, decltype(fst_deleter)> tfp{ new VerilatedFstC, fst_deleter };
	dut->trace(tfp.get(), 99);
	tfp->open("dump.fst");

	auto tick = [&]() {
		dut->clk = 0;
		dut->eval();
		tfp->dump(ctx->time());
		ctx->timeInc(1);

		dut->clk = 1;
		dut->eval();
		tfp->dump(ctx->time());
		ctx->timeInc(1);
	};

	dut->rst = 1;
	tick();
	tick();
	dut->rst = 0;
	tick();

	vtb::to_vlwide(A_m, dut->a);
	vtb::to_vlwide(B_m, dut->b);

	for(size_t i = 0; i < 10; i++) {
		tick();
	}

	std::println("");
	bga::bgint<W> hw_res = vtb::to_bgint<W>(dut->res);
	std::println("");

	auto hw_bits = std::format("{:b}", hw_res);
	std::println("Res BITS {}", hw_bits.size());

	std::println("hw_res: \t{}", hw_res);
	std::println("Hw_res: \t{:x}", hw_res);
	std::println("same: {}", (hw_res == gold_res));
	std::println("same: {}", (bga::sub<W>(hw_res, bga::bgint<W>(MOD)) == gold_res));

	auto gold_res_b = mont.trans_back(gold_res);
	auto hw_res_b = mont.trans_back(hw_res);
	auto hw_res_b2 = mont.trans_back(bga::sub<W>(hw_res, bga::bgint<W>(MOD)));
	std::println("gold_resb {}", gold_res_b);
	std::println("hw_resb {}", hw_res_b);
	std::println("same {}", (hw_res_b == gold_res_b));
	std::println("same {}", (hw_res_b2 == gold_res_b));

	return 0;
}
