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
	W.reserve(a.get_chunks());

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
		W.reserve(x.get_chunks());

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
	W.reserve(a.get_chunks() + b.get_chunks() + 1);
	W.resize(a.get_chunks() + b.get_chunks(), 0);
	// for(size_t i = 0; i < (a.get_chunks() + b.get_chunks()); i++) {
	// 	W.push_bits(0);
	// }

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
BigInt<Bits> lshift(BigInt<Bits> a, size_t shift) /* a << shift */
{
	static_assert(Bits <= 64 && "Can't support larger bits sizes");
	assertm(shift < 65306, "Doesn't support such large number {}", shift);
	if(shift == 0) return a;

	using uint = typename SelectIntType_t<Bits>::uint_t;
	using uintDouble = typename SelectIntType_t<Bits>::uintd_t;

	BigInt<Bits> W;

	size_t chunk_shift = shift / Bits; // How many chunks to shift
	size_t bit_shift = shift % Bits;   // How many bit shifts in a chunk

	W.reserve(a.get_chunks() + chunk_shift + 1);
	W.resize(chunk_shift, 0);

	if(bit_shift == 0) { // Shift only by chunks
		W.insert(W.end(), a.begin(), a.end());
	} else {
		uint carry = 0;
		uintDouble mask = (static_cast<uintDouble>(1) << Bits) - 1;

		for(const auto chunk : a.get()) {
			uintDouble tmp = ((uintDouble)chunk << bit_shift) | carry;

			W.push_bits(tmp & mask);
			carry = (tmp >> Bits);
		}
		if(carry != 0) {
			W.push_bits(carry);
		}
	}
	if(a.is_negative()) W.set_sign(true);

	return W;
}

template <size_t Bits>
BigInt<Bits> rshift(BigInt<Bits> a, size_t shift) /* a >> shift */
{
	static_assert(Bits <= 64 && "Can't support larger bits sizes");
	if(shift == 0) return a;

	BigInt<Bits> W;

	size_t chunk_shift = shift / Bits;
	size_t bit_shift = shift % Bits;

	if(chunk_shift >= a.get_chunks()) return W;

	using uint = typename SelectIntType_t<Bits>::uint_t;
	using uintDouble = typename SelectIntType_t<Bits>::uintd_t;

	uintDouble mask = (static_cast<uintDouble>(1) << Bits) - 1;

	W.reserve(a.get_chunks() - chunk_shift);

	if(bit_shift == 0) {
		for(size_t i = chunk_shift; i < a.get_chunks(); i++) {
			W.push_bits(a.get(i));
		}
	} else {
		for(size_t i = chunk_shift; i < a.get_chunks(); i++) {
			uint lower = a.get(i) >> bit_shift;

			uint upper = 0;
			if(i + 1 < a.get_chunks()) {
				upper = a.get(i + 1) << (Bits - bit_shift);
				upper &= mask;
			}
			W.push_bits(lower | upper);
		}
	}
	if(a.is_negative()) W.set_sign(true);
	return W;
}

} // namespace bga
