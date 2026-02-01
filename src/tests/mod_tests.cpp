#include <random>

#include "bigint.hpp"
#include "mod_arth.hpp"

#include "test_framework.hpp"
#include "tests.hpp"

struct TestModLogic {
	constexpr static mda::MONT_ALGO sos = mda::MONT_ALGO::SOS;
	constexpr static mda::MONT_ALGO cios = mda::MONT_ALGO::CIOS;
	constexpr static mda::MONT_ALGO fios = mda::MONT_ALGO::FIOS;

	static __int128_t power_ref(__int128_t base, __int128_t exp, __int128_t mod)
	{
		__int128_t res = 1;
		base %= mod;
		while(exp > 0) {
			if(exp % 2 == 1) {
				res = (res * base) % mod;
			}
			base = (base * base) % mod;
			exp /= 2;
		}
		return res;
	}

	static __int128_t mod_inv(__int128_t n, __int128_t mod)
	{
		return power_ref(n, mod - 2, mod);
	}

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
		test_assert("mod addition", N,
			    BigInt<N>((A + B) % M),
			    mda::add(a, b, m),
			    "Mod Add: a: {}, b: {}, m: {}", A, B, M);

		// Modular Subtraction
		// C++ Check: ((A - B) % M + M) % M (Euclidean Modulo)
		__int128_t sub_expected = ((A - B) % M + M) % M;
		test_assert("mod subtraction", N,
			    BigInt<N>(sub_expected),
			    mda::sub(a, b, m),
			    "Mod Sub: a: {}, b: {}, m: {}", A, B, M);

		// Modular Multiplication
		// C++ Check: (A * B) % M
		test_assert("mod multiplication SOS", N,
			    BigInt<N>((A * B) % M),
			    (mda::mul<N, sos>(a, b, m)),
			    "Mod Mul: a: {}, b: {}, m: {}", A, B, M);

		test_assert("mod multiplication CIOS", N,
			    BigInt<N>((A * B) % M),
			    (mda::mul<N, cios>(a, b, m)),
			    "Mod Mul: a: {}, b: {}, m: {}", A, B, M);

		test_assert("mod multiplication FIOS", N,
			    BigInt<N>((A * B) % M),
			    (mda::mul<N, fios>(a, b, m)),
			    "Mod Mul: a: {}, b: {}, m: {}", A, B, M);

		// Modular Exponentiation
		// test_assert("mod exponentiation with SOS", N,
		// 	    (BigInt<N>(power_ref(A, B, M))),
		// 	    (mda::pow<N, sos>(a, b, m)),
		// 	    "Mod Pow: base {}, exp: {}, m: {}", A, B, M);
		//
		// test_assert("mod exponentiation with CIOS", N,
		// 	    (BigInt<N>(power_ref(A, B, M))),
		// 	    (mda::pow<N, cios>(a, b, m)),
		// 	    "Mod Pow: base {}, exp: {}, m: {}", A, B, M);

		test_assert("mod exponentiation with FIOS", N,
			    (BigInt<N>(power_ref(A, B, M))),
			    (mda::pow<N, fios>(a, b, m)),
			    "Mod Pow: base {}, exp: {}, m: {}", A, B, M);

		// TODO: Implement generate prime function and replace hardcoded
		uint64_t P = 1000000007ULL;
		// uint64_t P = 10007;
		__int128_t A_p = A + 1;

		BigInt<N> a_p(A_p);
		BigInt<N> p_bg(P);

		// Modular Inverse
		// test_assert("mod inverse with SOS", N,
		// 	    BigInt<N>((1 / (A_p + 1)) % P),
		// 	    (mda::inverse<N, sos>(a_p, p_bg)),
		// 	    "Mod inv: A: {}, m: {}", A_p, P);
		//
		// test_assert("mod inverse with CIOS", N,
		// 	    BigInt<N>((1 / (A_p + 1)) % P),
		// 	    (mda::inverse<N, cios>(a_p, p_bg)),
		// 	    "Mod inv: A: {}, m: {}", A_p, P);

		test_assert("mod inverse with FIOS", N,
			    BigInt<N>(mod_inv(A_p, P)),
			    (mda::inverse<N, fios>(a_p, p_bg)),
			    "Mod inv: A: {}, m: {}", A_p, P);
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
		test_assert("mod bc addition", N,
			    run_mod_bc(a, "+", b, m),
			    mda::add(a, b, m),
			    "Mod Add: \na: {}\nb: {}\nm: {}", A_str, B_str, M_str);

		// (A - B) % M
		test_assert("mod bc subtraction", N,
			    run_mod_bc(a, "-", b, m),
			    mda::sub(a, b, m),
			    "Mod Sub: \na: {}\nb: {}\nm: {}", A_str, B_str, M_str);

		// (A * B) % M
		test_assert("mod bc multiplication SOS", N,
			    run_mod_bc(a, "*", b, m),
			    (mda::mul<N, sos>(a, b, m)),
			    "Mod Mul: \na: {}\nb: {}\nm: {}", A_str, B_str, M_str);

		test_assert("mod bc multiplication CIOS", N,
			    run_mod_bc(a, "*", b, m),
			    (mda::mul<N, cios>(a, b, m)),
			    "Mod Mul: \na: {}\nb: {}\nm: {}", A_str, B_str, M_str);

		test_assert("mod bc multiplication FIOS", N,
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
