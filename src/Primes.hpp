#include <cstdlib>
#include <string>

#include "bigint.hpp"

template <size_t Bits>
struct StaticPrimes {

	static inline const bga::BigInt<Bits> primes_list[] = {
#include "primes.h"
	};

	static inline constexpr std::string_view raw_literals[] = {
#include "primes.h"
	};

	static constexpr size_t count()
	{
		return std::size(primes_list);
		// return sizeof(primes_list) / sizeof(primes_list[0]);
	}

	static constexpr size_t get_length(size_t i)
	{
		return raw_literals[i].size();
	}
};
