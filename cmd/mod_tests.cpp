#include <random>

#include "bigint.hpp"
#include "mod_arth.hpp"

#include "tests.hpp"

void test_mod()
{
	// --- Single Chunk Tests (Small Numbers) ---
	std::random_device rd;
	std::mt19937_64 gen(rd());
	std::uniform_int_distribution<uint32_t> dist;

	auto run_test = [&dist, &gen]<size_t N>() {
		using bga::BigInt;

		uint64_t mod_val = dist(gen);
		if(mod_val == 0) mod_val = 1;

		__int128_t A = dist(gen) % mod_val;
		__int128_t B = dist(gen) % mod_val;
		__int128_t M = mod_val;

		BigInt<N> a(A), b(B), m(M);

		// Modular Addition
		// C++ Check: (A + B) % M
		test_assert(BigInt<N>((A + B) % M),
			    mda::add(a, b, m),
			    "Mod Add: a: {}, b: {}, m: {}", A, B, M);

		// Modular Subtraction
		// C++ Check: ((A - B) % M + M) % M (Euclidean Modulo)
		__int128_t sub_expected = ((A - B) % M + M) % M;
		test_assert(BigInt<N>(sub_expected),
			    mda::sub(a, b, m),
			    "Mod Sub: a: {}, b: {}, m: {}", A, B, M);

		// Modular Multiplication
		// C++ Check: (A * B) % M
		// test_assert(BigInt<N>((A * B) % M),
		// 	    mda::mul(a, b, m),
		// 	    "Mod Mul: a: {}, b: {}, m: {}", A, B, M);
	};

	auto run_test_batch = [&]<size_t... Ns>() {
		(run_test.template operator()<Ns>(), ...);
	};

	constexpr const size_t BATCHES = 1000;

	std::println("Running {} tests for MODULAR operations, with chunks of 2, 4, 8, 16, 32 and 64 bits", BATCHES);
	for(size_t i = 0; i < BATCHES; i++) {
		run_test_batch.template operator()<2, 4, 8, 16, 32, 64>();
		progress(i, BATCHES);
	}
	std::println("PASSED ALL MOD TESTS{}", std::string(12, ' '));
}

void test_mod_with_bc()
{
	constexpr const size_t DIGITS = 15;

	auto gen_mod = genRandBgN(DIGITS);
	auto gen_op = genRandBgN(DIGITS - 1);

	auto run_test = [&gen_mod, &gen_op]<size_t N>() {
		using bga::BigInt;

		std::string M_str = gen_mod();
		std::string A_str = gen_op();
		std::string B_str = gen_op();

		BigInt<N> m(M_str);
		BigInt<N> a(A_str);
		BigInt<N> b(B_str);

		// (A + B) % M
		test_assert(run_mod_bc(a, "+", b, m),
			    mda::add(a, b, m),
			    "Mod Add: \na: {}\nb: {}\nm: {}", A_str, B_str, M_str);

		// (A - B) % M
		test_assert(run_mod_bc(a, "-", b, m),
			    mda::sub(a, b, m),
			    "Mod Sub: \na: {}\nb: {}\nm: {}", A_str, B_str, M_str);

		// (A * B) % M
		// test_assert(run_mod_bc(a, "*", b, m),
		// 	    mda::mul(a, b, m),
		// 	    "Mod Mul: \na: {}\nb: {}\nm: {}", A_str, B_str, M_str);
	};

	auto run_test_batch = [&]<size_t... Ns>() {
		(run_test.template operator()<Ns>(), ...);
	};

	constexpr const size_t BATCHES = 200;

	std::println("Running {} tests for MODULAR operations with BC, chunks 32, 64", BATCHES);
	for(size_t i = 0; i < BATCHES; i++) {
		run_test_batch.template operator()<32, 64>();
		progress(i, BATCHES);
	}
	std::println("PASSED ALL MOD BC TESTS{}", std::string(12, ' '));
}
