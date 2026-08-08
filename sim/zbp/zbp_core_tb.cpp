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
#include <algorithm>

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

template <typename PointT>
struct msm_packet {
	bga::bgint<PointT::bits> scalar;
	PointT point;
};

template <size_t MSM_SIZE, typename CurveT, typename PointT>
	requires ecc::EllipticCurveConcept<CurveT, PointT>
auto generate_msm_data(
	std::function<std::string()> gen,
	const CurveT &curve,
	const PointT &G
) -> std::optional<std::pair< std::vector<PointT>, std::vector<bga::bgint<PointT::bits>> >>
{
	using ScalarT = bga::bgint<PointT::bits>;

	if(!curve.is_on_curve(G)) {
		std::println("[ERROR]: Generator point is not on the curve");
		return std::nullopt;
	}

	std::vector<PointT> points;
	points.reserve(MSM_SIZE);

	std::vector<ScalarT> scalars;
	scalars.reserve(MSM_SIZE);

	std::vector<ScalarT> used_scalars;
	used_scalars.reserve(MSM_SIZE);

	for(size_t i = 0; i < MSM_SIZE; i++) {
		PointT point;
		ScalarT k;
		do {
			do {
				k = ScalarT(gen());
			} while(k.is_zero() ||
				std::find(used_scalars.begin(), used_scalars.end(), k) != used_scalars.end());

			point = ecc::scalarMul<CurveT, PointT>(curve, G, k);
		} while(!curve.is_on_curve(point));
		used_scalars.push_back(k);

		points.push_back(std::move(point));
		scalars.push_back(ScalarT(gen()));
	}

	return std::pair{ points, scalars };
}

