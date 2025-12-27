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

		test_assert("cmp edge cases", (zero == one), false, "cmp 0 == 1");
		test_assert("cmp edge cases", (one == zero), false, "cmp 1 == 0");
		test_assert("cmp edge cases", (one == one), true, "cmp 1 == 1");
		test_assert("cmp edge cases", (zero == zero), true, "cmp 0 == 0");
		test_assert("cmp edge cases", (zero != one), true, "cmp 0 != 1");
		test_assert("cmp edge cases", (zero < one), true, "cmp 0 < 1");
		test_assert("cmp edge cases", (one > zero), true, "cmp 1 > 0");

		test_assert("cmp edge cases", (zero == large), false, "cmp 0 == large");
		test_assert("cmp edge cases", (large == zero), false, "cmp large == 0");

		BigInt<N> padded_zero(0);
		padded_zero.resize(5, 0);
		test_assert("cmp edge cases", (padded_zero == zero), true, "cmp pad0 == 0");
		test_assert("cmp edge cases", (zero == padded_zero), true, "cmp 0 == pad0");

		BigInt<N> neg_zero(0);
		neg_zero.set_sign(true);
		test_assert("cmp edge cases", (neg_zero == zero), true, "cmp -0 == +0");
		test_assert("cmp edge cases", (zero == neg_zero), true, "cmp +0 == -0");
		test_assert("cmp edge cases", (zero >= neg_zero), true, "cmp +0 >= -0");
		test_assert("cmp edge cases", (zero <= neg_zero), true, "cmp +0 <= -0");

		test_assert("cmp edge cases", (neg_one < one), true, "cmp -1 < 1");
		test_assert("cmp edge cases", (one > neg_one), true, "cmp 1 > -1");
		test_assert("cmp edge cases", (neg_one != one), true, "cmp -1 != 1");

		test_assert("cmp edge cases", (zero > neg_one), true, "cmp 0 > -1");
		test_assert("cmp edge cases", (neg_one < zero), true, "cmp -1 < 0");

		BigInt<N> three(3);
		test_assert("cmp edge cases", (three != large), true, "cmp 3 != large");
		test_assert("cmp edge cases", (three <= large), true, "cmp 3 <= large");
		test_assert("cmp edge cases", (large >= three), true, "cmp large >= 3");
		test_assert("cmp edge cases", (three < large), true, "cmp 3 < large");
		test_assert("cmp edge cases", (large > three), true, "cmp large > 3");

		test_assert("cmp edge cases", (one < two), true, "cmp 1 < 2");
		test_assert("cmp edge cases", (two > one), true, "cmp 2 > 1");
		test_assert("cmp edge cases", (neg_one < neg_two), false, "cmp -1 < -2");
		test_assert("cmp edge cases", (neg_two > neg_one), false, "cmp -2 > -1");
	}
};

struct TestCmpLogic {
	template <size_t N>
	void operator()(std::mt19937_64 &gen) const noexcept
	{
		std::uniform_int_distribution<uint8_t> dist;

		uint64_t A = dist(gen), B = dist(gen);
		bga::BigInt<N> a(A), b(B);

		test_assert("cmp >", A > B, a > b, "a: {}, b: {}", A, B);
		test_assert("cmp <", A < B, a < b, "a: {}, b: {}", A, B);
		test_assert("cmp >=", A >= B, a >= b, "a: {}, b: {}", A, B);
		test_assert("cmp <=", A <= B, a <= b, "a: {}, b: {}", A, B);
		test_assert("cmp ==", A == B, a == b, "a: {}, b: {}", A, B);
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

		test_assert("cmp bc >", run_bc(a, ">", b), (a > b), "a: {}, b: {}", A, B);
		test_assert("cmp bc <", run_bc(a, "<", b), (a < b), "a: {}, b: {}", A, B);
		test_assert("cmp bc >=", run_bc(a, ">=", b), (a >= b), "a: {}, b: {}", A, B);
		test_assert("cmp bc <=", run_bc(a, "<=", b), (a <= b), "a: {}, b: {}", A, B);
		test_assert("cmp bc ==", run_bc(a, "==", b), (a == b), "a: {}, b: {}", A, B);
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

void test_cmps()
{
	// --- Single Chunk Tests (Small Numbers) ---
	std::random_device rd;
	std::mt19937_64 gen(rd());
	std::uniform_int_distribution<uint64_t> dist;

	auto run_test = [&dist, &gen]<size_t N>() {
		uint64_t A = dist(gen), B = dist(gen);
		bga::BigInt<N> a(A), b(B);

		test_assert("cmp >", A > B, a > b, "a: {}, b: {}", A, B);
		test_assert("cmp <", A < B, a < b, "a: {}, b: {}", A, B);
		test_assert("cmp >=", A >= B, a >= b, "a: {}, b: {}", A, B);
		test_assert("cmp <=", A <= B, a <= b, "a: {}, b: {}", A, B);
		test_assert("cmp ==", A == B, a == b, "a: {}, b: {}", A, B);
	};

	auto run_test_batch = [&]<size_t... Ns>() {
		(run_test.template operator()<Ns>(), ...);
	};

	constexpr const size_t BATCHES = 1000;

	std::println("Running {} tests for all math operations, with chunks of {} bits", BATCHES, STR(BITS_TEST_CMP___N));
	for(size_t i = 0; i < BATCHES; i++) {
		// run_test_batch.template operator()<2, 4, 8, 16, 32, 64>();
		run_test_batch.template operator()<BITS_TEST_CMP___N>();
		progress(i, BATCHES);
	}
	std::println("PASSED ALL TESTS{}", std::string(12, ' '));
}

void test_cmps_with_bc()
{
	constexpr const size_t DIGITS = 10;
	auto gen = genRandBgN(DIGITS);

	auto run_test = [&gen]<size_t N>() {
		using namespace bga;

		std::string A = gen(), B = gen();
		BigInt<N> a(A), b(B);

		test_assert("cmp bc >", run_bc(a, ">", b), (a > b), "a: {}, b: {}", A, B);
		test_assert("cmp bc <", run_bc(a, "<", b), (a < b), "a: {}, b: {}", A, B);
		test_assert("cmp bc >=", run_bc(a, ">=", b), (a >= b), "a: {}, b: {}", A, B);
		test_assert("cmp bc <=", run_bc(a, "<=", b), (a <= b), "a: {}, b: {}", A, B);
		test_assert("cmp bc ==", run_bc(a, "==", b), (a == b), "a: {}, b: {}", A, B);
	};

	auto run_test_batch = [&]<size_t... Ns>() {
		(run_test.template operator()<Ns>(), ...);
	};

	constexpr const size_t BATCHES = 1000;

	std::println("Running {} tests for all math operations, with chunks of {} bits", BATCHES, STR(BITS_TEST_CMP__BC));
	for(size_t i = 0; i < BATCHES; i++) {
		// run_test_batch.template operator()<32, 64>();
		run_test_batch.template operator()<BITS_TEST_CMP__BC>();

		progress(i, BATCHES);
	}
	std::println("PASSED ALL TESTS{}", std::string(12, ' '));
}
