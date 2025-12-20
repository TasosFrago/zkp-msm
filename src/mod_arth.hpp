#pragma once

#include "big_arth.hpp"
#include "bigint.hpp"
#include <ostream>
#include <print>
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

enum class MONT_ALGO {
	SOS = 0,
	CIOS,
	FIOS
};

template <size_t Bits, MONT_ALGO algo = MONT_ALGO::SOS>
struct Montgomery {
	using bgint = bga::bgint<Bits>;
	using uint = bga::SelectIntType_t<Bits>::uint_t;
	using uintDouble = bga::SelectIntType_t<Bits>::uintd_t;

	bgint n;
	uint n_p;
	bgint r2;

	size_t n_chunks;
	size_t total_bits;

private:
	static uint calc_n_p(uint n0)
	{
		// n0 &= mask;
		uint x = 1;
		size_t width = Bits;
		size_t count = 0;
		while((1ULL << count) < width) count++;

		for(size_t i = 0; i < count; i++) {
			x *= ((uint)2 - n0 * x);
		}

		uint mask = (Bits >= sizeof(uint) * 8) ? (uint)-1 : ((uint)1 << Bits) - 1;

		return (uint)((-x) & mask);
	}

	bgint calc_r2() const
	{
		bgint x(1);
		size_t double_total_bits = 2 * n_chunks * Bits;

		for(size_t i = 0; i < double_total_bits; i++) {
			x = bga::lshift(x, 1);
			if(x >= n) {
				x = bga::sub(x, n);
			}
		}
		return x;
	}

public:
	Montgomery(bgint modulus)
	    : n(std::move(modulus)), n_chunks(modulus.get_chunks()),
	      total_bits(n.get_chunks() * Bits)
	{
		static_assert(Bits <= 64 && "Can't support larger bits sizes");
		assert(!n.is_negative() && "Modulus must be positive");
		assert((n.get(0) & 1) && "Modulus must be odd");

		n_p = calc_n_p(n.get(0));
		r2 = calc_r2();
	}

	bgint init(bgint x)
	{
		return mul(x, r2);
	}

	bgint mul(bgint a, bgint b)
	{
		if constexpr(algo == MONT_ALGO::SOS) {
			return mul_sos(a, b);
		} else if constexpr(algo == MONT_ALGO::CIOS) {
			return mul_cios(a, b);
		} else if constexpr(algo == MONT_ALGO::FIOS) {
			return mul_fios(a, b);
		}
	}

	bgint trans_back(bgint x)
	{
		bgint one(1);
		return mul(x, one);
	}

	bgint reduce_sos(bgint t)
	{
		/*
		 * for i = 0 to s - 1
		 * 	C = 0
		 * 	m = t[i] * n' modW
		 *
		 * 	for j = 0 to s - 1
		 * 		(C, S) = t[i + j] + mn[j] + C
		 * 		t[i + j] = S
		 * 	ADD(t[i+s], C)
		 * 	for j = 0 to s
		 * 		u[j] = t[j + s]
		 */
		uint mask = (Bits >= sizeof(uint) * 8) ? (uint)-1 : ((uint)1 << Bits) - 1;

		for(size_t i = 0; i < n_chunks; i++) {
			uint m = (uint)(((uintDouble)t.get_safe(i) * (uintDouble)n_p) & mask);

			bgint prod(m);
			prod = bga::mul(prod, n);
			prod = bga::lshift(prod, i * Bits);

			t = bga::add(t, prod);
		}

		t = bga::rshift(t, total_bits);

		return (t >= n) ? bga::sub(t, n) : t;
	}

	bgint mul_sos(bgint a, bgint b)
	{
		return reduce_sos(bga::mul(a, b));
	}

	bgint mul_cios(bgint a, bgint b)
	{
		/*
		 * for i = 0 to s - 1
		 * 	C = 0
		 *
		 * 	for j = 0 to s - 1
		 * 		(C, S) = t[j] + a[j]b[i] + C
		 * 		t[j] = S
		 * 	(C, S) = t[s] + C
		 * 	t[s] = S
		 * 	t[s+1] = C
		 * 	C = 0
		 * 	m = t[0]n'[0]modW
		 * 	(C,S) = t[0] + mn[0]
		 *
		 * 	for j = 1 to s-1
		 * 		(C, S) = t[j] + mn[j] + C
		 * 		t[j - 1] = S
		 * 	(C,S) = t[s] + C
		 * 	t[s - 1] = S
		 * 	t[s] = t[s + 1] + C
		 */
		bgint t;
		t.reserve(n_chunks + 2);
		t.resize(n_chunks + 2, 0);

		uintDouble mask = (static_cast<uintDouble>(1) << Bits) - 1;

		for(size_t i = 0; i < n_chunks; i++) {
			uint carry = 0;
			uint b_i = b.get_safe(i);

			for(size_t j = 0; j < n_chunks; j++) {
				uintDouble tmp = (uintDouble)t.get(j) + (uintDouble)a.get_safe(j) * (uintDouble)b_i + (uintDouble)carry;

				t.push_bits((uint)(tmp & mask), j);
				carry = (uint)(tmp >> Bits);
			}

			uintDouble tmp_s = (uintDouble)t.get(n_chunks) + (uintDouble)carry;
			t.push_bits((uint)(tmp_s & mask), n_chunks);
			t.push_bits((uint)(tmp_s >> Bits), n_chunks + 1);

			uint t_0 = t.get(0);
			uint m = (uint)(((uintDouble)t_0 * n_p) & mask);
			uintDouble tmp_red = (uintDouble)t_0 + (uintDouble)m * n.get(0);
			carry = (uint)(tmp_red >> Bits);

			for(size_t j = 1; j < n_chunks; j++) {
				uintDouble tmp = (uintDouble)t.get(j) + (uintDouble)m * n.get(j) + (uintDouble)carry;

				t.push_bits((uint)(tmp & mask), j - 1);
				carry = (uint)(tmp >> Bits);
			}
			uintDouble tmp_end = (uintDouble)t.get(n_chunks) + (uintDouble)carry;
			t.push_bits((uint)(tmp_end & mask), n_chunks - 1);
			carry = (uint)(tmp_end >> Bits);

			t.push_bits((uint)((t.get(n_chunks + 1) + carry) & mask), n_chunks);
			t.push_bits(0, n_chunks + 1);
		}

		return (t >= n) ? bga::sub(t, n) : t;
	}

