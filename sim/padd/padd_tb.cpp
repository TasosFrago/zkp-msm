#pragma clang diagnostic warning "-Wunused-variable"

#include <memory>
#include <print>
#include <queue>
#include <random>

#include <verilated.h>
#include <verilated_fst_c.h>

#include "Vpadd.h"
#include "Vpadd_padd.h"

#include "big_arth.hpp"
#include "bigint.hpp"
#include "ecc.hpp"
#include "mod_arth.hpp"
#include "tests/utils/helpers.hpp"
#include "utils.hpp"

#define STR(x) #x
#define XSTR(x) STR(x)

constexpr size_t NUMBER_SIZE = HW_NUMBER_SIZE;
constexpr size_t W = HW_W;
constexpr const char *MOD = "0x" XSTR(HW_MODULUS);

// Helper struct to hold test inputs
struct TestCase {
	bga::bgint<W> p_x, p_y, p_zz, p_zzz;
	bga::bgint<W> q_x, q_y;
};

int main(int argc, char **argv)
{
	constexpr int THREADS = Vpadd_padd::THREAD_CNT;

	std::println("W: {}", W);
	std::println("CHUNKS: {}", NUMBER_SIZE / W);
	std::println("MOD: {}", MOD);
	std::println("THREADS: {}", THREADS);

	std::mt19937 rng(42);
	auto gen = genRandBgN(77, rng);

	// Instantiate the curve for the software reference
	// Curve 'a' and 'b' parameters do not affect the XYZZ + Affine addition formula
	bga::bgint<W> zero("0");
	ecc::ShortWeierstrassCurve<W> curve(zero, zero, MOD);

	auto mod_val = curve.mont.n;

	Verilated::commandArgs(argc, argv);

	auto ctx = std::make_shared<VerilatedContext>();
	ctx->traceEverOn(true);

	ctx->timeunit(-9);
	ctx->timeprecision(-12);

	auto dut = std::make_unique<Vpadd>(ctx.get(), "dut");

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
		dut->clk = 1;
		dut->eval();
		tfp->dump(ctx->time());
		ctx->timeInc(5000);

		dut->clk = 0;
		dut->eval();
		tfp->dump(ctx->time());
		ctx->timeInc(5000);
	};

	// Reset Sequence
	tick();
	dut->rst = 1;
	dut->vld_in = 0;
	tick();
	tick();
	dut->rst = 0;
	tick();

	const int NUM_TESTS = 1000;
	std::queue<std::pair<TestCase, ecc::XYZZPoint<W>>> expected_queue;

	int test_count = 0;
	int pass_count = 0;
	int pass_count_back = 0;
	int fail_count = 0;

	int feed_step = 0;
	TestCase current_test;

	int read_step = 0;
	ecc::XYZZPoint<W> hw_result;

	std::println("\nStarting {} pipelined random XYZZ Point Add tests...", NUM_TESTS);

	std::unique_ptr<FILE, decltype(&std::fclose)> file_guard(nullptr, &std::fclose);
	FILE *f = std::fopen("output.log", "w");
	if(f) {
		file_guard.reset(f);
	}

	std::unique_ptr<FILE, decltype(&std::fclose)> errorf(nullptr, &std::fclose);
	FILE *f1 = std::fopen("errors.log", "w");
	if(f1) {
		errorf.reset(f1);
	}

	// Main Test Loop
	// Run until we've fed all tests AND the expected queue is completely empty (pipeline flushed)
	int timeout = 0;
	int cycle = 0;
	while(test_count < NUM_TESTS || !expected_queue.empty()) {
		// Evaluate comb logic to ensure dut->rdy_in is accurate for this cycle
		dut->eval();

		// --------------------------------------------------
		//  1. Input Stimulus State Machine (Feeding)
		// --------------------------------------------------
		if(test_count < NUM_TESTS && dut->rdy_in) {
			dut->vld_in = 1;

			if(feed_step == 0) {
				// Generate random inputs bounded by modulus
				current_test.p_x = curve.mont.init(bga::bgint<W>(gen()));
				if(current_test.p_x >= mod_val) std::println("WARNING: Val @ {} is bigger than MOD", cycle);
				current_test.p_y = curve.mont.init(bga::bgint<W>(gen()));
				if(current_test.p_y >= mod_val) std::println("WARNING: Val @ {} is bigger than MOD", cycle);
				current_test.p_zz = curve.mont.init(bga::bgint<W>(gen()));
				if(current_test.p_zz >= mod_val) std::println("WARNING: Val @ {} is bigger than MOD", cycle);
				current_test.p_zzz = curve.mont.init(bga::bgint<W>(gen()));
				if(current_test.p_zzz >= mod_val) std::println("WARNING: Val @ {} is bigger than MOD", cycle);
				current_test.q_x = curve.mont.init(bga::bgint<W>(gen()));
				if(current_test.q_x >= mod_val) std::println("WARNING: Val @ {} is bigger than MOD", cycle);
				current_test.q_y = curve.mont.init(bga::bgint<W>(gen()));
				if(current_test.q_y >= mod_val) std::println("WARNING: Val @ {} is bigger than MOD", cycle);

				// Generate Software Expected Result
				ecc::XYZZPoint<W> Pm(current_test.p_x, current_test.p_y, current_test.p_zz, current_test.p_zzz);
				ecc::AffinePoint<W> Qm(current_test.q_x, current_test.q_y, false);

				auto expected = curve.add_m(Pm, Qm);
				expected_queue.push({ current_test, expected });

				// Step 0 (Idx 0): Bank 0 = X1 (P.X), Bank 1 = X2 (Q.X)
				vtb::to_vlwide(current_test.p_x, dut->bus0_in);
				vtb::to_vlwide(current_test.q_x, dut->bus1_in);
			} else if(feed_step == 1) {
				// Step 1 (Idx 1): Bank 0 = Y2 (Q.Y), Bank 1 = Y1 (P.Y)
				vtb::to_vlwide(current_test.q_y, dut->bus0_in);
				vtb::to_vlwide(current_test.p_y, dut->bus1_in);
			} else if(feed_step == 2) {
				// Step 2 (Idx 2): Bank 0 = ZZ1 (P.ZZ), Bank 1 = ZZZ1 (P.ZZZ)
				vtb::to_vlwide(current_test.p_zz, dut->bus0_in);
				vtb::to_vlwide(current_test.p_zzz, dut->bus1_in);
			}

			feed_step++;
			if(feed_step == 3) {
				feed_step = 0;
				test_count++;
			}
		} else {
			dut->vld_in = 0;
		}

		// Advance time
		tick();

		// --------------------------------------------------
		//  2. Output Monitor State Machine (Reading)
		// --------------------------------------------------
		if(dut->valid_out) {
			bga::bgint<W> out_val = vtb::to_bgint<W>(dut->bus_out);

			if(read_step == 0)
				hw_result.X = out_val;
			else if(read_step == 1)
				hw_result.Y = out_val;
			else if(read_step == 2)
				hw_result.ZZ = out_val;
			else if(read_step == 3) {
				hw_result.ZZZ = out_val;

				if(expected_queue.empty()) {
					std::println("ERROR: Hardware produced valid_out, but test queue is empty!");
					fail_count++;
				} else {
					auto [producer, exp] = expected_queue.front();
					expected_queue.pop();

					auto hw_res_back = hw_result.demont(curve.mont);
					auto exp_back = exp.demont(curve.mont);

					// FILE LOG
					std::println(file_guard.get(), "\n--- Log at cycle {} (Time: {} ns)", cycle, (ctx->time() / 1000));
					std::println(file_guard.get(), "\tProd from X1: {0:x} | {0}", producer.p_x);
					std::println(file_guard.get(), "\tProd from Y1: {0:x} | {0}", producer.p_y);
					std::println(file_guard.get(), "\tProd from ZZ1: {0:x} | {0}", producer.p_zz);
					std::println(file_guard.get(), "\tProd from ZZZ1: {0:x} | {0}", producer.p_zzz);
					std::println(file_guard.get(), "\tProd from X2: {0:x} | {0}", producer.q_x);
					std::println(file_guard.get(), "\tProd from Y2: {0:x} | {0}", producer.q_y);

					std::println(file_guard.get(), "\tOut X: {0:x} | {0}", hw_result.X);
					std::println(file_guard.get(), "\tOut Y: {0:x} | {0}", hw_result.Y);
					std::println(file_guard.get(), "\tOut ZZ: {0:x} | {0}", hw_result.ZZ);
					std::println(file_guard.get(), "\tOut ZZZ: {0:x} | {0}", hw_result.ZZZ);

					auto res_match = (hw_result.X == exp.X && hw_result.Y == exp.Y &&
							  hw_result.ZZ == exp.ZZ && hw_result.ZZZ == exp.ZZZ);
					auto res_match_back = (hw_res_back.X == exp_back.X && hw_res_back.Y == exp_back.Y &&
							       hw_res_back.ZZ == exp_back.ZZ && hw_res_back.ZZZ == exp_back.ZZZ);

					if(res_match || res_match_back) {
						if(res_match) pass_count++;
						if(res_match_back) pass_count_back++;
					} else {
						fail_count++;
						int off_by_ones = 0;

						// FILE LOG
						std::println(errorf.get(), "\n--- Log at cycle {} (Time: {} ns)", cycle, (ctx->time() / 1000));
						std::println(errorf.get(), "\tProd from X1: {0:x} | {0}", producer.p_x);
						std::println(errorf.get(), "\tProd from Y1: {0:x} | {0}", producer.p_y);
						std::println(errorf.get(), "\tProd from ZZ1: {0:x} | {0}", producer.p_zz);
						std::println(errorf.get(), "\tProd from ZZZ1: {0:x} | {0}", producer.p_zzz);
						std::println(errorf.get(), "\tProd from X2: {0:x} | {0}", producer.q_x);
						std::println(errorf.get(), "\tProd from Y2: {0:x} | {0}", producer.q_y);
						std::println(errorf.get(), "");

						std::println(errorf.get(), "\tOut X: {0:x} | {0}", hw_result.X);
						std::println(errorf.get(), "\tOut Y: {0:x} | {0}", hw_result.Y);
						std::println(errorf.get(), "\tOut ZZ: {0:x} | {0}", hw_result.ZZ);
						std::println(errorf.get(), "\tOut ZZZ: {0:x} | {0}", hw_result.ZZZ);

						auto print_error = [&](const char *type, bga::bgint<W> exp, bga::bgint<W> hw_res,
								       bga::bgint<W> exp_b, bga::bgint<W> hw_res_b) {
							std::println("\tProd from X1: {0:x} | {0}", producer.p_x);
							std::println("\tProd from Y1: {0:x} | {0}", producer.p_y);
							std::println("\tProd from ZZ1: {0:x} | {0}", producer.p_zz);
							std::println("\tProd from ZZZ1: {0:x} | {0}", producer.p_zzz);
							std::println("\tProd from X2: {0:x} | {0}", producer.q_x);
							std::println("\tProd from Y2: {0:x} | {0}", producer.q_y);

							std::println("\tMontgomery Repr:");
							std::println("\tExpected {}:   {:x} \tMISS", type, exp);
							std::println("\tGot {}:        {:x} \tMISS", type, hw_res);

							auto diff = (exp_b > hw_res_b) ? bga::sub(exp_b, hw_res_b) : bga::sub(hw_res_b, exp_b);
							if(diff == 1) off_by_ones++;

							std::println("\tConverted back: diff: {} \t{}", diff, ((diff == 1) ? "OFF-BY-ONE" : ""));
							std::println("\tExpected {}:   {} \tMISS", type, exp_b);
							std::println("\tGot {}:        {} \tMISS", type, hw_res_b);
						};

						std::println("\n--- Pipeline Output Mismatch at cycle {} (Time: {} ns) ---", cycle, ctx->time() / 1000);
						if(exp_back.X != hw_res_back.X) {
							print_error("X", exp.X, hw_result.X, exp_back.X, hw_res_back.X);
							std::println("");
						}
						if(exp_back.Y != hw_res_back.Y) {
							print_error("Y", exp.Y, hw_result.Y, exp_back.Y, hw_res_back.Y);
							std::println("");
						}
						if(exp_back.ZZ != hw_res_back.ZZ) {
							print_error("ZZ", exp.ZZ, hw_result.ZZ, exp_back.ZZ, hw_res_back.ZZ);
							std::println("");
						}
						if(exp_back.ZZZ != hw_res_back.ZZZ) {
							print_error("ZZZ", exp.ZZZ, hw_result.ZZZ, exp_back.ZZZ, hw_res_back.ZZZ);
						}
						if(off_by_ones > 0) std::println("Off-by-ones: {}", off_by_ones);
					}
				}
			}

			read_step = (read_step + 1) % 4;
		}

		// std::println("Cycle {}", cycle);

		if(test_count == NUM_TESTS) {
			timeout++;
			if(timeout > 2 * (NUM_TESTS / THREADS) * 400) {
				std::println("ERROR: Timeout waiting in cycle {} for pipeline flush! Hardware might be deadlocked.", cycle);
				break;
			}
		}
		cycle++;
	}

	auto missed = NUM_TESTS - (pass_count_back + fail_count);

	// Final Reporting
	std::println("\n--- Test Summary ---");
	std::println("Batches: {}", (NUM_TESTS / THREADS));
	std::println("Total Tests Run: {}", NUM_TESTS);
	std::println("Passed: {}", (pass_count_back > pass_count) ? pass_count_back : pass_count);
	// std::println("Passed Back: {}", pass_count_back);
	std::println("Failed: {}", fail_count);
	std::println("Missed: {}", missed);
	std::println("--------------------");

	if(fail_count == 0 && missed == 0) {
		std::println("\nSUCCESS: All tests passed! Data safely streamed through the threaded ALU.");
	} else {
		std::println("\nERROR: Testbench failed due to output mismatches.");
	}

	return 0;
}