int inputs_base_extract(std::string_view &file)
{
	std::unique_ptr<FILE, decltype(&std::fclose)> fp(
		std::fopen(std::string(file).c_str(), "r"),
		&std::fclose
	);

	if (!fp) {
		return -1;
	}

	char line[512];
	const char *target = "INPUTS_BASE_HEX=";
	size_t target_len = std::strlen(target);

	while(std::fgets(line, sizeof(line), fp.get())) {
		if(const char *match = std::strstr(line, target)) {
			const char *value_start = match + target_len;
			try {
				return std::stoi(value_start, nullptr, 16);
			} catch(...) {
				return -2;
			}
		}
	}
	return -2;
}


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

	constexpr size_t MSM_SIZE = 64;
	// constexpr size_t POINTS_STRUCT_ADDR = 0x44000;
	assert(INPUTS_BASE_HEX != 0);
	constexpr size_t POINTS_STRUCT_ADDR = INPUTS_BASE_HEX;
	constexpr size_t SCALARS_STRUCT_ADDR = POINTS_STRUCT_ADDR + 4 + (64 * (4 * (256/8)));
	std::println("POINTS_STRUCT_ADDR = 0x{:X}", POINTS_STRUCT_ADDR);
	std::println("SCALARS_STRUCT_ADDR = 0x{:X}", SCALARS_STRUCT_ADDR);

	constexpr size_t RESULTS_ADDR = addr2line(0x10e0);

	dmem_write_word(addr2line(POINTS_STRUCT_ADDR), MSM_SIZE);
	dmem_write_word(addr2line(SCALARS_STRUCT_ADDR), MSM_SIZE);

	auto fmt_point_scalar_pair = [](size_t idx, const ecc::XYZZPoint<32> &point, const bga::bgint<32> &scalar) {
		return std::format("[{}] = {{\n\tscalar:   {:x},\n\tX:   {:x},\n\tY:   {:x},\n\tZZ:  {:x}\n\tZZZ: {:x}\n}},", idx, scalar, point.X, point.Y, point.ZZ, point.ZZZ);
	};

	// BN254
	bga::bgint<32> a("0");
	bga::bgint<32> b("3");
	ecc::ShortWeierstrassCurve<32> curve(a, b, "0x30644e72e131a029b85045b68181585d97816a916871ca8d3c208c16d87cfd47");
	auto mont = curve.mont;

	ecc::XYZZPoint<32> G_bn254{1, 2, 1, 1};
	
	auto msm_test_data = generate_msm_data<MSM_SIZE>(gen, curve, G_bn254);
	if(!msm_test_data) {
		return 0;
	}
	auto&& [points, scalars] = *std::move(msm_test_data);

	for(size_t i = 0; i < MSM_SIZE; i++) {
		// std::println("{}", fmt_point_scalar_pair(i, points[i], scalars[i]));
		store_point(points[i], addr2line(POINTS_STRUCT_ADDR + 4 + (i*0x80)));
		store_u256(scalars[i], addr2line(SCALARS_STRUCT_ADDR + 4 + (i*0x20)));
	}

	auto fmtXYZZ = [](const ecc::XYZZPoint<32> &p) {
		return std::format("[ \n\tX:   {:x},\n\tY:   {:x},\n\tZZ:  {:x},\n\tZZZ: {:x}\n]", p.X, p.Y, p.ZZ, p.ZZZ);
	};

	auto expected_res = ecc::msm<ecc::ShortWeierstrassCurve<32>, ecc::XYZZPoint<32>, 4>(curve, points, scalars);
	// auto naive_res = ecc::msm_easy<ecc::ShortWeierstrassCurve<32>, ecc::XYZZPoint<32>>(curve, points, scalars);

	// std::println("MSM Naive Result: {}", fmtXYZZ(naive_res));
	std::println("MSM Pipenger Result: {}", fmtXYZZ(expected_res));
	// auto match = curve.to_affine(expected_res) == curve.to_affine(naive_res);
	// std::println("DO they match {}", (match) ? "YES" : "NO");
	std::fflush(stdout);
	// assert(curve.to_affine(expected_res) == curve.to_affine(naive_res));

	store_u256(mont.n,   addr2line(0x40));
	store_u256(mont.r2,  addr2line(0x60));
	store_u256(mont.n_p, addr2line(0x80));
	store_u256(curve.a,  addr2line(0xa0));
	store_u256(curve.b,  addr2line(0xc0));

	std::vector<ecc::XYZZPoint<32>> expected(1, expected_res);

	while(!ctx->gotFinish() && !quit_requested) {
		tick();
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

	std::println("");
	std::println("Modulus = {0:x}", mont.n);
	std::println("R2 = {0:x}, {0}", mont.r2);
	std::println("Np = {0:x}, {0}", mont.n_p);
	std::println("");

	DMEM_handler dmem("dmem_dump.hex");

	std::vector<ecc::XYZZPoint<32>> hw_res;

	for(int i = 0; i < 1; i++) {
		hw_res.push_back(
			dmem.fetch_point(RESULTS_ADDR + i*32)
				.value_or(ecc::XYZZPoint<32>()));
	}

	std::vector<ecc::AffinePoint<32>> hw_res_aff;
	hw_res_aff.reserve(1);

	for(int i = 0; i < 1; i++) {
		hw_res_aff.push_back(curve.to_affine(hw_res[i]));
	}

	std::vector<ecc::AffinePoint<32>> exp_res_aff;
	exp_res_aff.reserve(1);

	for(int i = 0; i < 1; i++) {
		exp_res_aff.push_back(curve.to_affine(expected[i]));
	}

	test_results(hw_res, expected,
	      [](const ecc::XYZZPoint<32> &a, const ecc::XYZZPoint<32> &b) {
		return (a.X == b.X) && (a.Y == b.Y) && (a.ZZ == b.ZZ) && (a.ZZZ == b.ZZZ);
	      },
	      fmtXYZZ);

	test_results(hw_res_aff, exp_res_aff,
	      [](const ecc::AffinePoint<32> &a, const ecc::AffinePoint<32> &b) {
		return a == b;
	      },
	      [](const ecc::AffinePoint<32> &p) {
		return std::format("[\n\tX: {:x},\n\tY: {:x}\n ]", p.x, p.y);
	      });
	
	return 0;
}

void store_u256(const bga::bgint<32> &data, size_t addr)
{
	size_t words_written = 0;
	for(const auto [idx, chunk] : data | std::views::take(8) | std::views::enumerate) {
		// std::println("From TB writing to addr: 0x{:x} val: {:x}", addr+idx, chunk);
		dmem_write_word((addr + idx), chunk);
		words_written++;
	}
	for(size_t i = words_written; i < 8; i++) {
		// std::println("From TB writing to addr: 0x{:x} val: {:x}", addr+i, 0);
		dmem_write_word((addr + i), 0);
	}
}

void store_point(const ecc::XYZZPoint<32> &data, size_t addr)
{
	store_u256(data.X,   addr + 0);
	store_u256(data.Y,   addr + 8);
	store_u256(data.ZZ,  addr + 16);
	store_u256(data.ZZZ, addr + 24);
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