	bgint mul_fios(bgint a, bgint b)
	{
		/*
		 * for i to s - 1
		 * 	(C,S) = t[0] + a[0]b[i]
		 * 	ADD(t[1], C)
		 * 	m = S n'[0]modW
		 * 	(C,S) = S + mn[0]
		 *
		 * 	for j to s - 1
		 * 		(C,S) = t[j] + a[j]b[i] + C
		 * 		ADD(t[j + 1], C)
		 * 		(C,S) = S + mn[j]
		 * 		t[j-1] = S
		 * 	(C,S) = t[s] + C
		 * 	t[s - 1] = S
		 * 	t[s] = t[s + 1] + C
		 * 	t[s + 1] = 0
		 */
		bgint t;
		t.reserve(n_chunks + 2);
		t.resize(n_chunks + 2, 0);

		uintDouble mask = (static_cast<uintDouble>(1) << Bits) - 1;

		/*
		 *   t[j]      n     a[j]
		 *     \       |      |
		 *      \      |      |
		 *       \     V      V
		 *	  +----------------+
		 *	  |                |
		 *   <----|                |<---- (c_mul, c_red)
		 *        |                |
		 *   <----|       PE       |<---- b[i]
		 *        |                |
		 *   <----|                |<---- m
		 *	  |                |
		 *	  +----+------+----+
		 *	       |      |     \
		 *	       |      |      \
		 *	       V      V       \
		 *                            t[j-1]
		 */
		for(size_t i = 0; i < n_chunks; i++) {
			uint b_i = b.get_safe(i);

			uintDouble tmp_mult = (uintDouble)t.get(0) + (uintDouble)a.get(0) * (uintDouble)b_i;

			uintDouble carry_mul = tmp_mult >> Bits; // C1
			uint sum_mul = (tmp_mult & mask);

			uint m = ((uintDouble)sum_mul * n_p) & mask;

			uintDouble tmp_red = (uintDouble)sum_mul + (uintDouble)m * n.get(0);

			uintDouble carry_red = tmp_red >> Bits; // C2

			for(size_t j = 1; j < n_chunks; j++) {
				// uintDouble tmp_pe = (uintDouble)t.get(j) +
				// 		    (uintDouble)a.get_safe(j) * b_i +
				// 		    carry_mul + // C1
				// 		    carry_red;	// C2

				uintDouble tmp_pe1 = (uintDouble)t.get(j) +
						     (uintDouble)a.get_safe(j) * b_i +
						     carry_mul;

				uintDouble next_carry_mul = tmp_pe1 >> Bits;

				uintDouble tmp_pe2 = (tmp_pe1 & mask) + carry_red;

				next_carry_mul += tmp_pe2 >> Bits;

				carry_mul = next_carry_mul;

				uint S_partial = (uint)(tmp_pe2 & mask);

				uintDouble tmp_r = (uintDouble)S_partial + (uintDouble)m * n.get(j);

				t.push_bits((uint)(tmp_r & mask), j - 1);

				carry_red = tmp_r >> Bits;
			}

			uintDouble tmp_end = (uintDouble)t.get(n_chunks) +
					     carry_mul +
					     carry_red;

			t.push_bits((uint)(tmp_end & mask), n_chunks - 1);
			uintDouble carry_final = tmp_end >> Bits;

			uintDouble top_val = (uintDouble)t.get(n_chunks + 1) + carry_final;

			t.push_bits((uint)(top_val & mask), n_chunks);
			t.push_bits(0, n_chunks + 1);
		}

		return (t >= n) ? bga::sub(t, n) : t;
	}
};

template <size_t Bits, MONT_ALGO algo>
BigInt<Bits> mul(BigInt<Bits> a, BigInt<Bits> b, BigInt<Bits> q)
{
	Montgomery<Bits, algo> mont(q);

	auto a_m = mont.init(a);
	auto b_m = mont.init(b);
	auto c_m = mont.mul(a_m, b_m);

	return mont.trans_back(c_m);
}

} // namespace mda
