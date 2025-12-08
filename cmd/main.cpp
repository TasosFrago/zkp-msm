#include <cstdint>
#include <print>

#include "big_arth.hpp"
#include "bigint.hpp"
#include "mod_arth.hpp"

#include "tests.hpp"

int main(int argc, char *argv[])
{

	bga::BigInt<64> N("987654321");
	bga::BigInt<64> a("1234567");
	bga::BigInt<64> b("9876543");

	auto c_sos = mda::mul<64, mda::MONT_ALGO::SOS>(a, b, N);
	auto c_cios = mda::mul<64, mda::MONT_ALGO::CIOS>(a, b, N);

	std::println("c_sos {} == c_cios {}", c_sos, c_cios);

	test_cmps();
	test_cmps_with_bc();

	test_operations();
	test_operations_with_bc();

	test_mod();
	test_mod_with_bc();

	std::println("");

	// bga::SelectIntType_dbg::debug_loop<8>();

	return 0;
}
