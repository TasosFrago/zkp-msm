#pragma clang diagnostic warning "-Wunused-variable"

#include <memory>
#include <print>
#include <queue>
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
	// std::string val(64, 'F');
	// bga::bgint<W> A("0x" + val);
	// bga::bgint<W> B("0x" + val);
	// bga::bgint<W> A("2");
	// bga::bgint<W> B("1");

	mda::Montgomery<W, mda::MONT_ALGO::FIOS> mont(MOD);
	auto mod_val = mont.n;

	std::println("n_prime {}", mont.n_p);

	// bga::bgint<W> testA("2");
	// bga::bgint<W> testB("3");
	// bga::bgint<W> testC("2");
	//
	// auto testAm = mont.init(testA);
	// auto testBm = mont.init(testB);
	// auto testCm = mont.init(testC);
	//
	// auto res = mont.mul_fios_test(testAm, testBm, testCm);
	// std::println("Test Res: {}", mont.trans_back(res));

	// auto A_m = mont.init(A);
	// auto B_m = mont.init(B);

	// auto gold_res = mont.mul(A_m, B_m);
	//
	// std::println("A: {}, A_m: {}", A, A_m);
	// std::println("B: {}, B_m: {}", B, B_m);
	// std::println("Expected res: \t{}", gold_res);
	// auto bits = std::format("{:b}", gold_res);
	// std::println("Res BITS {}", bits.size());

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

	constexpr int NUM_TESTS = 10000;
	constexpr int PIPELINE_STAGES = 3 * CHUNKS; // 3 * (CHUNKS - 1) + 3;

	std::queue<bga::bgint<W>> expected_queue;

	int pass_count = 0;
	int fail_count = 0;
	int unreduced_count = 0;

	for(int i = 0; i < NUM_TESTS; i++) {
		bga::bgint<W> A(gen());
		bga::bgint<W> B(gen());

		auto A_m = mont.init(A);
		auto B_m = mont.init(B);

		auto gold_res = mont.mul(A_m, B_m);
		expected_queue.push(gold_res);

		vtb::to_vlwide(A_m, dut->a);
		vtb::to_vlwide(B_m, dut->b);
		// std::println("Pushed A_m {:x}", A_m);
		// std::println("Pushed B_m {:x}", B_m);
		// std::println("Expecting  {:x}", gold_res);
		// std::println("");

		tick();

		if(expected_queue.size() == PIPELINE_STAGES) {
			bga::bgint<W> expected = expected_queue.front();
			expected_queue.pop();

			bga::bgint<W> hw_res_p = vtb::to_bgint<W>(dut->res_p);

			if(hw_res_p == expected) {
				pass_count++;
			} else if(bga::sub<W>(hw_res_p, mod_val) == expected) {
				pass_count++;
				unreduced_count++;
			} else {
				fail_count++;
				std::println("Mismatch at pipeline read!");
				std::println("Expected (Reduced):   {}", expected);
				std::println("Expected (Unreduced): {}", bga::add<W>(expected, mod_val));
				std::println("Got from Hardware:    {}", hw_res_p);
			}
		}
	}

	bga::bgint<W> zero("0");

	vtb::to_vlwide(zero, dut->a);
	vtb::to_vlwide(zero, dut->b);

	while(!expected_queue.empty()) {
		tick();

		auto expected = expected_queue.front();
		expected_queue.pop();

		auto hw_res_p = vtb::to_bgint<W>(dut->res_p);
		if(hw_res_p == expected) {
			pass_count++;
		} else if(bga::sub(hw_res_p, mod_val) == expected) {
			pass_count++;
			unreduced_count++;
		} else {
			fail_count++;
			std::println("Mismatch during pipeline flush!");
			std::println("Expected: {:x}", expected);
			std::println("Got:      {:x}", hw_res_p);
		}
	}

	std::println("\n--- Test Summary ---");
	std::println("Total Tests Run: {}", NUM_TESTS);
	std::println("Passed: {}", pass_count);
	std::println("Failed: {}", fail_count);
	std::println("--------------------");
	std::println("HW skipped final subtraction (Unreduced valid): {}", unreduced_count);
	std::println("HW performed final subtraction (Reduced valid): {}", pass_count - unreduced_count);

	if(fail_count == 0) {
		std::println("\nSUCCESS: All tests passed!");
	} else {
		std::println("\nERROR: Testbench failed");
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
