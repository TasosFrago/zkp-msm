#include <random>
#include <vector>

#include "bigint.hpp"
#include "mod_arth.hpp"

#include "test_framework.hpp"
#include "tests.hpp"

struct PrimalityTestLogic {
	template <size_t N>
	void operator()(std::mt19937_64 &gen) const noexcept
	{
		std::array<bga::bgint<N>, 11> primes = {
			bga::bgint<N>("602965189904783654649202646197"),
			bga::bgint<N>("247032407684929238000639144081"),
			bga::bgint<N>("112885674055769989164003996667"),
			bga::bgint<N>("473048221264249589418878230921"),
			bga::bgint<N>("683621864741305577030561896879"),
			bga::bgint<N>("424641282493284205631060643503"),
			bga::bgint<N>("402118697095595679513768968233"),
			bga::bgint<N>("760208604702126596303094393403"),
			bga::bgint<N>("813257223982215644597710297561"),
			bga::bgint<N>("271393071376780668733209719689"),
			bga::bgint<N>("172617212725091150802541987123")
		};

		auto &prime_tester = mda::get_primalityTester<N, mda::MONT_ALGO::FIOS>();

		for(auto p : primes) {
			test_assert("Primality test", N,
				    prime_tester.is_prime(p), true,
				    "p: {}", p);
		}
	}
};

void register_primality_tests()
{
	TESTS.register_test<BITS_TEST_MOD___N>(
	    "Primality Tests",
	    1,
	    PrimalityTestLogic{});
}
