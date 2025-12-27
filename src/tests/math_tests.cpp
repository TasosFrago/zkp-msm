#include <cstdint>
#include <print>
#include <random>
#include <string>

#include "big_arth.hpp"
#include "bigint.hpp"

#include "test_framework.hpp"
#include "tests.hpp"

struct TestMathEdgeCases {
	template <size_t N>
	void operator()(std::mt19937_64 &gen) const noexcept
	{
		using namespace bga;

		BigInt<N> zero(0);
		BigInt<N> one(1);
		BigInt<N> two(2);
		BigInt<N> large("0xf00");

		BigInt<N> neg_one(1);
		neg_one.set_sign(true);
		BigInt<N> neg_two(2);
		neg_two.set_sign(true);

		test_assert("math edge cases", zero, add(zero, zero), "0 + 0 = 0");
		test_assert("math edge cases", zero, sub(zero, zero), "0 - 0 = 0");
		test_assert("math edge cases", zero, mul(zero, zero), "0 * 0 = 0");
		test_assert("math edge cases", large, add(large, zero), "large + 0 = 0");
		test_assert("math edge cases", large, sub(large, zero), "large - 0 = 0");
		test_assert("math edge cases", zero, mul(large, zero), "large * 0 = 0");
	}
};

struct TestMathLogic {
	static uint64_t safe_rshift(uint64_t a, uint8_t b) noexcept
	{
		return (b >= sizeof(uint64_t) * 8) ? 0 : (a >> b);
	};

	template <size_t N>
	void operator()(std::mt19937_64 &gen) const noexcept
	{
		std::uniform_int_distribution<uint32_t> dist;

		using namespace bga;
		__int128_t A = dist(gen), B = dist(gen);
		BigInt<N> a(A), b(B);

		test_assert("math addition",
			    BigInt<N>(A + B), add(a, b), "a: {}, b: {}, (A + B) = {}", A, B, (A + B));
		test_assert("math subtraction",
			    BigInt<N>(A - B), sub(a, b), "a: {}, b: {}, (A - B) = {}", A, B, (A - B));
		test_assert("math multiplication",
			    BigInt<N>(A * B), mul(a, b), "a: {}, b: {}, (A * B) = {}", A, B, (A * B));

		// if constexpr(N > 4) {
		__int128_t A_div = (A > B) ? A : B;
		__int128_t B_div = (A > B) ? B : A;
		BigInt<N> a_div(A_div), b_div(B_div);

		auto [div_a_div_b_div_quotient, div_a_div_b_div_remainder] =
		    div(a_div, b_div);

		test_assert("math division (q)",
			    BigInt<N>(A_div / B_div), div_a_div_b_div_quotient, "a: {}, b: {}, (A_div / B_div) = {}", A_div, B_div, (A_div / B_div));
		test_assert("math division (r)",
			    BigInt<N>(A_div % B_div), div_a_div_b_div_remainder, "a: {}, b: {}, (A_div % B_div) = {}", A_div, B_div, (A_div % B_div));
		// }

		test_assert("math rshift",
			    BigInt<N>(safe_rshift(A, (uint8_t)B)), rshift(a, (uint8_t)B), "a: {}, b: {}, chunk {}", A, (uint8_t)B, rshift(a, (uint8_t)B).get_chunks());
		// uint16_t A2 = dist2(gen), B2 = dist2(gen);
		// BigInt<N> a2(A2);
		// test_assert(BigInt<N>((uint64_t)A2 << (uint64_t)B2), lshift(a2, B2), "a: {}, b: {}, (A << B) = {}", A2, B2, ((uint64_t)A2 << (uint64_t)B2));

		// int64_t C = dist2(gen);
		// BigInt<N> c(C);

		using u32 = uint32_t;
		test_assert("math AND",
			    BigInt<N>((u32)A & (u32)B), AND(a, b), "a: {}, b: {}, (A & B) = {}", A, B, (A & B));
		test_assert("math OR",
			    BigInt<N>((u32)A | (u32)B), OR(a, b), "a: {}, b: {}, (A | B) = {}", A, B, (A | B));
		test_assert("math XOR",
			    BigInt<N>((u32)A ^ (u32)B), XOR(a, b), "a: {}, b: {}, (A ^ B) = {}", A, B, (A ^ B));
		test_assert("math NOT",
			    BigInt<N>(~A), NOT_signed(a), "a: {}, (~A) = {}, {:b}", A, (~A), (~A));
	};
};

