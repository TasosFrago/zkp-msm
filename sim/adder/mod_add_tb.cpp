#pragma clang diagnostic warning "-Wunused-variable"

#include <memory>
#include <print>
#include <queue>
#include <random>

#include <verilated.h>
#include <verilated_fst_c.h>

// #include "Vmod_add.h"
#include "Vmod_add_var.h"

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
constexpr size_t CHUNKS = (NUMBER_SIZE / W);

int main(int argc, char **argv)
{
	std::println("W: {}", W);
	std::println("CHUNKS: {}", NUMBER_SIZE / W);
	std::println("MOD: {}", MOD);

	// constexpr unsigned int seed = 123;

	std::mt19937 rng;

	auto gen = genRandBgN(77, rng);

	// bga::bgint<W> A(gen());
	// bga::bgint<W> B(gen());
	std::string val(64, 'F');
	bga::bgint<W> A("0x" + val);
	bga::bgint<W> B("0x" + val);
	// bga::bgint<W> A("2");
	// bga::bgint<W> B("1");

	mda::Montgomery<W, mda::MONT_ALGO::FIOS> mont(MOD);
	auto mod_val = mont.n;

	Verilated::commandArgs(argc, argv);

	auto ctx = std::make_shared<VerilatedContext>();
	ctx->traceEverOn(true);

	// auto dut = std::make_unique<Vmod_add>(ctx.get(), "dut");
	auto dut = std::make_unique<Vmod_add_var>(ctx.get(), "dut");

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

	vtb::to_vlwide(mod_val, dut->modulus);

	const int NUM_TESTS = 10000;
	const int PIPELINE_STAGES = CHUNKS + 1;

	std::queue<std::pair<bga::bgint<W>, bool>> expected_queue;

	int pass_count = 0;
	int fail_count = 0;

	int add_high_count = 0;	   // Addition >= MOD
	int add_low_count = 0;	   // Addition < MOD
	int sub_no_wrap_count = 0; // Subtraction A >= B
	int sub_wrap_count = 0;	   // Subtraction A < B (Wrapped)

	std::println("\nStarting {} pipelined random tests (Add & Sub)...", NUM_TESTS);

	// Main Test Loop
	for(int i = 0; i < NUM_TESTS; i++) {
		bga::bgint<W> A(gen());
		bga::bgint<W> B(gen());

		auto A_m = mont.init(A);
		auto B_m = mont.init(B);

		bool do_sub = mda::mod(bga::bgint<W>(gen()), bga::bgint<W>(2)) != 0;
		bga::bgint<W> gold_res;

		if(do_sub) {
			gold_res = mda::sub(A_m, B_m, mod_val);
			if(A_m >= B_m) {
				sub_no_wrap_count++;
			} else {
				sub_wrap_count++;
			}
		} else {
			gold_res = mda::add(A_m, B_m, mod_val);
			if(gold_res < A_m) {
				add_high_count++;
			} else {
				add_low_count++;
			}
		}

		expected_queue.push({ gold_res, do_sub });

		dut->op = do_sub;
		vtb::to_vlwide(A_m, dut->a);
		vtb::to_vlwide(B_m, dut->b);

		tick();

		if(expected_queue.size() == PIPELINE_STAGES) {
			auto [expected, was_sub] = expected_queue.front();
			expected_queue.pop();

			bga::bgint<W> hw_res = vtb::to_bgint<W>(dut->res);

			if(hw_res == expected) {
				pass_count++;
			} else {
				fail_count++;
				std::println("Mismatch at pipeline read!");
				std::println("Op:       {}", was_sub ? "SUB" : "ADD");
				std::println("Expected: {:x}", expected);
				std::println("Got:      {:x}", hw_res);
			}
		}
	}

	bga::bgint<W> zero("0");
	dut->op = 0;
	vtb::to_vlwide(zero, dut->a);
	vtb::to_vlwide(zero, dut->b);

	int extra_ticks = (PIPELINE_STAGES - 1) - expected_queue.size();
	if(extra_ticks > 0) {
		for(int i = 0; i < extra_ticks; i++) {
			tick();
		}
	}

	while(!expected_queue.empty()) {
		tick();

		auto [expected, was_sub] = expected_queue.front();
		expected_queue.pop();
		bga::bgint<W> hw_res = vtb::to_bgint<W>(dut->res);

		if(hw_res == expected) {
			pass_count++;
		} else {
			fail_count++;
			std::println("Mismatch during pipeline flush!");
			std::println("Op:       {}", was_sub ? "SUB" : "ADD");
			std::println("Expected: {:x}", expected);
			std::println("Got:      {:x}", hw_res);
		}
	}

	// Final Reporting
	std::println("\n--- Test Summary ---");
	std::println("Total Tests Run: {}", NUM_TESTS);
	std::println("Passed: {}", pass_count);
	std::println("Failed: {}", fail_count);
	std::println("--------------------");
	std::println("Adds >= MODULUS:   {}", add_high_count);
	std::println("Adds < MODULUS:    {}", add_low_count);
	std::println("Subs (A >= B):     {}", sub_no_wrap_count);
	std::println("Subs (A < B Wrap): {}", sub_wrap_count);

	if(fail_count == 0) {
		std::println("\nSUCCESS: All tests passed! Module seamlessly pipelined adds and subs.");
	} else {
		std::println("\nERROR: Testbench failed due to output mismatches.");
	}

	// auto print_hw = [](const bga::bgint<W> &in, const char *name, const bga::bgint<W> &gold) {
	// 	std::println("");
	// 	std::println("{}: \t{}", name, in);
	// 	std::println("{}: \t{:x}", name, in);
	//
	// 	std::print("(gold == {}): {}", name, (gold == in));
	// 	if(auto cmp_sub = (bga::sub<W>(in, bga::bgint<W>(MOD)) == gold)) {
	// 		std::print("\t(gold == {}_sub): {}\n", name, cmp_sub);
	// 	}
	// 	std::println("");
	// };

	return 0;
}
