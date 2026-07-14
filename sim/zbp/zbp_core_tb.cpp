#include <iostream>
#include <cstdint>
#include <optional>
#include <print>
#include <verilated.h>
#include <verilated_fst_c.h>
#include <csignal>
#include <atomic>
#include <ranges>
#include <random>

#include "svdpi.h"
#include "tests/utils/helpers.hpp"

#include "bigint.hpp"
#include "big_arth.hpp"
#include "mod_arth.hpp"
#include "ecc.hpp"

#include "Vtb_top.h"

std::atomic<bool> quit_requested{false};

void handle_sigint(int signal)
{
	std::cout << ("CTRL+C") << std::endl;
	quit_requested = true;
}

bool test_results(const std::vector<uint32_t> &hw_res, const std::vector<uint32_t> &expected);
std::vector<uint32_t> expected_code(const size_t ITERATIONS = 5000, const size_t MAX_THREADS = 32);
void store_u256(const bga::bgint<32> &data, size_t addr);
void store_point(const ecc::XYZZPoint<32> &data, size_t addr);

constexpr uint32_t addr2line(uint32_t addr)
{
	return addr / 4;
}

extern "C" {
void dmem_write_word(int word_addr, int data);
}

template <typename T, typename EqualityFn, typename FormatFn>
bool test_results(const std::vector<T> &hw_res,
		  const std::vector<T> &expected,
		  EqualityFn equals,
		  FormatFn format_val)
{
	bool all_match = true;

	if(hw_res.size() != expected.size()) {
		std::println(stderr, "[ERROR]: Size missmatch. HW returned {} results, Expected {} results",
			hw_res.size(), expected.size());
		return false;
	}

	int pass_count = 0;
	int fail_count = 0;
	std::println("\n========================================");
	std::println("           TEST RESULTS");
	std::println("========================================");

	for(size_t i = 0; i < hw_res.size(); i++) {
		if(!equals(hw_res[i], expected[i])) {
			std::println("  [FAILED] Thread[{:2}]: HW = {} | Expected = {}",
				i, format_val(hw_res[i]), format_val(expected[i]));

			all_match = false;
			fail_count++;
		} else {
			std::println("  [PASSED] Thread[{:2}]: {}", i, format_val(hw_res[i]));
			pass_count++;
		}
	}
	std::println("========================================");
	if(all_match) {
		std::println("  SUCCESS: All threads matched!");
	} else {
		std::println("  FAILURE: One or more threads missmatched.");
	}
	std::println("");
	std::println("===============");
	std::println("   Results");
	std::println("===============");
	std::println(" PASSED: {:2} / {}", pass_count, 32);
	std::println(" FAILED: {:2} / {}", fail_count, 32);
	std::println("");
	return all_match;
}

struct DMEM_handler {
	std::unique_ptr<FILE, decltype(&std::fclose)> fp;

	DMEM_handler(std::string_view filename)
		: fp(std::fopen(std::string(filename).c_str(), "r"), &std::fclose)
	{
		if(!fp) {
			std::println(stderr, "[ERROR]: Could not open the hex file: {}", filename);
		}
	}

	std::optional<uint32_t> fetch_u32(size_t addr)
	{
		std::rewind(fp.get());
		
		char buffer[256];
		size_t current_line = 0;

		while(std::fgets(buffer, sizeof(buffer), fp.get())) {
			if(current_line == addr) {
				char *endptr;
				uint32_t result = std::strtoul(buffer, &endptr, 16);

				if(buffer == endptr) {
					std::println(stderr, "[ERROR]: Invalid hex format on line {}: '{}'", addr, buffer);
					return std::nullopt;
				}

				return result;
			}
			current_line++;
		}
		std::println("[ERROR]: Addr {} line exceeds file length {}", addr, current_line);
		return std::nullopt;
	}

