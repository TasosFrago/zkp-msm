#include <cstdint>
#include <print>

#include "big_arth.hpp"
#include "bigint.hpp"
#include "mod_arth.hpp"

#include "tests/tests.hpp"

int main(int argc, char *argv[])
{
	if(argc > 1 && std::string(argv[1]) == "--serial") {
		TESTS.parallel_exec = false;
	}
	// TESTS.parallel_exec = false;

	// __int128_t a = -8175231853481960273;
	// bga::bgint<4> d(a);
	// std::println("{}, {}", bga::NOT(d), ~a);
	// uint64_t b = 1857413140;
	// std::println("(a & b) {0},\t {0:b}", (a & b));
	// bga::bgint<4> A((a & b));
	// auto c = bga::AND<4>(a, b);
	// std::println("c \t{0},\t {0:b}", c);

	// constexpr size_t BITS = 8;
	// bga::BigInt<BITS> N("1096546981");
	// bga::BigInt<BITS> a("993037165");
	// bga::BigInt<BITS> b("666307479");
	//
	// auto c_sos = mda::mul<BITS, mda::MONT_ALGO::SOS>(a, b, N);
	// auto c_cios = mda::mul<BITS, mda::MONT_ALGO::CIOS>(a, b, N);
	// auto c_fios = mda::mul<BITS, mda::MONT_ALGO::FIOS>(a, b, N);
	//
	// std::println("c_sos {} == c_cios {} == c_fios {}", c_sos, c_cios, c_fios);

	// bga::bgint<2> test("0x3");
	// test.print_all("test");
	// bga::bgint<2> test1("-0x322323");
	// test1.print_all("test1");
	//
	// bga::bgint<32> x(-10);
	// bga::bgint<32> y(2);
	// auto c = bga::mul(x, y);
	// std::println("q: {}", c);
	//
	// bga::bgint<2> x_(-10);
	// bga::bgint<2> y_(2);
	// auto c_ = bga::mul(x_, y_);
	// std::println("q: {}", c_);

	// std::exit(1);

	// test_cmps();
	// test_cmps_with_bc();
	//
	// test_operations();
	// test_operations_with_bc();
	//
	// test_mod();
	// test_mod_with_bc();

	register_cmp_tests();
	register_math_tests();
	register_mod_tests();

	TESTS.run_all();

	// TESTS.print_results();

	std::println("");

	// bga::SelectIntType_dbg::debug_loop<8>();

	return 0;
}
