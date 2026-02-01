#include <print>
#include <random>

#include "bigint.hpp"

#include "test_framework.hpp"
#include "tests.hpp"

struct TestCmpEdgeCases {
	template <size_t N>
	void operator()(std::mt19937_64 &gen) const noexcept
	{
		using namespace bga;

		BigInt<N> zero(0);
		BigInt<N> one(1);
		BigInt<N> two(2);

		BigInt<N> neg_one(1);
		neg_one.set_sign(true);
		BigInt<N> neg_two(2);
		neg_two.set_sign(true);

		BigInt<N> large("0xFFFFFFFFF");

		test_assert("cmp edge cases", N, (zero == one), false, "cmp 0 == 1");
		test_assert("cmp edge cases", N, (one == zero), false, "cmp 1 == 0");
		test_assert("cmp edge cases", N, (one == one), true, "cmp 1 == 1");
		test_assert("cmp edge cases", N, (zero == zero), true, "cmp 0 == 0");
		test_assert("cmp edge cases", N, (zero != one), true, "cmp 0 != 1");
		test_assert("cmp edge cases", N, (zero < one), true, "cmp 0 < 1");
		test_assert("cmp edge cases", N, (one > zero), true, "cmp 1 > 0");

		test_assert("cmp edge cases", N, (zero == large), false, "cmp 0 == large");
		test_assert("cmp edge cases", N, (large == zero), false, "cmp large == 0");

		BigInt<N> padded_zero(0);
		padded_zero.resize(5, 0);
		test_assert("cmp edge cases", N, (padded_zero == zero), true, "cmp pad0 == 0");
		test_assert("cmp edge cases", N, (zero == padded_zero), true, "cmp 0 == pad0");

		BigInt<N> neg_zero(0);
		neg_zero.set_sign(true);
		test_assert("cmp edge cases", N, (neg_zero == zero), true, "cmp -0 == +0");
		test_assert("cmp edge cases", N, (zero == neg_zero), true, "cmp +0 == -0");
		test_assert("cmp edge cases", N, (zero >= neg_zero), true, "cmp +0 >= -0");
		test_assert("cmp edge cases", N, (zero <= neg_zero), true, "cmp +0 <= -0");

		test_assert("cmp edge cases", N, (neg_one < one), true, "cmp -1 < 1");
		test_assert("cmp edge cases", N, (one > neg_one), true, "cmp 1 > -1");
		test_assert("cmp edge cases", N, (neg_one != one), true, "cmp -1 != 1");

		test_assert("cmp edge cases", N, (zero > neg_one), true, "cmp 0 > -1");
		test_assert("cmp edge cases", N, (neg_one < zero), true, "cmp -1 < 0");

		BigInt<N> three(3);
		test_assert("cmp edge cases", N, (three != large), true, "cmp 3 != large");
		test_assert("cmp edge cases", N, (three <= large), true, "cmp 3 <= large");
		test_assert("cmp edge cases", N, (large >= three), true, "cmp large >= 3");
		test_assert("cmp edge cases", N, (three < large), true, "cmp 3 < large");
		test_assert("cmp edge cases", N, (large > three), true, "cmp large > 3");

		test_assert("cmp edge cases", N, (one < two), true, "cmp 1 < 2");
		test_assert("cmp edge cases", N, (two > one), true, "cmp 2 > 1");
		test_assert("cmp edge cases", N, (neg_one < neg_two), false, "cmp -1 < -2");
		test_assert("cmp edge cases", N, (neg_two > neg_one), false, "cmp -2 > -1");
	}
};

struct TestCmpLogic {
	template <size_t N>
	void operator()(std::mt19937_64 &gen) const noexcept
	{
		std::uniform_int_distribution<uint8_t> dist;

		uint64_t A = dist(gen), B = dist(gen);
		bga::BigInt<N> a(A), b(B);

		test_assert("cmp >", N, A > B, a > b, "a: {}, b: {}", A, B);
		test_assert("cmp <", N, A < B, a < b, "a: {}, b: {}", A, B);
		test_assert("cmp >=", N, A >= B, a >= b, "a: {}, b: {}", A, B);
		test_assert("cmp <=", N, A <= B, a <= b, "a: {}, b: {}", A, B);
		test_assert("cmp ==", N, A == B, a == b, "a: {}, b: {}", A, B);
	};
};

struct TestCmpBCLogic {
	constexpr static const size_t DIGITS = 10;

	template <size_t N>
	void operator()(std::mt19937_64 &gen) const noexcept
	{
		auto generator = genRandBgN(DIGITS);

		using namespace bga;

		std::string A = generator(), B = generator();
		BigInt<N> a(A), b(B);

		test_assert("cmp bc >", N, run_bc(a, ">", b), (a > b), "a: {}, b: {}", A, B);
		test_assert("cmp bc <", N, run_bc(a, "<", b), (a < b), "a: {}, b: {}", A, B);
		test_assert("cmp bc >=", N, run_bc(a, ">=", b), (a >= b), "a: {}, b: {}", A, B);
		test_assert("cmp bc <=", N, run_bc(a, "<=", b), (a <= b), "a: {}, b: {}", A, B);
		test_assert("cmp bc ==", N, run_bc(a, "==", b), (a == b), "a: {}, b: {}", A, B);
	};
};

void register_cmp_tests()
{
	TESTS.register_test<BITS_TEST_CMP___N>(
	    "Comparison operations",
	    CMP_BATCHES,
	    TestCmpLogic{});

	TESTS.register_test<BITS_TEST_CMP___N>(
	    "Comparison edge cases",
	    1,
	    TestCmpEdgeCases{});

	TESTS.register_test<BITS_TEST_CMP__BC>(
	    "Comparison BC operations",
	    CMP_BC_BATCHES,
	    TestCmpBCLogic{});
}
