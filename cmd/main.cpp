#include <print>

#include "bigint.hpp"
#include "tests.hpp"

int main(int argc, char *argv[])
{

	test_cmps();
	test_operations();

	// bga::SelectIntType_dbg::debug_loop<8>();

	std::println("");

	return 0;
}