struct TestMathBCLogic {
	template <size_t N>
	void operator()(std::mt19937_64 &gen) const noexcept
	{
		constexpr const size_t DIGITS = 10;
		auto generator = genRandBgN(DIGITS);
		auto gen2 = genRandBgN(3);
		auto gen3 = genRandBgN(1);

		using namespace bga;
		auto fast_atoi = [](const char *str) {
			uint32_t val = 0;
			while(*str) {
				val = val * 10 + (*str++ - '0');
			}
			return val;
		};

		std::string A = generator(), B = generator();
		BigInt<N> a(A), b(B);

		test_assert("math bc addition",
			    run_bc(a, "+", b), add(a, b), "a: {}, b: {}", A, B);
		test_assert("math bc subtraction",
			    run_bc(a, "-", b), sub(a, b), "a: {}, b: {}", A, B);
		test_assert("math bc multiplication",
			    run_bc(a, "*", b), mul(a, b), "a: {}, b: {}", A, B);

		BigInt<N> &a_div = (a > b) ? a : b;
		BigInt<N> &b_div = (a > b) ? b : a;

		auto [div_a_div_b_div_quotient, div_a_div_b_div_remainder] =
		    div(a_div, b_div);

		test_assert("math bc division (q)",
			    run_bc(a_div, "/", b_div), div_a_div_b_div_quotient, "a: {}, b: {}", a, b);
		test_assert("math bc division (r)",
			    run_bc(a_div, "%", b_div), div_a_div_b_div_remainder, "a: {}, b: {}", a, b);

		std::string A1 = gen2(), B1 = gen2();
		BigInt<N> a1(A1), b1(B1);
		test_assert("math bc lshift",
			    shift_bc(a1, "*", b1), lshift(a1, fast_atoi(B1.c_str())), "a: {}, b: {}", A1, B1);

		std::string B2 = gen3();
		BigInt<N> b2(B2);
		test_assert("math bc rshift",
			    shift_bc(a, "/", b2), rshift(a, fast_atoi(B2.c_str())), "a: {}, b: {}", A, B2);
	};
};

