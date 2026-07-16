#include <unistd.h>
#include <iostream>
#include <cstdint>
#include <vector>
#include <random>
#include <ranges>
#include <print>

#include "svdpi.h"

#include "bigint.hpp"
#include "big_arth.hpp"
#include "mod_arth.hpp"
#include "ecc.hpp"

#include "tests/utils/helpers.hpp"

extern "C" {
void dmem_write_word(int word_addr, int data);
int dmem_read_word(int word_addr);
}

constexpr uint32_t addr2line(uint32_t addr)
{
	return addr / 4;
}

void store_u256(const bga::bgint<32> &data, size_t addr)
{
    // Force exactly 8 chunks (256 bits) to be written every time
    for (int i = 0; i < 8; i++) {
        // If the chunk exists in the bigint, write it. Otherwise, write 0.
        uint32_t chunk = (i < data.get_chunks()) ? data.get(i) : 0;
        dmem_write_word((addr + i), chunk);
    }
}

void store_point(const ecc::XYZZPoint<32> &data, size_t addr)
{
	store_u256(data.X,   addr2line(addr + 0x00));
	store_u256(data.Y,   addr2line(addr + 0x20));
	store_u256(data.ZZ,  addr2line(addr + 0x40));
	store_u256(data.ZZZ, addr2line(addr + 0x60));
}

static std::vector<ecc::XYZZPoint<32>> g_expected;
static constexpr int MAX_THREADS = 32;
static constexpr size_t RESULTS_ADDR = addr2line(0x180);

extern "C" void dpi_tb_init()
{
	std::println(stderr, "\nHello from DPI\n");
	std::fflush(stderr);
	svScope scope = svGetScopeFromName("zbp_tb.dut.dmem_inst");
	if(!scope) {
		std::println(stderr, "[DPI-C++ ERROR]: DPI Scope not found");
		return;
	}
	svSetScope(scope);

	std::mt19937 rng(42);
	auto gen = genRandBgN(77, rng);

	bga::bgint<32> zero("0");
	constexpr const char *MOD_STR = "0x30644e72e131a029b85045b68181585d97816a916871ca8d3c208c16d87cfd47";
	ecc::ShortWeierstrassCurve<32> curve(zero, zero, MOD_STR);
	auto mont = curve.mont;

	auto x1 = gen();
	auto y1 = gen();
	auto zz1 = gen();
	auto zzz1 = gen();
	ecc::XYZZPoint<32> P1(x1, y1, zz1, zzz1);
	auto x2 = gen();
	auto y2 = gen();
	auto zz2 = gen();
	auto zzz2 = gen();
	ecc::XYZZPoint<32> P2(x2, y2, zz2, zzz2);

	store_u256(mont.n,   addr2line(0x40));
	store_u256(mont.r2,  addr2line(0x0));
	store_u256(mont.n_p, addr2line(0x60));

	store_point(P1, 0x080);
	store_point(P2, 0x100);
	std::println("[DPI-C++]: Memory Initialization Complete.");

	g_expected = std::vector<ecc::XYZZPoint<32>>(MAX_THREADS, curve.add(P1, P2));
	std::println("[DPI-C++]: SW model run.");
}

std::optional<bga::bgint<32>> fetch_u256(size_t addr)
{
	bga::bgint<32> res;
	res.reserve(8);
	for(int i = 0; i < 8; i++) {
		res.push_bits(dmem_read_word(addr + i));
	}
	return res;
}

std::optional<ecc::XYZZPoint<32>> fetch_point_dpi(size_t addr)
{
	bga::bgint<32> zero(0);
	return ecc::XYZZPoint<32> {
		fetch_u256(addr)     .value_or(zero),
		fetch_u256(addr + 8) .value_or(zero),
		fetch_u256(addr + 16).value_or(zero),
		fetch_u256(addr + 24).value_or(zero)
	};
}

extern "C" void dpi_tb_verify()
{
	svScope scope = svGetScopeFromName("zbp_tb.dut.dmem_inst");
	svSetScope(scope);
	std::println("\n========================================");
	std::println("           TEST RESULTS");
	std::println("========================================");

	bool all_match = true;
	int pass_count = 0;
	int fail_count = 0;

	auto fmt_point = [](const ecc::XYZZPoint<32> &p) {
		return std::format("[ \n\tX:   {:x},\n\tY:   {:x},\n\tZZ:  {:x},\n\tZZZ: {:x}\n]", p.X, p.Y, p.ZZ, p.ZZZ);
	};

	for(int i = 0; i < MAX_THREADS; i++) {
		auto hw_res = fetch_point_dpi(RESULTS_ADDR + i * 32).value();
		auto exp = g_expected[i];

		bool match = (hw_res.X == exp.X) && (hw_res.Y == exp.Y) && (hw_res.ZZ == exp.ZZ) && (hw_res.ZZZ == exp.ZZZ);

		if(match) {
			std::println("  [PASSED] Thread[{:2}]: {}", i, fmt_point(hw_res));
			pass_count++;
		} else {
			std::println("  [FAILED] Thread[{:2}]: Hw = {} | Exp = {}", i, fmt_point(hw_res), fmt_point(exp));
			fail_count++;
			all_match = false;
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
}
