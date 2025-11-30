#pragma once

#include "big_arth.hpp"
#include "bigint.hpp"
#include <utility>

namespace mda
{
using bga::BigInt;

template <size_t Bits>
BigInt<Bits> mod(BigInt<Bits> a, BigInt<Bits> q)
{
	static_assert(Bits <= 64 && "Can't support larger bits sizes");
	assertm(!q.is_negative(), "q can't be negative");

	if(a.is_negative()) {
		a.set_sign(false);

		BigInt<Bits> rem = mod(a, q);

		if(rem == 0) return rem;

		return bga::sub(q, rem);
	}

	while(a >= q) {
		a = bga::sub(a, q);
	}
	return a;
}

template <size_t Bits>
BigInt<Bits> add(BigInt<Bits> a, BigInt<Bits> b, BigInt<Bits> q)
{
	static_assert(Bits <= 64 && "Can't support larger bits sizes");
	assert(a < q && b < q && "a and b need to be smaller than q");

	BigInt<Bits> W = bga::add(a, b);

	if(W >= q) {
		W = bga::sub(W, q);
	}

	assert(W < q && "Result should be smaller than q");
	return W;
}

template <size_t Bits>
BigInt<Bits> sub(BigInt<Bits> a, BigInt<Bits> b, BigInt<Bits> q)
{
	static_assert(Bits <= 64 && "Can't support larger bits sizes");
	assert(a < q && b < q && "a and b need to be smaller than q");
	using bga::add;
	using bga::sub;

	if(a >= b) {
		return sub(a, b);
	} else if(a < b) {
		return sub(add(a, q), b);
	}
	std::unreachable();
}

} // namespace mda
