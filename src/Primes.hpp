#include <cstdlib>

#include "bigint.hpp"

template <size_t Bits>
struct StaticPrimes {

	static inline constexpr bga::BigInt<Bits> primes_list[] = {
#include "primes.h"
	};

	static size_t count()
	{
		return sizeof(primes_list) / sizeof(primes_list[0]);
	}
};
