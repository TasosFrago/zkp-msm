#include <csignal>
#include <cstdint>
#include <print>
#include <unistd.h>

#include "big_arth.hpp"
#include "bigint.hpp"
#include "ecc.hpp"
#include "mod_arth.hpp"

#include "mda_primetest.hpp"

#include "tests/tests.hpp"
#include "tests/utils/helpers.hpp"

int main(int argc, char *argv[])
{
	// FIX: Test the is_prime<2>("17")
	// FIX: Check the error_important.log
	std::signal(SIGINT, [](int signal) {
		const char msg[] = "\n\033[?25h\033[0m\n*** TEST RUN INTERRUPTED ***\n";
		write(STDOUT_FILENO, msg, sizeof(msg) - 1);
		_Exit(128 + signal);
	});
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

	// constexpr size_t BITS = 32;
	// bga::bgint<BITS> N("1000000007");
	// bga::bgint<BITS> a("143382731");
	//
	// auto a_inv = mda::inverse(a, N);
	// std::println("a_inv {}", a_inv);

	// constexpr size_t BITS = 32;
	// bga::BigInt<BITS> N("1096546981");
	// bga::BigInt<BITS> a("993037165");
	// bga::BigInt<BITS> b("-666307479");
	//
	// bga::bgint<BITS> c_sos, c_cios, c_fios;
	//
	// auto sos_time = measure_time([&]() {
	// 	c_sos = mda::mul<BITS, mda::MONT_ALGO::SOS, true>(a, b, N);
	// });
	//
	// auto cios_time = measure_time([&]() {
	// 	c_cios = mda::mul<BITS, mda::MONT_ALGO::CIOS, true>(a, b, N);
	// });
	//
	// auto fios_time = measure_time([&]() {
	// 	c_fios = mda::mul<BITS, mda::MONT_ALGO::FIOS, true>(a, b, N);
	// });
	//
	// // auto c_sos = mda::mul<BITS, mda::MONT_ALGO::SOS>(a, b, N);
	// // auto c_cios = mda::mul<BITS, mda::MONT_ALGO::CIOS>(a, b, N);
	// // auto c_fios = mda::mul<BITS, mda::MONT_ALGO::FIOS>(a, b, N);
	//
	// std::println("c_sos {} == c_cios {} == c_fios {}", c_sos, c_cios, c_fios);
	// std::println("t: {},\t {}, \t {}", sos_time, cios_time, fios_time);

	// {
	// 	bga::bgint<2> n(17);
	// 	bga::bgint<2> a(5);
	// 	bga::bgint<2> b(1);
	//
	// 	mda::Montgomery<2, mda::MONT_ALGO::FIOS> mont(n);
	// 	auto res_a = mont.init(a);
	// 	auto res_b = mont.init(b);
	// 	auto res_c = mont.mul(res_a, res_b);
	// 	std::println("mont.r2 {} a_m {}, b_m {}, c_m {}, c {}", mont.r2, res_a, res_b, res_c, mont.trans_back(res_c));
	//
	// 	bga::bgint<32> N(17);
	// 	bga::bgint<32> A(5);
	// 	bga::bgint<32> B(1);
	//
	// 	mda::Montgomery<32, mda::MONT_ALGO::FIOS> Mont(N);
	// 	auto res_A = Mont.init(A);
	// 	auto res_B = Mont.init(B);
	// 	auto res_C = Mont.mul(res_A, res_B);
	// 	std::println("Mont.r2 {} A_m {}, B_m {}, C_m {}, C {}", Mont.r2, res_A, res_B, res_C, Mont.trans_back(res_C));
	// }
	//
	// constexpr size_t B = 2;
	// bga::bgint<B> a(2), b(2), p(17);
	//
	// ecc::ShortWeierstrassCurve<B> small_curve(a, b, p);
	// ecc::AffinePoint<B> P(bga::bgint<B>(5), bga::bgint<B>(1));
	//
	// auto P2 = small_curve.dbl(P);
	// std::println("P ({}, {}), expected (6, 3) got -> P2 ({}, {})", P.x, P.y, P2.x, P2.y);

	// std::exit(1);

	TESTS.error_log_file = "error.log";

	// register_primality_tests();

	// register_cmp_tests();
	// register_math_tests();
	// register_mod_tests();
	register_ecc_tests();

	TESTS.run_all();

	std::println("");

	// bga::SelectIntType_dbg::debug_loop<8>();

	return 0;
}
