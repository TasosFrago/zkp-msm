#include <random>

#include "bigint.hpp"
#include "mod_arth.hpp"

#include "test_framework.hpp"
#include "tests.hpp"

struct TestModLogic {
	constexpr static mda::MONT_ALGO sos = mda::MONT_ALGO::SOS;
	constexpr static mda::MONT_ALGO cios = mda::MONT_ALGO::CIOS;
	constexpr static mda::MONT_ALGO fios = mda::MONT_ALGO::FIOS;

	template <size_t N>
	void operator()(std::mt19937_64 &gen) const noexcept
	{
		std::uniform_int_distribution<uint32_t> dist;
		using bga::BigInt;

		uint64_t mod_val = dist(gen);
		if(mod_val == 0) mod_val = 1;
		mod_val |= 1;

		__int128_t A = dist(gen) % mod_val;
		__int128_t B = dist(gen) % mod_val;
		__int128_t M = mod_val;

		BigInt<N> a(A), b(B), m(M);

		// Modular Addition
		// C++ Check: (A + B) % M
		test_assert("mod addition",
			    BigInt<N>((A + B) % M),
			    mda::add(a, b, m),
			    "Mod Add: a: {}, b: {}, m: {}", A, B, M);

		// Modular Subtraction
		// C++ Check: ((A - B) % M + M) % M (Euclidean Modulo)
		__int128_t sub_expected = ((A - B) % M + M) % M;
		test_assert("mod subtraction",
			    BigInt<N>(sub_expected),
			    mda::sub(a, b, m),
			    "Mod Sub: a: {}, b: {}, m: {}", A, B, M);

		// Modular Multiplication
		// C++ Check: (A * B) % M
		if constexpr(N > 8) {

			test_assert("mod multiplication SOS",
				    BigInt<N>((A * B) % M),
				    (mda::mul<N, sos>(a, b, m)),
				    "Mod Mul: a: {}, b: {}, m: {}", A, B, M);
		}

		test_assert("mod multiplication CIOS",
			    BigInt<N>((A * B) % M),
			    (mda::mul<N, cios>(a, b, m)),
			    "Mod Mul: a: {}, b: {}, m: {}", A, B, M);

		test_assert("mod multiplication FIOS",
			    BigInt<N>((A * B) % M),
			    (mda::mul<N, fios>(a, b, m)),
			    "Mod Mul: a: {}, b: {}, m: {}", A, B, M);
	};
};

struct TestModBCLogic {
	constexpr static const size_t DIGITS = 15;
	constexpr static mda::MONT_ALGO sos = mda::MONT_ALGO::SOS;
	constexpr static mda::MONT_ALGO cios = mda::MONT_ALGO::CIOS;
	constexpr static mda::MONT_ALGO fios = mda::MONT_ALGO::FIOS;

	template <size_t N>
	void operator()(std::mt19937_64 &gen) const noexcept
	{
		auto gen_mod = genRandBgN(DIGITS);
		auto gen_op = genRandBgN(DIGITS - 1);

		using bga::BigInt;

		std::string M_str = gen_mod();

		if(!M_str.empty()) {
			M_str.back() |= 1;
		} else {
			M_str = "1";
		}

		std::string A_str = gen_op();
		std::string B_str = gen_op();

		BigInt<N> m(M_str);
		BigInt<N> a(A_str);
		BigInt<N> b(B_str);

		// (A + B) % M
		test_assert("mod bc addition",
			    run_mod_bc(a, "+", b, m),
			    mda::add(a, b, m),
			    "Mod Add: \na: {}\nb: {}\nm: {}", A_str, B_str, M_str);

		// (A - B) % M
		test_assert("mod bc subtraction",
			    run_mod_bc(a, "-", b, m),
			    mda::sub(a, b, m),
			    "Mod Sub: \na: {}\nb: {}\nm: {}", A_str, B_str, M_str);

		// (A * B) % M
		test_assert("mod bc multiplication SOS",
			    run_mod_bc(a, "*", b, m),
			    (mda::mul<N, sos>(a, b, m)),
			    "Mod Mul: \na: {}\nb: {}\nm: {}", A_str, B_str, M_str);

		test_assert("mod bc multiplication CIOS",
			    run_mod_bc(a, "*", b, m),
			    (mda::mul<N, cios>(a, b, m)),
			    "Mod Mul: \na: {}\nb: {}\nm: {}", A_str, B_str, M_str);

		test_assert("mod bc multiplication FIOS",
			    run_mod_bc(a, "*", b, m),
			    (mda::mul<N, fios>(a, b, m)),
			    "Mod Mul: \na: {}\nb: {}\nm: {}", A_str, B_str, M_str);
	};
};

