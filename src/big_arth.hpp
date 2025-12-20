#pragma once

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <print>
#include <sys/types.h>

#include "bigint.hpp"

namespace bga
{

template <size_t Bits>
BigInt<Bits> add(BigInt<Bits> a, BigInt<Bits> b)
{
	static_assert(Bits <= 64 && "Can't support larger bits sizes");
	assert(!a.is_negative() && !b.is_negative() && "Can't do addition with negative numbers");
	using uintDouble = typename SelectIntType_t<Bits>::uintd_t;

	BigInt<Bits> W;
	W.reserve(a.get_chunks());

	uintDouble carry = 0;
	uintDouble mask = (static_cast<uintDouble>(1) << Bits) - 1;

	size_t size = (a.get_chunks() > b.get_chunks()) ? a.get_chunks() : b.get_chunks();

	for(size_t i = 0; i < size; i++) {
		uintDouble tmp = (uintDouble)a.get_safe(i) + (uintDouble)b.get_safe(i) + carry;
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

	uintDouble mask = (static_cast<uintDouble>(1) << Bits) - 1;

	auto subtract = [mask](BigInt<Bits> &x, BigInt<Bits> &y) -> std::pair<BigInt<Bits>, intDouble> {
		intDouble carry = 0;
		BigInt<Bits> W;
		W.reserve(x.get_chunks());

		size_t size = (x.get_chunks() > y.get_chunks()) ? x.get_chunks() : y.get_chunks();

		for(size_t i = 0; i < size; i++) {
			intDouble diff = (intDouble)x.get_safe(i) - (intDouble)y.get_safe(i) + carry;
			W.push_bits(diff & mask);

			carry = ((intDouble)diff < 0) ? -1 : 0;
		}
		return { W, carry };
	};

	auto [W, carry] = subtract(a, b);

	if(carry == -1) {
		BigInt<Bits> zero;
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

	if(a == BigInt<Bits>(1))
		return b;
	else if(b == BigInt<Bits>(1))
		return a;

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

	return W;
}

template <size_t Bits>
std::pair<BigInt<Bits>, BigInt<Bits>> div(BigInt<Bits> a, BigInt<Bits> b)
{
	static_assert(Bits <= 64 && "Can't support larger bits sizes");
	// assertm(a.get_chunks() >= b.get_chunks() && a.get_chunks() >= 1 && b.get_chunks() >= 1, "a needs to be larger than b and they both need to be larger or equal than 1");
	assertm(b.get_chunks() >= 1 && b.get(b.get_chunks() - 1) != 0, "Can't divide by zero");

	// using uint = typename SelectIntType_t<Bits>::uint_t;
	using uintDouble = typename SelectIntType_t<Bits>::uintd_t;

	if(a == b) return { BigInt<Bits>(1), BigInt<Bits>(0) };
	if(a < b) return { BigInt<Bits>(0), a };

	// std::println("BEFORE NORMALIZATION: a {}, n {},  b {}, t {}\n", a, a.get_chunks(), b, b.get_chunks());

	// NORMALIZATION
	uint top_chunk = b.get(b.get_chunks() - 1);
	size_t shift = 2 * 4 * Bits; // + Bits / 2;
	// size_t shift = 0;
	// if constexpr(Bits < 8) std::println("shift {}, a {}, b {}", shift, a, b);

	while(((top_chunk >> (Bits - 1 - shift)) & 1) == 0) {
		shift++;
	}

	if(shift > 0) {
		a = lshift(a, shift);
		b = lshift(b, shift);
	}

	// if constexpr(Bits < 8) std::println("same shift {}, a {}, b {}", shift, a, b);
	size_t n = a.get_chunks() - 1;
	size_t t = b.get_chunks() - 1;

	// std::println("\n");
	// std::println("a {} | n {}, b {} | t {}", a, n, b, t);

	BigInt<Bits> q;
	q.resize((n - t + 1), 0);

	// std::println("q {}", q);

	BigInt<Bits> r;
	r.reserve(b.get_chunks());

	uintDouble mask = (static_cast<uintDouble>(1) << Bits) - 1;
	// std::println("mask {:b}", mask);

	uintDouble BASE = (uintDouble)1 << Bits;

	BigInt<Bits> bb_nt = lshift(b, (n - t) * Bits);
	// std::println("bb_nt {}", bb_nt);
	uint q_val = q.get_safe(n - t);
	// std::println("q_val {}", q_val);

	while(a >= bb_nt) {
		q_val++;
		q_val &= mask;
		a = sub(a, bb_nt);
	}
	// std::println("q_val {}", q_val);
	q.push_bits(q_val, n - t);
	// std::println("start q {}", q);

	for(size_t i = n; i >= (t + 1); i--) {
		long long q_idx = i - t - 1;
		uintDouble q_tmp = 0;

		// std::println("{:-^40}", "");
		// std::println("ITERATION:[[ i: {}, i >= (t+1):{}, q_idx ]]", i, t + 1, q_idx);
		//
		// std::println("");
		// std::println("if(a[i]: {} == b[t]: {}) {{", a.get(i), b.get(t));
		// std::println("\tq_tmp = {}", (BASE - 1) & mask);
		// std::println("}} else {{");
		// std::println("\tq_tmp = {}", ((((uintDouble)a.get(i) << Bits) | a.get(i - 1)) / b.get(t)) & mask);
		// std::println("}}");

		// uintDouble numerator = (((uintDouble)a.get(i) << Bits) | a.get(i - 1));
		// uintDouble denumerator = b.get(t);
		// uintDouble hat_r = 0;

		if(a.get(i) == b.get(t)) {
			q_tmp = (BASE - 1) & mask;
			// hat_r = numerator - (q_tmp * denumerator);
		} else {
			q_tmp = ((uint)a.get(i) * BASE + a.get(i - 1)) / (uint)b.get(t);

			// q_tmp = ((uint)a.get_safe(i) * (BASE * BASE) + a.get_safe(i - 1) * BASE + a.get_safe(i - 2)) / ((uint)b.get_safe(t) * BASE + b.get_safe(t - 1));
			// q_tmp = (((uintDouble)a.get_safe(i) << (Bits * 2)) + ((uintDouble)a.get_safe(i - 1) << Bits) + a.get_safe(i - 2)) / ((uint)b.get_safe(t) * BASE + b.get_safe(t - 1));

			// q_tmp = (((uintDouble)a.get_safe(i) << Bits) + a.get_safe(i - 1) + ((uintDouble)a.get_safe(i - 2) >> Bits)) / (b.get_safe(t) + ((uintDouble)b.get_safe(t - 1) >> Bits));

			// q_tmp = (/ b.get(t)) & mask;
			// q_tmp = numerator / denumerator;
			// hat_r = numerator % denumerator;
		}
		// std::println("q_tmp = {}", q_tmp);
		// std::println("");

		// uintDouble v2 = b.get_safe(t - 1);
		// uintDouble u2 = a.get_safe(i - 2);
		// while(q_tmp * v2 > (hat_r * BASE) + u2) {
		// 	q_tmp--;
		// 	hat_r += denumerator;
		// 	if(hat_r >= BASE) break;
		// }

		// uintDouble lhs = b.get_safe(t) * BASE + b.get_safe(t - 1);
		// uintDouble lhs = b.get_safe(t) + ((uintDouble)b.get_safe(t - 1) >> Bits);
		uintDouble lhs = ((uintDouble)b.get_safe(t) << Bits) | b.get_safe(t - 1);
		// uintDouble lhs = b.get_safe(t) + (0);

		// uintDouble rhs = a.get_safe(i) * (BASE * BASE) + a.get_safe(i - 1) * BASE + a.get_safe(i - 2);
		uintDouble rhs = a.get_safe(i) * BASE + a.get_safe(i - 1) + ((uintDouble)a.get_safe(i - 2) >> Bits);
		// uintDouble rhs = a.get_safe(i) * BASE + a.get_safe(i - 1) + (0);

		// int cnt = 0;
		// std::println("");
		// std::println("while(( q_tmp: {} * lhs: {} ): {} > rhs: {}) {{", q_tmp, lhs, (q_tmp * lhs), rhs);
		while((q_tmp * lhs) > rhs) {
			q_tmp--;
			q_tmp &= mask;
			// 	cnt++;
		}
		// std::println("cnt: {}, q_tmp: {}", cnt, q_tmp);
		// std::println("");

		// BigInt<Bits> rhs = lshift<Bits>(BigInt<Bits>(a.get_safe(i)), 2 * Bits);
		// rhs = add<Bits>(rhs, ((uintDouble)a.get_safe(i - 1) << Bits));
		// rhs = add<Bits>(rhs, a.get_safe(i - 2));

		// while(true) {
		// 	uintDouble q_high = q_tmp * b.get_safe(t);
		// 	uintDouble q_low = q_tmp * b.get_safe(t - 1);
		//
		// 	uint lhs[3];
		//
		// 	lhs[2] = (uint)(q_high >> Bits);
		// 	uintDouble tmp = (q_high & mask) + (q_low >> Bits);
		// 	lhs[2] += (uint)(tmp >> Bits);
		// 	lhs[1] = (uint)(tmp & Bits);
		// 	lhs[0] = (uint)(q_low >> Bits);
		//
		// 	uint rhs[3];
		//
		// 	rhs[2] = a.get(i);
		// 	rhs[1] = a.get(i - 1);
		// 	rhs[0] = a.get(i - 2);
		//
		// 	if(lhs[2] < rhs[2]) break;
		// 	if(lhs[2] > rhs[2]) {
		// 		q_tmp--;
		// 		continue;
		// 	}
		//
		// 	if(lhs[0] <= rhs[0]) break;
		//
		// 	q_tmp--;
		// }

		BigInt<Bits> term = lshift(b, q_idx * Bits);
		// std::println("term: {}, b: {}, q_idx * Bits: {}", term, b, q_idx * Bits);

		BigInt<Bits> prod = mul<Bits>(q_tmp, term);
		// std::println("prod: {}, q_tmp: {}", prod, q_tmp);

		BigInt<Bits> temp = a;
		a = sub(a, prod);
		// std::println("a_before: {}, a: {}", temp, a);
		// std::println("");

		// if(prod > a) {
		// 	q_tmp--;
		// 	q_tmp &= mask;
		// 	prod = sub(prod, term);
		// }
		// a = sub(a, prod);

		if(a.is_negative()) {
			a.set_sign(false);
			a = sub(term, a);
			// std::println("a.is_negative = a {}", a);

			// std::println("BEFORE q_tmp: {}", q_tmp);
			q_tmp--;
			q_tmp &= mask;
			// std::println("AFTER q_tmp: {}", q_tmp);
		}
		q.push_bits(q_tmp, q_idx);
		// std::println("");
		// std::println("PUSHED to {} val {}", q_idx, q_tmp);
		// std::println("{:-^40}", "");
		// std::println("");
	}
	// r = std::move(a);
	// std::println("");
	r = a;
	// NORMALIZATION
	if(shift > 0) {
		r = rshift(r, shift);
	}
	// std::println("DENORMALIZATION r {}", r);
	return { q, r };
}

template <size_t Bits>
std::pair<BigInt<Bits>, BigInt<Bits>> div_fi(BigInt<Bits> a, BigInt<Bits> b)
{
	static_assert(Bits <= 64 && "Can't support larger bits sizes");
	assertm(b.get_chunks() >= 1 && b.get(b.get_chunks() - 1) != 0, "Can't divide by zero");

	using uint = typename SelectIntType_t<Bits>::uint_t;
	using uintDouble = typename SelectIntType_t<Bits>::uintd_t;

	// --- 0. Trivial Cases ---
	if(a < b) return { BigInt<Bits>(0), a };
	if(a == b) return { BigInt<Bits>(1), BigInt<Bits>(0) };

	// --- 1. Single Chunk Divisor Optimization ---
	// Knuth Algorithm D requires divisor length > 1.
	// We must handle single-chunk divisors separately (and it's faster!).
	if(b.get_chunks() == 1) {
		uintDouble divisor = b.get(0);
		uintDouble remainder = 0;
		BigInt<Bits> q;
		q.resize(a.get_chunks(), 0);

		// Standard "schoolbook" short division
		for(int i = a.get_chunks() - 1; i >= 0; i--) {
			uintDouble cur = ((uintDouble)remainder << Bits) | a.get(i);
			q.push_bits((uint)(cur / divisor), i);
			remainder = cur % divisor;
		}
		// q.trim(); // Remove leading zeros if your BigInt class supports it
		BigInt<Bits> r;
		r.push_bits((uint)remainder);
		return { q, r };
	}

	// --- 2. Normalization (Algorithm D Step D1) ---
	uint top_chunk = b.get(b.get_chunks() - 1);
	size_t shift = 0;
	while(((top_chunk >> (Bits - 1 - shift)) & 1) == 0) {
		shift++;
	}

	if(shift > 0) {
		a = lshift(a, shift);
		b = lshift(b, shift);
	}

	// We effectively extended 'a' by shifting.
	// Ideally, Algorithm D expects u[m+n]...u[0].
	// We ensure we can access indices safely by relying on get_safe returning 0.

	size_t n = a.get_chunks() - 1;
	size_t t = b.get_chunks() - 1; // t is divisor MSB index

	// Safety check after normalization (in case a grew or b didn't)
	if(a < b) {
		if(shift > 0) a = rshift(a, shift);
		return { BigInt<Bits>(0), a };
	}

	BigInt<Bits> q;
	q.resize(n - t + 1, 0);

	uintDouble mask = (static_cast<uintDouble>(1) << Bits) - 1;
	uintDouble BASE = (uintDouble)1 << Bits;

	// --- 3. Main Loop (Step D2) ---
	// We loop from n down to t.
	// Loop condition must include 't' to handle the case where len(a) == len(b).
	for(size_t i = n; i >= t; i--) {
		size_t q_idx = i - t; // Quotient index matches difference in position

		// --- D3. Estimate q_hat ---
		// Numerator is top 2 digits of current window: (u[i] * B + u[i-1])
		// If i=n and a was not extended, a[i] might be small or 0?
		// Actually, with lshift, a[n] is valid.

		uintDouble q_hat = 0;
		uintDouble r_hat = 0;

		// Use get_safe to handle i-1 < 0 gracefully (though with t>=1 this shouldn't trigger for i-1)
		uintDouble u_i = a.get_safe(i);
		uintDouble u_i_1 = a.get_safe(i - 1);
		uintDouble v_t = b.get_safe(t);

		uintDouble numerator = (u_i << Bits) | u_i_1;

		// If the top digits match, the estimate is BASE-1
		if(u_i == v_t) {
			q_hat = mask;
			r_hat = numerator - (q_hat * v_t);
		} else {
			q_hat = numerator / v_t;
			r_hat = numerator % v_t;
		}

		// --- Refine q_hat ---
		// Check: q_hat * v[t-1] > (r_hat * B + u[i-2])
		uintDouble v_t_1 = b.get_safe(t - 1);
		uintDouble u_i_2 = a.get_safe(i - 2); // get_safe returns 0 if index < 0

		while(true) {
			if(r_hat >= BASE) break; // Term on right is massive, inequality fails

			if((q_hat * v_t_1) > ((r_hat << Bits) | u_i_2)) {
				q_hat--;
				r_hat += v_t;
			} else {
				break;
			}
		}

		// --- D4. Multiply and Subtract ---
		// a -= q_hat * (b << (q_idx * Bits))

		BigInt<Bits> term = lshift(b, q_idx * Bits);
		BigInt<Bits> prod = mul<Bits>(q_hat, term);

		bool borrow = false;
		if(prod > a) borrow = true;
		a = sub(a, prod);

		// --- D5 & D6. Test Remainder and Add Back ---
		if(borrow) {
			q_hat--;
			a.set_sign(false);
			a = sub(term, a); // Correction
		}

		q.push_bits((uint)q_hat, q_idx);

		// Prevent unsigned underflow in loop
		if(i == 0) break;
	}

	// --- D8. Unnormalize ---
	if(shift > 0) {
		a = rshift(a, shift);
	}

	// Optional: Trim result vectors
	// q.trim();
	// a.trim();

	return { q, a };
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

enum class BitW_2op {
	AND,
	OR,
	XOR
};

template <size_t Bits, BitW_2op op>
BigInt<Bits> bitwise_2op(BigInt<Bits> a, BigInt<Bits> b)
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

template <size_t Bits>
BigInt<Bits> AND(BigInt<Bits> a, BigInt<Bits> b)
{
	return bitwise_2op<Bits, BitW_2op::AND>(a, b);
}

template <size_t Bits>
BigInt<Bits> OR(BigInt<Bits> a, BigInt<Bits> b)
{
	return bitwise_2op<Bits, BitW_2op::OR>(a, b);
}

template <size_t Bits>
BigInt<Bits> XOR(BigInt<Bits> a, BigInt<Bits> b)
{
	return bitwise_2op<Bits, BitW_2op::XOR>(a, b);
}

template <size_t Bits>
BigInt<Bits> NOT(BigInt<Bits> a)
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

template <size_t Bits>
BigInt<Bits> NOT_signed(BigInt<Bits> a)
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
