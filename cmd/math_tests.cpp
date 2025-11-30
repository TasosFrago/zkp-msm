#include <cstdint>
#include <print>
#include <random>
#include <string>

#include "big_arth.hpp"
#include "bigint.hpp"

#include "tests.hpp"

void test_operations()
{
	// --- Single Chunk Tests (Small Numbers) ---
	std::random_device rd;
	std::mt19937_64 gen(rd());
	std::uniform_int_distribution<uint32_t> dist;
	// std::uniform_int_distribution<uint8_t> dist2;

	auto run_test = [&dist, &gen]<size_t N>() {
		using namespace bga;
		__int128_t A = dist(gen), B = dist(gen);
		BigInt<N> a(A), b(B);

		test_assert(BigInt<N>(A + B), add(a, b), "a: {}, b: {}, (A + B) = {}", A, B, (A + B));
		test_assert(BigInt<N>(A - B), sub(a, b), "a: {}, b: {}, (A - B) = {}", A, B, (A - B));
		test_assert(BigInt<N>(A * B), mul(a, b), "a: {}, b: {}, (A * B) = {}", A, B, (A * B));

		auto safe_rshift = [](uint64_t a, uint8_t b) { return (b >= sizeof(uint64_t) * 8) ? 0 : (a >> b); };
		test_assert(BigInt<N>(safe_rshift(A, (uint8_t)B)), rshift(a, (uint8_t)B), "a: {}, b: {}, chunk {}", A, (uint8_t)B, rshift(a, (uint8_t)B).get_chunks());
		// uint16_t A2 = dist2(gen), B2 = dist2(gen);
		// BigInt<N> a2(A2);
		// test_assert(BigInt<N>((uint64_t)A2 << (uint64_t)B2), lshift(a2, B2), "a: {}, b: {}, (A << B) = {}", A2, B2, ((uint64_t)A2 << (uint64_t)B2));
	};

	auto run_test_batch = [&]<size_t... Ns>() {
		(run_test.template operator()<Ns>(), ...);
	};

	constexpr const size_t BATCHES = 1000;

	std::println("Running {} tests for all math operations, with chunks of 2, 4, 8, 16, 32 and 64 bits", BATCHES);
	for(size_t i = 0; i < BATCHES; i++) {
		run_test_batch.template operator()<2, 4, 8, 16, 32, 64>();
		progress(i, BATCHES);
	}
	std::println("PASSED ALL TESTS{}", std::string(12, ' '));
}

void test_operations_with_bc()
{
	constexpr const size_t DIGITS = 10;
	auto gen = genRandBgN(DIGITS);
	auto gen2 = genRandBgN(3);
	auto gen3 = genRandBgN(1);

	auto run_test = [&gen, &gen2, &gen3]<size_t N>() {
		using namespace bga;
		auto fast_atoi = [](const char *str) {
			uint32_t val = 0;
			while(*str) {
				val = val * 10 + (*str++ - '0');
			}
			return val;
		};

		std::string A = gen(), B = gen();
		BigInt<N> a(A), b(B);

		test_assert(run_bc(a, "+", b), add(a, b), "a: {}, b: {}", A, B);
		test_assert(run_bc(a, "-", b), sub(a, b), "a: {}, b: {}", A, B);
		test_assert(run_bc(a, "*", b), mul(a, b), "a: {}, b: {}", A, B);

		std::string A1 = gen2(), B1 = gen2();
		BigInt<N> a1(A1), b1(B1);
		test_assert(shift_bc(a1, "*", b1), lshift(a1, fast_atoi(B1.c_str())), "a: {}, b: {}", A1, B1);

		std::string B2 = gen3();
		BigInt<N> b2(B2);
		test_assert(shift_bc(a, "/", b2), rshift(a, fast_atoi(B2.c_str())), "a: {}, b: {}", A, B2);
	};

	auto run_test_batch = [&]<size_t... Ns>() {
		(run_test.template operator()<Ns>(), ...);
	};

	constexpr const size_t BATCHES = 1000;

	std::println("Running {} tests for all math operations with bc, with chunks of 32 and 64 bits", BATCHES);
	for(size_t i = 0; i < BATCHES; i++) {
		run_test_batch.template operator()<32, 64>();

		progress(i, BATCHES);
	}
	std::println("PASSED ALL TESTS{}", std::string(12, ' '));
}
