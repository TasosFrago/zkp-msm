#include <expected>
#include <print>

#include "big_arth.hpp"
#include "bigint.hpp"
#include "tests.hpp"

int main(int argc, char *argv[])
{
	test_cmps();
	test_cmps_with_bc();
	test_operations();
	test_operations_with_bc();

	std::println("");

	// bga::SelectIntType_dbg::debug_loop<8>();

	return 0;
}
