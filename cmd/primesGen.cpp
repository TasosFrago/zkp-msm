#include <chrono>
#include <memory>
#include <print>
#include <ranges>
#include <string>
#include <vector>

#include "bigint.hpp"
#include "mod_arth.hpp"

#include "tests/utils/helpers.hpp"

void print_usage(const char *prog_name);

int main(int argc, char **argv)
{
	std::string fileName = "out.txt";
	size_t num_primes = 10;
	size_t num_digits = 10;

	for(int i = 1; i < argc; ++i) {
		std::string_view arg = argv[i];

		if(arg == "-o" || arg == "--output") {
			if(i + 1 < argc) {
				fileName = argv[++i];
			} else {
				std::println(stderr, "Error: Missing value for output file flag.");
				return 1;
			}
		} else if(arg == "-n" || arg == "--count") {
			if(i + 1 < argc) {
				try {
					num_primes = std::stoul(argv[++i]);
				} catch(...) {
					std::println(stderr, "Error: Invalid number format for count.");
					return 1;
				}
			} else {
				std::println(stderr, "Error: Missing value for count flag.");
				return 1;
			}
		} else if(arg == "-d" || arg == "--digits") {
			if(i + 1 < argc) {
				try {
					num_digits = std::stoul(argv[++i]);
				} catch(...) {
					std::println(stderr, "Error: Invalid number format for digits.");
					return 1;
				}
			} else {
				std::println(stderr, "Error: Missing value for digits flag.");
				return 1;
			}
		} else if(arg == "-h" || arg == "--help") {
			print_usage(argv[0]);
			return 0;
		} else {
			std::println(stderr, "Unknown argument: {}", arg);
			print_usage(argv[0]);
			return 1;
		}
	}

	std::println("Configuration -> File: '{}', Count: {}, Digits: {}", fileName, num_primes, num_digits);

	constexpr size_t N = 32;
	auto range = bga::bgint<N>(genRandBgN(10)());

	std::vector<bga::bgint<N>> primes;
	primes.reserve(num_primes);

	auto start_time = std::chrono::high_resolution_clock::now();

	for(size_t i = 0; i < num_primes; i++) {
		auto &pt = mda::get_primalityTester<N, mda::MONT_ALGO::FIOS>();
		primes.emplace_back(pt.generate_prime(range));
	}
	auto end_time = std::chrono::high_resolution_clock::now();

	auto duration = std::chrono::duration_cast<std::chrono::duration<double>>(end_time - start_time);
	std::println("Finished Generating {} primes in \033[94m{:.3f}s\033[0m", num_primes, duration.count());

	std::unique_ptr<FILE, decltype(&std::fclose)> fp{
		std::fopen(fileName.c_str(), "w"),
		&std::fclose
	};
	if(!fp) {
		std::println(stderr, "Couldn't open file {}", fileName);
		return -1;
	}

	for(const auto [idx, p] : primes | std::views::enumerate) {
		std::print(fp.get(), "\"{}\"", p);
		if(static_cast<size_t>(idx) < (primes.size() - 1)) {
			std::print(fp.get(), ",\n");
		}
	}

	std::println("Completed writing primes!");

	return 0;
}

void print_usage(const char *prog_name)
{
	std::println(stderr, "Usage: {} [-o output_file] [-n number_of_primes]", prog_name);
	std::println(stderr, "  -o, --output  : Output filename (default: out.txt)");
	std::println(stderr, "  -n, --count   : Number of primes to generate (default: 10)");
	std::println(stderr, "  -d, --digits  : Digit count for the range (default: 10)");
	std::println(stderr, "  -h, --help    : Show this help message");
}
