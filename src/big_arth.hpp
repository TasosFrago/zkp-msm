#pragma once

/**
 * @file big_arth.hpp
 * @brief Arithmetic and bitwise operations for the bga::BigInt class.
 */

#include <cassert>
#include <cstddef>
#include <cstdint>

#include "bigint.hpp"

namespace bga
{

template <size_t Bits>
int cmp_mag(const BigInt<Bits> &a, const BigInt<Bits> &b)
{
	// -1: |a| <  |b|
	// 0:  |a| == |b|
	// 1:  |a| >  |b|
	size_t a_chunks = a.effective_size();
	size_t b_chunks = b.effective_size();

	if(a_chunks > b_chunks) return 1;
	if(a_chunks < b_chunks) return -1;

	assertm(a_chunks == b_chunks, "Chunk sizes are not the same");

	for(int64_t i = a_chunks - 1; i >= 0; i--) {
		auto a_val = a.get(i);
		auto b_val = b.get(i);
		if(a_val > b_val) return 1;
		if(a_val < b_val) return -1;
	}
	return 0;
}

template <size_t Bits>
BigInt<Bits> add_mag(const BigInt<Bits> &a, const BigInt<Bits> &b)
{
	static_assert(Bits <= 64 && "Can't support larger bits sizes");
	// assert(!a.is_negative() && !b.is_negative() && "Can't do addition with negative numbers");
	using uintDouble = typename SelectIntType_t<Bits>::uintd_t;

	size_t size = (a.get_chunks() > b.get_chunks()) ? a.get_chunks() : b.get_chunks();

	BigInt<Bits> W;
	W.reserve(size + 1);

	uintDouble carry = 0;
	constexpr uintDouble mask = (static_cast<uintDouble>(1) << Bits) - 1;

	for(size_t i = 0; i < size; i++) {
		uintDouble tmp = (uintDouble)a.get_safe(i) + (uintDouble)b.get_safe(i) + carry;
		W.push_bits(tmp & mask);
		carry = (tmp >> Bits);
	}

	if(carry != 0) W.push_bits(carry);
	return W;
}

template <size_t Bits>
BigInt<Bits> sub_mag(const BigInt<Bits> &a, const BigInt<Bits> &b)
{
	using uintDouble = typename SelectIntType_t<Bits>::uintd_t;
	using intDouble = typename SelectIntType_t<Bits>::intd_t;

	constexpr uintDouble mask = (static_cast<uintDouble>(1) << Bits) - 1;

	BigInt<Bits> W;
	W.reserve(a.get_chunks());

	intDouble carry = 0;
	size_t size = a.get_chunks();

	for(size_t i = 0; i < size; i++) {
		intDouble diff = (intDouble)a.get_safe(i) - (intDouble)b.get_safe(i) + carry;
		W.push_bits(diff & mask);

		carry = (diff < 0) ? -1 : 0;
	}

	W.trim();
	return W;
}

template <size_t Bits>
BigInt<Bits> add(const BigInt<Bits> &a, const BigInt<Bits> &b)
{
	static_assert(Bits <= 64 && "Can't support larger bits sizes");

	auto a_sign = a.is_negative();
	auto b_sign = b.is_negative();

	if(a_sign == b_sign) [[likely]] {
		BigInt<Bits> res = add_mag(a, b);
		res.set_sign(a_sign);
		return res;
	}

	int comparison = cmp_mag(a, b);

	if(comparison == 0) {
		return BigInt<Bits>(0);
	}

	BigInt<Bits> res;
	if(comparison > 0) {
		res = sub_mag(a, b);
		res.set_sign(a_sign);
	} else {
		res = sub_mag(b, a);
		res.set_sign(b_sign);
	}
	return res;
}

template <size_t Bits>
BigInt<Bits> sub(const BigInt<Bits> &a, const BigInt<Bits> &b)
{
	static_assert(Bits <= 64 && "Can't support larger bits sizes");

	auto a_sign = a.is_negative();
	auto b_sign = b.is_negative();

	if(a_sign != b_sign) [[unlikely]] {
		BigInt<Bits> res = add_mag(a, b);
		res.set_sign(a_sign);
		return res;
	}

	int comparison = cmp_mag(a, b);

	if(comparison == 0) {
		return BigInt<Bits>(0);
	}

	BigInt<Bits> res;
	if(comparison > 0) {
		res = sub_mag(a, b);
		res.set_sign(a_sign);
	} else {
		res = sub_mag(b, a);
		res.set_sign(!b_sign);
	}

	return res;
}

/**
 * @brief Performs subtraction of two BigInts.
 * @tparam Bits Number of bits per chunk.
 * @param a The first operand (minuend).
 * @param b The second operand (subtrahend).
 * @return A new BigInt representing (a - b).
 * @note Correctly handles signs and negative results using borrow logic.
 */
// template <size_t Bits>
// BigInt<Bits> sub(BigInt<Bits> a, BigInt<Bits> b)
// {
// 	static_assert(Bits <= 64 && "Can't support larger bits sizes");
// 	using uintDouble = typename SelectIntType_t<Bits>::uintd_t;
// 	using intDouble = typename SelectIntType_t<Bits>::intd_t;
//
// 	uintDouble mask = (static_cast<uintDouble>(1) << Bits) - 1;
//
// 	auto subtract = [mask](BigInt<Bits> &x, BigInt<Bits> &y) -> std::pair<BigInt<Bits>, intDouble> {
// 		intDouble carry = 0;
// 		BigInt<Bits> W;
// 		W.reserve(x.get_chunks());
//
// 		size_t size = (x.get_chunks() > y.get_chunks()) ? x.get_chunks() : y.get_chunks();
//
// 		for(size_t i = 0; i < size; i++) {
// 			intDouble diff = (intDouble)x.get_safe(i) - (intDouble)y.get_safe(i) + carry;
// 			W.push_bits(diff & mask);
//
// 			carry = ((intDouble)diff < 0) ? -1 : 0;
// 		}
// 		return { W, carry };
// 	};
//
// 	auto [W, carry] = subtract(a, b);
//
// 	if(carry == -1) {
// 		BigInt<Bits> zero;
// 		auto [result, final_c] = subtract(zero, W);
// 		result.set_sign(true);
//
// 		return result;
// 	}
// 	W.trim();
//
// 	return W;
// }

/**
 * @brief Performs addition of two non-negative BigInts.
 * @tparam Bits Number of bits per chunk.
 * @param a The first operand.
 * @param b The second operand.
 * @return A new BigInt representing the sum of a and b.
 * @note This function asserts that both inputs are non-negative.
 */
// template <size_t Bits>
// BigInt<Bits> add(const BigInt<Bits> &a, const BigInt<Bits> &b)
// {
// 	static_assert(Bits <= 64 && "Can't support larger bits sizes");
// 	assert(!a.is_negative() && !b.is_negative() && "Can't do addition with negative numbers");
// 	using uintDouble = typename SelectIntType_t<Bits>::uintd_t;
//
// 	size_t size = (a.get_chunks() > b.get_chunks()) ? a.get_chunks() : b.get_chunks();
//
// 	BigInt<Bits> W;
// 	W.reserve(size + 1);
//
// 	uintDouble carry = 0;
// 	constexpr uintDouble mask = (static_cast<uintDouble>(1) << Bits) - 1;
//
// 	for(size_t i = 0; i < size; i++) {
// 		uintDouble tmp = (uintDouble)a.get_safe(i) + (uintDouble)b.get_safe(i) + carry;
// 		W.push_bits(tmp & mask);
// 		carry = (tmp >> Bits);
// 	}
//
// 	if(carry != 0) W.push_bits(carry);
// 	return W;
// }

/**
 * @brief Performs multiplication of two BigInts.
 * @tparam Bits Number of bits per chunk.
 * @param a The first factor.
 * @param b The second factor.
 * @return A new BigInt representing the product of a and b.
 */
template <size_t Bits>
BigInt<Bits> mul(BigInt<Bits> a, BigInt<Bits> b)
{
	static_assert(Bits <= 64 && "Can't support larger bits sizes");
	using uint = typename SelectIntType_t<Bits>::uint_t;
	using uintDouble = typename SelectIntType_t<Bits>::uintd_t;

	uintDouble mask = (static_cast<uintDouble>(1) << Bits) - 1;

	bool a_sign = a.is_negative();
	bool b_sign = b.is_negative();
	a.set_sign(false);
	b.set_sign(false);

	if(a == BigInt<Bits>(1)) {
		b.set_sign(a_sign != b_sign);
		return b;
	}
	if(b == BigInt<Bits>(1)) {
		a.set_sign(a_sign != b_sign);
		return a;
	}
	if(a.is_zero() || b.is_zero())
		return BigInt<Bits>(0);

	BigInt<Bits> W;
	W.reserve(a.get_chunks() + b.get_chunks() + 1);
	W.resize(a.get_chunks() + b.get_chunks(), 0);

	for(size_t i = 0; i < b.get_chunks(); i++) {
		uint carry = 0;
		for(size_t j = 0; j < a.get_chunks(); j++) {
			uintDouble tmp = (uintDouble)W.get(i + j) + ((uintDouble)a.get(j) * (uintDouble)b.get(i)) + (uintDouble)carry;
			W.push_bits((uint)(tmp & mask), i + j); // Lower part of mul
			carry = (uint)(tmp >> Bits);		// Upper part of mul
		}
		W.push_bits(carry, i + a.get_chunks());
	}

	W.set_sign(a_sign != b_sign);

	return W;
}

/**
 * @brief Performs division of two BigInts.
 * @tparam Bits Number of bits per chunk.
 * @param a The dividend.
 * @param b The divisor.
 * @return A pair containing {Quotient, Remainder}.
 * @note Implements division with normalization and quotient estimation.
 */
template <size_t Bits>
std::pair<BigInt<Bits>, BigInt<Bits>> div(BigInt<Bits> a, BigInt<Bits> b)
{
	static_assert(Bits <= 64 && "Can't support larger bits sizes");
	assertm(b.get_chunks() >= 1 && b.get(b.get_chunks() - 1) != 0, "Can't divide by zero");

	bool a_sign = a.is_negative();
	bool b_sign = b.is_negative();

	a.set_sign(false);
	b.set_sign(false);

	if(a == b) {
		BigInt<Bits> q(1);
		q.set_sign(a_sign != b_sign);
		return { std::move(q), std::move(BigInt<Bits>(0)) };
	}
	if(a < b) {
		a.set_sign(a_sign);
		return { std::move(BigInt<Bits>(0)), std::move(a) };
	}

	using uint = typename SelectIntType_t<Bits>::uint_t;
	using uintDouble = typename SelectIntType_t<Bits>::uintd_t;

	uintDouble BASE = (uintDouble)1 << Bits;

	// NORMALIZATION
	uint top_chunk = b.get(b.get_chunks() - 1);
	size_t shift = 0;

	while(((top_chunk >> (Bits - 1 - shift)) & 1) == 0) {
		shift++;
	}

	if(shift > 0) {
		a = lshift(a, shift);
		b = lshift(b, shift);
	}

	size_t n = a.get_chunks() - 1;
	size_t t = b.get_chunks() - 1;

	BigInt<Bits> q;
	q.resize((n - t + 1), 0);

	BigInt<Bits> bb_nt = lshift(b, Bits * (n - t));
	uint q_val = q.get(n - t);

	while(a >= bb_nt) {
		q_val++;
		a = sub(a, bb_nt);
	}
	q.push_bits(q_val, n - t);

	for(size_t i = n; i >= (t + 1); i--) {
		int64_t q_idx = i - t - 1;
		uint q_tmp = 0;
		uintDouble r_tmp = 0;

		uintDouble numer = ((uintDouble)a.get_safe(i) << Bits) + a.get_safe(i - 1);
		uintDouble denumer = b.get(t);

		if(a.get_safe(i) == b.get(t)) {
			q_tmp = BASE - 1;
			r_tmp = numer - ((uintDouble)q_tmp * denumer);
		} else {
			q_tmp = (uint)(numer / denumer);
			r_tmp = numer % denumer;
		}

		while((b.get_safe(t - 1) * q_tmp) > ((r_tmp << Bits) + a.get_safe(i - 2))) {
			q_tmp--;
			r_tmp += denumer;
			if(r_tmp >= BASE) break;
		}

		// uintDouble lhs = ((uintDouble)b.get(t) << Bits) + b.get_safe(t - 1);
		// uintDouble rhs = ((uintDouble)a.get(i) * (BASE * BASE)) + ((uintDouble)a.get_safe(i - 1) << Bits) + a.get_safe(i - 2);

		// uintDouble lhs = b.get_safe(t) + (0);
		// uintDouble rhs = a.get_safe(i) * BASE + a.get_safe(i - 1) + (0);
		//
		// while((q_tmp * lhs) > rhs) {
		//      q_tmp--;
		// }

		BigInt<Bits> term = lshift(b, Bits * q_idx);
		BigInt<Bits> prod = mul<Bits>(q_tmp, term);
		a = sub(a, prod);

		if(a.is_negative()) {
			a.set_sign(false);
			a = sub_mag(term, a);

			q_tmp--;
		}

		q.push_bits(q_tmp, q_idx);
	}

	if(shift > 0) {
		a = rshift(a, shift);
	}

	q.set_sign(a_sign != b_sign);
	a.set_sign(a_sign);

	return { std::move(q), std::move(a) /* remainder */ };
}

/**
 * @brief Performs a logical left shift on a BigInt.
 * @tparam Bits Number of bits per chunk.
 * @param a The source BigInt.
 * @param shift The number of bits to shift.
 * @return A new BigInt representing (a << shift).
 */
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

/**
 * @brief Performs a logical right shift on a BigInt.
 * @tparam Bits Number of bits per chunk.
 * @param a The source BigInt.
 * @param shift The number of bits to shift.
 * @return A new BigInt representing (a >> shift).
 */
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

/**
 * @enum BitW_2op
 * @brief Bitwise operator selection for internal use.
 */
enum class BitW_2op {
	AND,
	OR,
	XOR
};

/**
 * @brief Internal helper to execute bitwise operations (AND, OR, XOR).
 * @tparam Bits Number of bits per chunk.
 * @tparam op The operation to perform.
 */
template <size_t Bits, BitW_2op op>
BigInt<Bits> bitwise_2op(const BigInt<Bits> &a, const BigInt<Bits> &b)
{
	static_assert(Bits <= 64 && "Can't support larger bits sizes");
	assertm(!a.is_negative() && !b.is_negative(), "Bitwise operations don't work for negative numbers. a: {}, b: {}", a, b);

	using uint = typename SelectIntType_t<Bits>::uint_t;
	using uintDouble = typename SelectIntType_t<Bits>::uintd_t;

	uint mask = (static_cast<uintDouble>(1) << Bits) - 1;

	size_t size = (a.get_chunks() > b.get_chunks()) ? a.get_chunks() : b.get_chunks();

	BigInt<Bits> W;
	W.reserve(size);

	for(size_t i = 0; i < size; i++) {
		if constexpr(op == BitW_2op::AND) {
			W.push_bits((a.get_safe(i) & b.get_safe(i)) & mask);
		} else if constexpr(op == BitW_2op::OR) {
			W.push_bits((a.get_safe(i) | b.get_safe(i)) & mask);
		} else if constexpr(op == BitW_2op::XOR) {
			W.push_bits((a.get_safe(i) ^ b.get_safe(i)) & mask);
		}
	}
	return W;
}

/** @brief Performs bitwise AND. */
template <size_t Bits>
BigInt<Bits> AND(const BigInt<Bits> &a, const BigInt<Bits> &b)
{
	return bitwise_2op<Bits, BitW_2op::AND>(a, b);
}

/** @brief Performs bitwise OR. */
template <size_t Bits>
BigInt<Bits> OR(const BigInt<Bits> &a, const BigInt<Bits> &b)
{
	return bitwise_2op<Bits, BitW_2op::OR>(a, b);
}

/** @brief Performs bitwise XOR. */
template <size_t Bits>
BigInt<Bits> XOR(const BigInt<Bits> &a, const BigInt<Bits> &b)
{
	return bitwise_2op<Bits, BitW_2op::XOR>(a, b);
}

/**
 * @brief Performs bitwise NOT on a non-negative BigInt.
 */
template <size_t Bits>
BigInt<Bits> NOT(const BigInt<Bits> &a)
{
	static_assert(Bits <= 64 && "Can't support larger bits sizes");
	assertm(!a.is_negative(), "NOT operation don't work for negative numbers. a: {}", a);

	BigInt<Bits> W;
	W.reserve(a.get_chunks());

	using uint = typename SelectIntType_t<Bits>::uint_t;
	using uintDouble = typename SelectIntType_t<Bits>::uintd_t;

	uint mask = (static_cast<uintDouble>(1) << Bits) - 1;

	for(size_t i = 0; i < a.get_chunks(); i++) {
		W.push_bits(~a.get(i) & mask);
	}
	return W;
}

/**
 * @brief Performs two's complement-style negation on a BigInt.
 */
template <size_t Bits>
BigInt<Bits> NOT_signed(const BigInt<Bits> &a)
{
	static_assert(Bits <= 64 && "Can't support larger bits sizes");
	using uint = typename SelectIntType_t<Bits>::uint_t;
	using uintDouble = typename SelectIntType_t<Bits>::uintd_t;

	uint mask = (static_cast<uintDouble>(1) << Bits) - 1;

	BigInt<Bits> W = a;

	if(!a.is_negative()) {

		bool carry = true;
		for(size_t i = 0; i < a.get_chunks() && carry; i++) {
			auto val = W.get(i);
			val++;
			val &= mask;
			W.get(i) = val;
			carry = (val == 0);
		}
		W.set_sign(true);
	} else {
		bool borrow = true;
		for(size_t i = 0; i < a.get_chunks() && borrow; i++) {
			auto val = W.get(i);
			bool next_borrow = (val == 0);
			val--;
			val &= mask;
			W.get(i) = val;
			borrow = next_borrow;
		}
		W.set_sign(false);
	}
	return W;
}

/**
 * @brief Increments a BigInt in-place.
 */
template <size_t Bits>
void inc(BigInt<Bits> &a)
{
	static_assert(Bits <= 64 && "Can't support larger bits sizes");
	using uint = typename SelectIntType_t<Bits>::uint_t;
	using uintDouble = typename SelectIntType_t<Bits>::uintd_t;

	uint mask = (static_cast<uintDouble>(1) << Bits) - 1;

	uint carry = 0;
	size_t idx = 0;
	do {
		uintDouble tmp = (uintDouble)a.get(idx) + 1 + (uintDouble)carry;
		a.push_bits(tmp & mask, idx);
		carry = (tmp >> Bits);

		idx++;
	} while(carry != 0 && idx < a.get_chunks());
}

/**
 * @brief Decrements a BigInt in-place.
 */
template <size_t Bits>
void dec(BigInt<Bits> &a)
{
	static_assert(Bits <= 64 && "Can't support larger bits sizes");
	using uint = typename SelectIntType_t<Bits>::uint_t;
	using uintDouble = typename SelectIntType_t<Bits>::uintd_t;

	uint mask = (static_cast<uintDouble>(1) << Bits) - 1;

	uint carry = 0;
	size_t idx = 0;
	do {
		uintDouble tmp = (uintDouble)a.get(idx) - 1 + (uintDouble)carry;
		a.push_bits(tmp & mask, idx);
		carry = (tmp >> Bits);

		idx++;
	} while(carry != 0 && idx < a.get_chunks());
}

} // namespace bga
