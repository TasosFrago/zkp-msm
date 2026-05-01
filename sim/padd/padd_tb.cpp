#pragma clang diagnostic warning "-Wunused-variable"

#include <memory>
#include <print>
#include <queue>
#include <random>

#include <map>
#include <string>
#include <tuple>

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

	constexpr int BATCHES = 3;
	std::println("BATCHES: {}", BATCHES);
	constexpr int NUM_TESTS = BATCHES * THREADS;
	// constexpr int NUM_TESTS = 27 + 5;

	std::mt19937 rng(42);
	auto gen = genRandBgN(77, rng);

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

	int cycle = 0;
	auto tick = [&]() {
		dut->clk = 1;
		dut->eval();
		tfp->dump(ctx->time());
		ctx->timeInc(5000);

		dut->clk = 0;
		dut->eval();
		tfp->dump(ctx->time());
		ctx->timeInc(5000);
		cycle++;
	};

	std::unique_ptr<FILE, decltype(&std::fclose)> csv_guard(nullptr, &std::fclose);
	FILE *csv_f = std::fopen("schedule.csv", "w");
	if(csv_f) {
		csv_guard.reset(csv_f);
		std::println(csv_guard.get(), "start_cycle,duration,op_type,produces,threadID");
	}

	struct OpType {
		const char *name;
		char type;
	};
	const OpType OP_SEQ[] = {
		{ "U2", 'M' }, { "S2", 'M' }, { "P", 'A' }, { "R", 'A' }, { "PP", 'M' }, { "R2", 'M' }, { "PPP", 'M' }, { "Q", 'M' }, { "ZZ3", 'M' }, { "X3_sub1", 'A' }, { "Y1P", 'M' }, { "ZZZ3", 'M' }, { "2Q", 'A' }, { "X3", 'A' }, { "QsubX3", 'A' }, { "RT", 'M' }, { "Y3", 'A' }
	};
	struct InFlightOp {
		int start_c;
		std::string name;
		char type;
	};

	std::map<std::tuple<int, int, int>, InFlightOp> inflight_ops;
	int thread_op_idx[Vpadd_padd::THREAD_CNT] = { 0 };

	// Reset Sequence
	tick();
	dut->rst = 1;
	dut->vld_in = 0;
	tick();
	tick();
	dut->rst = 0;
	tick();

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

	int timeout = 0;
	int print_timer = 0;
	while(test_count < NUM_TESTS || !expected_queue.empty()) {
		dut->eval();

		if(dut->padd->tb_vld_issue) {
			int tid = dut->padd->tb_issue_tid;
			int bank = dut->padd->tb_issue_bank;
			int idx = dut->padd->tb_issue_idx;
			int op_idx = thread_op_idx[tid];

			if(op_idx < 17) {
				const auto &op = OP_SEQ[op_idx];
				inflight_ops[{ tid, bank, idx }] = { cycle, op.name, op.type };
			}
			thread_op_idx[tid]++;
			if(thread_op_idx[tid] >= 17) thread_op_idx[tid] = 0;
		}
		if(dut->padd->tb_add_wb_vld && csv_guard) {
			auto key = std::make_tuple((int)dut->padd->tb_add_wb_tid,
						   (int)dut->padd->tb_add_wb_bank,
						   (int)dut->padd->tb_add_wb_idx);

			if(inflight_ops.contains(key)) {
				auto op = inflight_ops[key];
				int duration = cycle - op.start_c;
				std::println(csv_guard.get(), "{},{},A,{},{}", op.start_c, duration, op.name, dut->padd->tb_add_wb_tid);
				inflight_ops.erase(key);
			}
		}
		if(dut->padd->tb_mul_wb_vld && csv_guard) {
			auto key = std::make_tuple((int)dut->padd->tb_mul_wb_tid,
						   (int)dut->padd->tb_mul_wb_bank,
						   (int)dut->padd->tb_mul_wb_idx);

			if(inflight_ops.contains(key)) {
				auto op = inflight_ops[key];
				int duration = cycle - op.start_c;
				std::println(csv_guard.get(), "{},{},M,{},{}", op.start_c, duration, op.name, dut->padd->tb_mul_wb_tid);
				inflight_ops.erase(key);
			}
		}
		if(dut->padd->tb_load_en && csv_guard) {
			int tid = dut->padd->tb_load_tid;
			int side = dut->padd->tb_load_side;
			int step = dut->padd->tb_load_step;

			const char *produces = "";
			if(step == 0)
				produces = (side == 0) ? "0: X1-X2" : "1: X1-X2";
			else if(step == 1)
				produces = (side == 0) ? "0: Y2-Y1" : "1: Y2-Y1";
			else if(step == 2)
				produces = (side == 0) ? "0: ZZ1-ZZZ1" : "1: ZZ1-ZZZ1";

			std::println(csv_guard.get(), "{},1,L,{},{}", cycle, produces, tid);
		}

		if(test_count < NUM_TESTS && dut->rdy_in) {
			dut->vld_in = 1;

			if(feed_step == 0) {
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

		if(print_timer >= 10000) {
			print_timer = 0;
			std::print("Now @ cycle {} (time: {} ns), currently passed: {}, failed: {}\r", cycle, ctx->time() / 1000, pass_count_back, fail_count);
		}

		print_timer++;
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