void register_math_tests()
{
	TESTS.register_test<BITS_TEST_MATH__N>(
	    "Math Operations",
	    MATH_BATCHES,
	    TestMathLogic{});

	TESTS.register_test<BITS_TEST_MATH__N>(
	    "Math Edge Cases",
	    1,
	    TestMathEdgeCases{});

	TESTS.register_test<BITS_TEST_MATH_BC>(
	    "Math BC operations",
	    MATH_BC_BATCHES,
	    TestMathBCLogic{});
}

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

		test_assert("math addition",
			    BigInt<N>(A + B), add(a, b), "a: {}, b: {}, (A + B) = {}", A, B, (A + B));
		test_assert("math subtraction",
			    BigInt<N>(A - B), sub(a, b), "a: {}, b: {}, (A - B) = {}", A, B, (A - B));
		test_assert("math multiplication",
			    BigInt<N>(A * B), mul(a, b), "a: {}, b: {}, (A * B) = {}", A, B, (A * B));

		__int128_t A_div = (A > B) ? A : B;
		__int128_t B_div = (A > B) ? B : A;
		BigInt<N> a_div(A_div), b_div(B_div);

		auto [div_a_div_b_div_quotient, div_a_div_b_div_remainder] =
		    div(a_div, b_div);

		test_assert("math division (q)",
			    BigInt<N>(A_div / B_div), div_a_div_b_div_quotient, "a: {}, b: {}, (A_div / B_div) = {}", A_div, B_div, (A_div / B_div));
		test_assert("math division (r)",
			    BigInt<N>(A_div % B_div), div_a_div_b_div_remainder, "a: {}, b: {}, (A_div % B_div) = {}", A_div, B_div, (A_div % B_div));

		auto safe_rshift = [](uint64_t a, uint8_t b) { return (b >= sizeof(uint64_t) * 8) ? 0 : (a >> b); };

		test_assert("math rshift",
			    BigInt<N>(safe_rshift(A, (uint8_t)B)), rshift(a, (uint8_t)B), "a: {}, b: {}, chunk {}", A, (uint8_t)B, rshift(a, (uint8_t)B).get_chunks());
		// uint16_t A2 = dist2(gen), B2 = dist2(gen);
		// BigInt<N> a2(A2);
		// test_assert(BigInt<N>((uint64_t)A2 << (uint64_t)B2), lshift(a2, B2), "a: {}, b: {}, (A << B) = {}", A2, B2, ((uint64_t)A2 << (uint64_t)B2));

		// int64_t C = dist2(gen);
		// BigInt<N> c(C);

		using u32 = uint32_t;
		test_assert("math AND",
			    BigInt<N>((u32)A & (u32)B), AND(a, b), "a: {}, b: {}, (A & B) = {}", A, B, (A & B));
		test_assert("math OR",
			    BigInt<N>((u32)A | (u32)B), OR(a, b), "a: {}, b: {}, (A | B) = {}", A, B, (A | B));
		test_assert("math XOR",
			    BigInt<N>((u32)A ^ (u32)B), XOR(a, b), "a: {}, b: {}, (A ^ B) = {}", A, B, (A ^ B));
		test_assert("math NOT",
			    BigInt<N>(~A), NOT_signed(a), "a: {}, (~A) = {}, {:b}", A, (~A), (~A));
	};

	auto run_test_batch = [&]<size_t... Ns>() {
		(run_test.template operator()<Ns>(), ...);
	};

	constexpr const size_t BATCHES = 1000;

	std::println("Running {} tests for all math operations, with chunks of {} bits", BATCHES, STR(BITS_TEST_MATH__N));
	for(size_t i = 0; i < BATCHES; i++) {
		// run_test_batch.template operator()<2, 4, 8, 16, 32, 64>();
		run_test_batch.template operator()<BITS_TEST_MATH__N>();
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

		test_assert("math bc addition",
			    run_bc(a, "+", b), add(a, b), "a: {}, b: {}", A, B);
		test_assert("math bc subtraction",
			    run_bc(a, "-", b), sub(a, b), "a: {}, b: {}", A, B);
		test_assert("math bc multiplication",
			    run_bc(a, "*", b), mul(a, b), "a: {}, b: {}", A, B);

		BigInt<N> &a_div = (a > b) ? a : b;
		BigInt<N> &b_div = (a > b) ? b : a;

		auto [div_a_div_b_div_quotient, div_a_div_b_div_remainder] =
		    div(a_div, b_div);

		test_assert("math bc division (q)",
			    run_bc(a_div, "/", b_div), div_a_div_b_div_quotient, "a: {}, b: {}", a, b);
		test_assert("math bc division (r)",
			    run_bc(a_div, "%", b_div), div_a_div_b_div_remainder, "a: {}, b: {}", a, b);

		std::string A1 = gen2(), B1 = gen2();
		BigInt<N> a1(A1), b1(B1);
		test_assert("math bc lshift",
			    shift_bc(a1, "*", b1), lshift(a1, fast_atoi(B1.c_str())), "a: {}, b: {}", A1, B1);

		std::string B2 = gen3();
		BigInt<N> b2(B2);
		test_assert("math bc rshift",
			    shift_bc(a, "/", b2), rshift(a, fast_atoi(B2.c_str())), "a: {}, b: {}", A, B2);
	};

	auto run_test_batch = [&]<size_t... Ns>() {
		(run_test.template operator()<Ns>(), ...);
	};

	constexpr const size_t BATCHES = 1000;

	std::println("Running {} tests for all math operations, with chunks of {} bits", BATCHES, STR(BITS_TEST_MATH_BC));
	for(size_t i = 0; i < BATCHES; i++) {
		// run_test_batch.template operator()<32, 64>();
		run_test_batch.template operator()<BITS_TEST_MATH_BC>();

		progress(i, BATCHES);
	}
	std::println("PASSED ALL TESTS{}", std::string(12, ' '));
}
