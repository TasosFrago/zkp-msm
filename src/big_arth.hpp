#pragma once

#include <cassert>
#include <cstddef>
#include <print>

#include "bigint.hpp"

namespace bga
{

template <size_t Bits>
BigInt<Bits> add(BigInt<Bits> a, BigInt<Bits> b)
{
	static_assert(Bits <= 64 && "Can't support larger bits sizes");
	assert(!a.is_negative() && !b.is_negative() && "Can't do addition with negative numbers");
	using uintDouble = typename SelectIntType_t<Bits>::uintd_t;

	pad_BigInts<Bits>(a, b);

	BigInt<Bits> W;

	uintDouble carry = 0;
	uintDouble mask = (static_cast<uintDouble>(1) << Bits) - 1;

	for(size_t i = 0; i < a.get_chunks(); i++) {
		uintDouble tmp = (uintDouble)a.get(i) + (uintDouble)b.get(i) + carry;
		W.push_bits(tmp & mask);
		carry = (tmp >> Bits);
	}

	if(carry != 0) W.push_bits(carry);
	return W;
}

template <size_t Bits>
BigInt<Bits> sub(BigInt<Bits> a, BigInt<Bits> b)
{
	static_assert(Bits <= 64 && "Can't support larger bits sizes");
	using uintDouble = typename SelectIntType_t<Bits>::uintd_t;
	using intDouble = typename SelectIntType_t<Bits>::intd_t;

	pad_BigInts<Bits>(a, b);

	uintDouble mask = (static_cast<uintDouble>(1) << Bits) - 1;

	auto subtract = [mask](BigInt<Bits> &x, BigInt<Bits> &y) -> std::pair<BigInt<Bits>, intDouble> {
		intDouble carry = 0;
		BigInt<Bits> W;

		for(size_t i = 0; i < x.get_chunks(); i++) {
			intDouble diff = (intDouble)x.get(i) - (intDouble)y.get(i) + carry;
			W.push_bits(diff & mask);

			carry = ((intDouble)diff < 0) ? -1 : 0;
		}
		return std::make_pair(W, carry);
	};

	auto [W, carry] = subtract(a, b);

	if(carry == -1) {
		BigInt<Bits> zero;
		pad_BigInts<Bits>(zero, W);
		auto [result, final_c] = subtract(zero, W);
		result.set_sign(true);

		return result;
	}

	return W;
}

template <size_t Bits>
BigInt<Bits> mul(BigInt<Bits> a, BigInt<Bits> b)
{
	static_assert(Bits <= 64 && "Can't support larger bits sizes");
	using uint = typename SelectIntType_t<Bits>::uint_t;
	using uintDouble = typename SelectIntType_t<Bits>::uintd_t;

	uintDouble mask = (static_cast<uintDouble>(1) << Bits) - 1;

	BigInt<Bits> W;
	for(size_t i = 0; i < (a.get_chunks() + b.get_chunks()); i++) {
		W.push_bits(0);
	}

	for(size_t i = 0; i < b.get_chunks(); i++) {
		uint carry = 0;
		for(size_t j = 0; j < a.get_chunks(); j++) {
			uintDouble tmp = (uintDouble)W.get(i + j) + ((uintDouble)a.get(j) * (uintDouble)b.get(i)) + (uintDouble)carry;
			W.push_bits((uint)(tmp & mask), i + j); // Lower part of mul
			carry = (uint)(tmp >> Bits);		// Upper part of mul
		}
		W.push_bits(carry, i + a.get_chunks());
	}

	return W;
}

template <size_t Bits>
bool cmp_gt(BigInt<Bits> a, BigInt<Bits> b) /* a > b */
{
	static_assert(Bits <= 64 && "Can't support larger bits sizes");
	assert(!a.is_negative() && !b.is_negative() && "Numbers need to be positive");
	using uint = typename SelectIntType_t<Bits>::uint_t;

	if(a.get_chunks() > b.get_chunks() &&
	   a.get(a.get_chunks() - 1) != 0 &&
	   b.get(b.get_chunks() - 1) != 0) {
		return true;
	} else if(a.get_chunks() < b.get_chunks() &&
		  a.get(a.get_chunks() - 1) != 0 &&
		  b.get(b.get_chunks() - 1) != 0) {
		return false;
	}
#ifndef NDEBUG
	using uintDouble = typename SelectIntType_t<Bits>::uintd_t;
#endif
	for(int64_t i = a.get_chunks() - 1; i >= 0; i--) {
		assert(((uintDouble)a.get(i) >> Bits) == 0 &&
		       ((uintDouble)b.get(i) >> Bits) == 0 &&
		       "The part outside the number is non zero");
		uint A = a.get(i);
		uint B = b.get(i);
		if(A == 0 || B == 0) continue;
		if(A > B) {
			return true;
		} else if(A < B) {
			return false;
		}
	}
	return false;
}

template <size_t Bits>
bool cmp_lt(BigInt<Bits> a, BigInt<Bits> b) /* a < b */
{
	static_assert(Bits <= 64 && "Can't support larger bits sizes");
	assert(!a.is_negative() && !b.is_negative() && "Numbers need to be positive");

	if(a.get_chunks() > b.get_chunks()) {
		return false;
	} else if(a.get_chunks() < b.get_chunks()) {
		return true;
	}
#ifndef NDEBUG
	using uintDouble = typename SelectIntType_t<Bits>::uintd_t;
#endif
	for(int64_t i = a.get_chunks() - 1; i >= 0; i--) {
		assert(((uintDouble)a.get(i) >> Bits) == 0 &&
		       ((uintDouble)b.get(i) >> Bits) == 0 &&
		       "The part outside the number is non zero");
		if(a.get(i) > b.get(i)) {
			return false;
		} else if(a.get(i) < b.get(i)) {
			return true;
		}
	}
	return false;
}

template <size_t Bits>
bool cmp_gt_eq(BigInt<Bits> a, BigInt<Bits> b) /* a >= b */
{
	static_assert(Bits <= 64 && "Can't support larger bits sizes");
	assert(!a.is_negative() && !b.is_negative() && "Numbers need to be positive");

	if(a.get_chunks() > b.get_chunks()) {
		return true;
	} else if(a.get_chunks() < b.get_chunks()) {
		return false;
	}
#ifndef NDEBUG
	using uintDouble = typename SelectIntType_t<Bits>::uintd_t;
#endif
	for(int64_t i = a.get_chunks() - 1; i >= 0; i--) {
		assert(((uintDouble)a.get(i) >> Bits) == 0 &&
		       ((uintDouble)b.get(i) >> Bits) == 0 &&
		       "The part outside the number is non zero");

		if(a.get(i) > b.get(i)) {
			return true;
		} else if(a.get(i) < b.get(i)) {
			return false;
		}
	}
	return true;
}

template <size_t Bits>
bool cmp_lt_eq(BigInt<Bits> a, BigInt<Bits> b) /* a <= b */
{
	static_assert(Bits <= 64 && "Can't support larger bits sizes");
	assert(!a.is_negative() && !b.is_negative() && "Numbers need to be positive");

	if(a.get_chunks() > b.get_chunks()) {
		return false;
	} else if(a.get_chunks() < b.get_chunks()) {
		return true;
	}
#ifndef NDEBUG
	using uintDouble = typename SelectIntType_t<Bits>::uintd_t;
#endif
	for(int64_t i = a.get_chunks() - 1; i >= 0; i--) {
		assert(((uintDouble)a.get(i) >> Bits) == 0 &&
		       ((uintDouble)b.get(i) >> Bits) == 0 &&
		       "The part outside the number is non zero");
		if(a.get(i) > b.get(i)) {
			return false;
		} else if(a.get(i) < b.get(i)) {
			return true;
		}
	}
	return true;
}

template <size_t Bits>
bool cmp_eq(BigInt<Bits> a, BigInt<Bits> b) /* a == b */
{
	static_assert(Bits <= 64 && "Can't support larger bits sizes");
	assert(!a.is_negative() && !b.is_negative() && "Numbers need to be positive");

	if(a.get_chunks() != b.get_chunks()) return false;

#ifndef NDEBUG
	using uintDouble = typename SelectIntType_t<Bits>::uintd_t;
#endif
	for(int64_t i = a.get_chunks() - 1; i >= 0; i--) {
		assert(((uintDouble)a.get(i) >> Bits) == 0 &&
		       ((uintDouble)b.get(i) >> Bits) == 0 &&
		       "The part outside the number is non zero");

		if(a.get(i) != b.get(i)) {
			return false;
		}
	}
	return true;
}

} // namespace bga