	std::optional<bga::bgint<32>> fetch_u256(size_t addr)
	{
		std::rewind(fp.get());

		char buffer[256];
		size_t current_line = 0;

		while(current_line < addr && std::fgets(buffer, sizeof(buffer), fp.get())) {
			current_line++;
		}

		if(current_line < addr) {
			std::println(stderr, "[ERROR]: Addr {} line exceeds file length {}", addr, current_line);
			return std::nullopt;
		}

		bga::bgint<32> res;
		res.reserve(8);
		for(int i = 0; i < 8; i++) {
			if(!std::fgets(buffer, sizeof(buffer), fp.get())) {
				std::println(stderr, "[ERROR]: Unexpected EOF while reading 256b value at line {}", current_line);
				return std::nullopt;
			}

			char *endptr;
			uint32_t result = std::strtoul(buffer, &endptr, 16);

			if(buffer == endptr) {
				std::println(stderr, "[ERROR]: Invalid hex format on line {}: {}", current_line, buffer);
				return std::nullopt;
			}

			res.push_bits(result);
			current_line++;
		}

		return res;
	}

	std::optional<ecc::XYZZPoint<32>> fetch_point(size_t addr)
	{
		auto opt_x = fetch_u256(addr);
		if(!opt_x) return std::nullopt;

		auto opt_y = fetch_u256(addr + 8);
		if(!opt_y) return std::nullopt;

		auto opt_zz = fetch_u256(addr + 16);
		if(!opt_zz) return std::nullopt;
		
		auto opt_zzz = fetch_u256(addr + 24);
		if(!opt_zzz) return std::nullopt;

		bga::bgint<32> zero(0);
		return ecc::XYZZPoint<32> {
			opt_x.value_or(zero),
			opt_y.value_or(zero),
			opt_zz.value_or(zero),
			opt_zzz.value_or(zero)
		};
	}
};


int main(int argc, char **argv)
{
	std::signal(SIGINT, handle_sigint);

	auto ctx = std::make_shared<VerilatedContext>();
	ctx->commandArgs(argc, argv);
	ctx->traceEverOn(true);

	auto dut = std::make_unique<Vtb_top>(ctx.get(), "dut");

	auto fst_deleter = [](VerilatedFstC *p) { if(p) { p->close(); delete p; }};
	std::unique_ptr<VerilatedFstC, decltype(fst_deleter)> tfp{ new VerilatedFstC, fst_deleter };

	constexpr int MAX_THREADS = 32;

	uint64_t cycles = 0;

	dut->trace(tfp.get(), 99);
	tfp->open("dump.fst");

	svScope scope = svGetScopeFromName("dut.tb_top.dmem_inst");

	if(!scope) {
		std::println(stderr, "[ERROR]: DPI Scope not found.");
		std::exit(1);
	}

	svSetScope(scope);

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

	std::mt19937 rng(42);
	auto gen = genRandBgN(77, rng);

	std::println("Starting CPU Simulation...");

	dut->rst = 1;
	tick();
	tick();
	dut->rst = 0;

	// mda::Montgomery<32, mda::MONT_ALGO::FIOS> mont("0xFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF43");
	// mda::Montgomery<32, mda::MONT_ALGO::FIOS> mont("0x30644e72e131a029b85045b68181585d97816a916871ca8d3c208c16d87cfd47");

	bga::bgint<32> zero("0");
	// ecc::ShortWeierstrassCurve<32> curve(zero, zero, "0x73eda753299d7d483339d80809a1d80553bda402fffe5bfeffffffff00000001");
	ecc::ShortWeierstrassCurve<32> curve(zero, zero, "0x30644e72e131a029b85045b68181585d97816a916871ca8d3c208c16d87cfd47");
	auto mont = curve.mont;

	ecc::XYZZPoint<32> P1(gen(), gen(), gen(), gen());
	ecc::XYZZPoint<32> P2(gen(), gen(), gen(), gen());

	constexpr size_t RESULTS_ADDR = addr2line(0x180);

	store_u256(mont.n,   addr2line(0x40));
	store_u256(mont.r2,  addr2line(0x0));
	store_u256(mont.n_p, addr2line(0x60));

	store_point(P1, 0x080);
	store_point(P2, 0x100);


	while(!ctx->gotFinish() && !quit_requested) {
		tick();

		if(cycles % 100000 == 0) {
			std::println("[Heartbeat]: Simulation running... CYCLE: {} ", cycles);
		}
	}

	tfp.reset();
	dut->final();

	if(quit_requested) {
		std::println("\nCtrl+C Early Quiting Simulation");
		std::fflush(stdout);
		return 0;
	} else {
		std::println("Simulation Complete after {} clk cycles", cycles);
		std::fflush(stdout);
	}

	constexpr int W = 32;

	std::println("");
	std::println("Modulus = {0:x}", mont.n);
	std::println("R2 = {0:x}, {0}", mont.r2);
	std::println("Np = {0:x}, {0}", mont.n_p);
	std::println("");

	DMEM_handler dmem("dmem_dump.hex");

	std::vector<ecc::XYZZPoint<32>> expected(MAX_THREADS, curve.add(P1, P2));
	std::vector<ecc::XYZZPoint<32>> hw_res;

	for(int i = 0; i < MAX_THREADS; i++) {
		hw_res.push_back(
			dmem.fetch_point(RESULTS_ADDR + i*32)
				.value_or(ecc::XYZZPoint<32>()));
	}

	test_results(hw_res, expected,
	      [](const ecc::XYZZPoint<32> &a, const ecc::XYZZPoint<32> &b) {
		return (a.X == b.X) && (a.Y == b.Y) && (a.ZZ == b.ZZ) && (a.ZZZ == b.ZZZ);
	      },
	      [](const ecc::XYZZPoint<32> &p) {
	      	return std::format("[ \n\tX:   {:x},\n\tY:   {:x},\n\tZZ:  {:x},\n\tZZZ: {:x}\n]", p.X, p.Y, p.ZZ, p.ZZZ);
	      });
	
	return 0;
}