void register_mod_tests()
{
	TESTS.register_test<BITS_TEST_MOD___N>(
	    "Modular operations",
	    MOD_BATCHES,
	    TestModLogic{});

	TESTS.register_test<BITS_TEST_MOD__BC>(
	    "Modular BC operations",
	    MOD_BC_BATCHES,
	    TestModBCLogic{});
}

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
		mod_val |= 1;

		__int128_t A = dist(gen) % mod_val;
		__int128_t B = dist(gen) % mod_val;
		__int128_t M = mod_val;

		BigInt<N> a(A), b(B), m(M);

		// Modular Addition
		// C++ Check: (A + B) % M
		test_assert("mod addition",
			    BigInt<N>((A + B) % M),
			    mda::add(a, b, m),
			    "Mod Add: a: {}, b: {}, m: {}", A, B, M);

		// Modular Subtraction
		// C++ Check: ((A - B) % M + M) % M (Euclidean Modulo)
		__int128_t sub_expected = ((A - B) % M + M) % M;
		test_assert("mod subtraction",
			    BigInt<N>(sub_expected),
			    mda::sub(a, b, m),
			    "Mod Sub: a: {}, b: {}, m: {}", A, B, M);

		// Modular Multiplication
		// C++ Check: (A * B) % M
		if constexpr(N > 8) {
			constexpr mda::MONT_ALGO sos = mda::MONT_ALGO::SOS;

			test_assert("mod multiplication SOS",
				    BigInt<N>((A * B) % M),
				    (mda::mul<N, sos>(a, b, m)),
				    "Mod Mul: a: {}, b: {}, m: {}", A, B, M);
		}

		constexpr mda::MONT_ALGO cios = mda::MONT_ALGO::CIOS;

		test_assert("mod multiplication CIOS",
			    BigInt<N>((A * B) % M),
			    (mda::mul<N, cios>(a, b, m)),
			    "Mod Mul: a: {}, b: {}, m: {}", A, B, M);

		constexpr mda::MONT_ALGO fios = mda::MONT_ALGO::FIOS;

		test_assert("mod multiplication FIOS",
			    BigInt<N>((A * B) % M),
			    (mda::mul<N, fios>(a, b, m)),
			    "Mod Mul: a: {}, b: {}, m: {}", A, B, M);
	};

	auto run_test_batch = [&]<size_t... Ns>() {
		(run_test.template operator()<Ns>(), ...);
	};

	constexpr const size_t BATCHES = 1000;

	std::println("Running {} tests for all math operations, with chunks of {} bits", BATCHES, STR(BITS_TEST_MOD___N));
	for(size_t i = 0; i < BATCHES; i++) {
		// run_test_batch.template operator()<2, 4, 8, 16, 32, 64>();
		run_test_batch.template operator()<BITS_TEST_MOD___N>();
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

		if(!M_str.empty()) {
			M_str.back() |= 1;
		} else {
			M_str = "1";
		}

		std::string A_str = gen_op();
		std::string B_str = gen_op();

		BigInt<N> m(M_str);
		BigInt<N> a(A_str);
		BigInt<N> b(B_str);

		// (A + B) % M
		test_assert("mod bc addition",
			    run_mod_bc(a, "+", b, m),
			    mda::add(a, b, m),
			    "Mod Add: \na: {}\nb: {}\nm: {}", A_str, B_str, M_str);

		// (A - B) % M
		test_assert("mod bc subtraction",
			    run_mod_bc(a, "-", b, m),
			    mda::sub(a, b, m),
			    "Mod Sub: \na: {}\nb: {}\nm: {}", A_str, B_str, M_str);

		// (A * B) % M
		constexpr mda::MONT_ALGO sos = mda::MONT_ALGO::SOS;
		test_assert("mod bc multiplication SOS",
			    run_mod_bc(a, "*", b, m),
			    (mda::mul<N, sos>(a, b, m)),
			    "Mod Mul: \na: {}\nb: {}\nm: {}", A_str, B_str, M_str);

		constexpr mda::MONT_ALGO cios = mda::MONT_ALGO::CIOS;
		test_assert("mod bc multiplication CIOS",
			    run_mod_bc(a, "*", b, m),
			    (mda::mul<N, cios>(a, b, m)),
			    "Mod Mul: \na: {}\nb: {}\nm: {}", A_str, B_str, M_str);

		constexpr mda::MONT_ALGO fios = mda::MONT_ALGO::FIOS;
		test_assert("mod bc multiplication FIOS",
			    run_mod_bc(a, "*", b, m),
			    (mda::mul<N, fios>(a, b, m)),
			    "Mod Mul: \na: {}\nb: {}\nm: {}", A_str, B_str, M_str);
	};

	auto run_test_batch = [&]<size_t... Ns>() {
		(run_test.template operator()<Ns>(), ...);
	};

	constexpr const size_t BATCHES = 500;

	std::println("Running {} tests for all math operations, with chunks of {} bits", BATCHES, STR(BITS_TEST_MOD__BC));
	for(size_t i = 0; i < BATCHES; i++) {
		// run_test_batch.template operator()<32, 64>();
		run_test_batch.template operator()<BITS_TEST_MOD__BC>();
		progress(i, BATCHES);
	}
	std::println("PASSED ALL MOD BC TESTS{}", std::string(12, ' '));
}
