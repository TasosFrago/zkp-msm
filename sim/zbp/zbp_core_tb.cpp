#include <cstdint>
#include <print>
#include <verilated.h>
#include <verilated_fst_c.h>
#include <csignal>
#include <atomic>

#include "Vtb_top.h"

std::atomic<bool> quit_requested{false};

void handle_sigint(int signal)
{
	std::println("CTRL+C");
	quit_requested = true;
}

bool test_results(const std::vector<uint32_t> &hw_res, const std::vector<uint32_t> &expected);
std::vector<uint32_t> expected_code(const size_t ITERATIONS = 5000, const size_t MAX_THREADS = 32);

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

	// constexpr int MAX_CYCLES = 100000;
	// for(int cycle = 0; cycle < MAX_CYCLES; cycle++) {
	while(!ctx->gotFinish() && !quit_requested) {
		tick();

		if(cycles % 100000 == 0) {
			std::println("[Heartbeat]: Simulation running... CYCLE: {} ", cycles);
		}
	}

	if(quit_requested) {
		std::println("\nCtrl+C Early Quiting Simulation");
		std::fflush(stdout);
	} else {
		std::println("Simulation Complete after {} clk cycles", cycles);
		std::fflush(stdout);
	}

	tfp.reset();
	dut->final();

	constexpr size_t RESULTS_ADDR = (0x200 / 4);

	std::vector<uint32_t> hw_res;

	DMEM_handler dmem("dmem_dump.hex");

	for(int i = 0; i < MAX_THREADS; i++) {
		hw_res.push_back(
			dmem.fetch_u32(RESULTS_ADDR + i).value_or(0)
		);
	}

	auto expected_res = expected_code(5000);

	test_results(hw_res, expected_res);
	
	return 0;
}

bool test_results(const std::vector<uint32_t> &hw_res, const std::vector<uint32_t> &expected)
{
	bool all_match = true;

	if(hw_res.size() != expected.size()) {
		std::println(stderr, "[ERROR]: Size missmatch. HW returned {} results, Expected {} results",
			hw_res.size(), expected.size());
		return false;
	}

	std::println("\n========================================");
	std::println("           TEST RESULTS");
	std::println("========================================");

	for(size_t i = 0; i < hw_res.size(); i++) {
		if(hw_res[i] != expected[i]) {
			std::println("  [FAILED] Thread[{:2}]: HW = 0x{:08X} | Expected = 0x{:08X}",
				i, hw_res[i], expected[i]);

			all_match = false;
		} else {
			std::println("  [PASSED] Thread[{:2}]: 0x{:08X}", i, hw_res[i]);
		}
	}
	std::println("========================================");
	if(all_match) {
		std::println("  SUCCESS: All threads matched!");
	} else {
		std::println("  FAILURE: One or more threads missmatched.");
	}
	return all_match;
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
