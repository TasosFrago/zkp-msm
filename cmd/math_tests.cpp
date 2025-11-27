#include <print>
#include <random>

#include "big_arth.hpp"
#include "bigint.hpp"

#include "tests.hpp"

void test_operations()
{
	// --- Single Chunk Tests (Small Numbers) ---
	std::random_device rd;
	std::mt19937_64 gen(rd());
	std::uniform_int_distribution<uint32_t> dist;

	auto run_test = [&dist, &gen]<size_t N>() {
		using namespace bga;
		__int128_t A = dist(gen), B = dist(gen);
		BigInt<N> a(A), b(B);

		test_assert(BigInt<N>(A + B), add(a, b), "a: {}, b: {}, (A + B) = {}", A, B, (A + B));
		test_assert(BigInt<N>(A - B), sub(a, b), "a: {}, b: {}, (A - B) = {}", A, B, (A - B));
		test_assert(BigInt<N>(A * B), mul(a, b), "a: {}, b: {}, (A * B) = {}", A, B, (A * B));
	};

	auto run_test_batch = [&]<size_t... Ns>() {
		(run_test.template operator()<Ns>(), ...);
	};

	constexpr const size_t BATCHES = 1000;

	std::println("Running {} tests for all math operations, with chunks of 2, 4, 8, 16, 32 and 64 bits", BATCHES);
	for(size_t i = 0; i < BATCHES; i++) {
		run_test_batch.template operator()<2, 4, 8, 16, 32, 64>();
	}
	std::println("PASSED ALL TESTS");
}