void store_u256(const bga::bgint<32> &data, size_t addr)
{
	for(const auto [idx, chunk] : data | std::views::take(8) | std::views::enumerate) {
		dmem_write_word((addr + idx), chunk);
	}
}

void store_point(const ecc::XYZZPoint<32> &data, size_t addr)
{
	store_u256(data.X,   addr2line(addr + 0x00));
	store_u256(data.Y,   addr2line(addr + 0x20));
	store_u256(data.ZZ,  addr2line(addr + 0x40));
	store_u256(data.ZZZ, addr2line(addr + 0x60));
}

std::vector<uint32_t> expected_code(const size_t ITERATIONS, const size_t MAX_THREADS)
{
	std::vector<uint32_t> results;
	results.reserve(MAX_THREADS);

	for(size_t tid = 0; tid < MAX_THREADS; tid++) {
		uint32_t state = 0x811C9DC5 ^ (static_cast<uint32_t>(tid) * 0x01000193);
		uint32_t acc = 0;
		
		// Scratch array using standard C-arrays
		uint32_t scratch[4] = {
		    static_cast<uint32_t>(tid),
		    static_cast<uint32_t>(tid + 1),
		    static_cast<uint32_t>(tid + 2),
		    static_cast<uint32_t>(tid + 3)
		};

		for (size_t i = 0; i < ITERATIONS; ++i) {
			state ^= (state << 13);
			state ^= (state >> 17);
			state ^= (state << 5);

			state = (state * 1664525) + 1013904223;

			size_t read_idx = (i + 2) & 3;
			size_t write_idx = i & 3;

			uint32_t mix = scratch[read_idx];
			scratch[write_idx] = state ^ mix;

			acc += (state ^ scratch[write_idx]);
		}

		results.push_back(acc);
	}
	return results;
}
