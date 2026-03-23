#pragma once

/**
 * @file mod_arth.hpp
 * @brief Modular arithmetic and Montgomery multiplication implementations.
 */
#include <utility>

#include "big_arth.hpp"
#include "bigint.hpp"
#include "mda_primetest.hpp"

/**
 * @namespace mda
 * @brief Modular Data Architecture - Contains modular arithmetic logic and Montgomery structures.
 */
namespace mda
{
using bga::BigInt;

/**
 * @brief Performs the modulo operation (a mod q).
 * @tparam Bits Number of bits per chunk.
 * @param a The dividend.
 * @param q The modulus (must be positive).
 * @return BigInt<Bits> representing (a mod q).
 * @note Correctly handles negative values for 'a' by adding 'q' to the remainder.
 */
template <size_t Bits>
BigInt<Bits> mod(BigInt<Bits> a, BigInt<Bits> q)
{
	static_assert(Bits <= 64 && "Can't support larger bits sizes");
	assertm(!q.is_negative(), "q can't be negative");

	auto [_, r] = bga::div(a, q);
	return r;

	// if(a.is_negative()) {
	// 	a.set_sign(false);
	//
	// 	BigInt<Bits> rem = mod(a, q);
	//
	// 	if(rem == 0) return rem;
	//
	// 	return bga::sub(q, rem);
	// }
	//
	// while(a >= q) {
	// 	a = bga::sub(a, q);
	// }
	// return a;
}

/**
 * @brief Performs modular addition (a + b mod q).
 * @tparam Bits Number of bits per chunk.
 * @param a The first operand (must be < q).
 * @param b The second operand (must be < q).
 * @param q The modulus.
 * @return BigInt<Bits> representing (a + b mod q).
 */
template <size_t Bits>
BigInt<Bits> add(const BigInt<Bits> &a, const BigInt<Bits> &b, const BigInt<Bits> &q)
{
	static_assert(Bits <= 64 && "Can't support larger bits sizes");
	assert(a < q && b < q && "a and b need to be smaller than q");

	BigInt<Bits> W = bga::add_mag(a, b);

	if(W >= q) {
		W = bga::sub_mag(W, q);
	}

	assert(W < q && "Result should be smaller than q");
	return W;
}

/**
 * @brief Performs modular subtraction (a - b mod q).
 * @tparam Bits Number of bits per chunk.
 * @param a The minuend (must be < q).
 * @param b The subtrahend (must be < q).
 * @param q The modulus.
 * @return BigInt<Bits> representing (a - b mod q).
 */
template <size_t Bits>
BigInt<Bits> sub(const BigInt<Bits> &a, const BigInt<Bits> &b, const BigInt<Bits> &q)
{
	static_assert(Bits <= 64 && "Can't support larger bits sizes");
	assertm(a < q && b < q, "a and b need to be smaller than q. a: {}, b: {}, q: {}", a, b, q);
	using bga::add_mag;
	using bga::sub_mag;

	if(a >= b) {
		return sub_mag(a, b);
	} else {
		return sub_mag(add_mag(a, q), b);
	}
	std::unreachable();
}

/**
 * @enum MONT_ALGO
 * @brief Available Montgomery Multiplication algorithms.
 */
enum class MONT_ALGO {
	SOS = 0, ///< Separated Operand Scanning
	CIOS,	 ///< Coarsely Integrated Operand Scanning
	FIOS	 ///< Finely Integrated Operand Scanning
};

/**
 * @struct Montgomery
 * @brief Context for performing Montgomery Multiplication and reduction.
 * * Montgomery multiplication allows for efficient modular multiplication without trial division.
 * * @tparam Bits Number of bits per chunk.
 * @tparam algo The Montgomery algorithm variant to use (SOS, CIOS, or FIOS).
 */
template <size_t Bits, MONT_ALGO algo = MONT_ALGO::SOS>
struct Montgomery {
	using bgint = bga::bgint<Bits>;
	using uint = bga::SelectIntType_t<Bits>::uint_t;
	using uintDouble = bga::SelectIntType_t<Bits>::uintd_t;

	bgint n;  ///< The modulus.
	uint n_p; ///< Montgomery constant n' = -n^-1 mod W.
	bgint r2; ///< Precomputed value R^2 mod n.

	size_t n_chunks;   ///< Number of chunks in the modulus.
	size_t total_bits; ///< Total bit width (n_chunks * Bits).

	// NOTE: Maybe revisit this idea later.
	struct bgint_m {
		bga::bgint<Bits> value;

		explicit bgint_m(bga::bgint<Bits> v) : value(std::move(v)) {};

		operator const bga::bgint<Bits> &() const
		{
			return value;
		}
	};

private:
	/**
	 * @brief Calculates the Montgomery constant n' using Newton's method.
	 */
	static constexpr uint calc_n_p(uint n0)
	{
		// n0 &= mask;
		uint x = 1;
		size_t width = Bits;
		size_t count = 0;
		while((1ULL << count) < width) count++;

		for(size_t i = 0; i < count; i++) {
			x *= ((uint)2 - n0 * x);
		}

		uintDouble mask = (static_cast<uintDouble>(1) << Bits) - 1;

		return (uint)((-x) & mask);
	}

	/**
	 * @brief Precomputes R^2 mod n.
	 */
	bgint calc_r2() const
	{
		bgint x(1);
		size_t double_total_bits = 2 * n_chunks * Bits;

		for(size_t i = 0; i < double_total_bits; i++) {
			x = bga::lshift(x, 1);
			if(x >= n) {
				x = bga::sub_mag(x, n);
			}
		}
		return x;
	}

public:
	/**
	 * @brief Constructs a Montgomery context and precomputes necessary constants.
	 * @param modulus The modular base (must be positive and odd).
	 */
	Montgomery(bgint modulus)
	    : n(std::move(modulus)), n_chunks(n.get_chunks()),
	      total_bits(n_chunks * Bits)
	{
		static_assert(Bits <= 64 && "Can't support larger bits sizes");
		assert(!n.is_negative() && "Modulus must be positive");
		assert((n.get(0) & 1) && "Modulus must be odd");

		n_p = calc_n_p(n.get(0));
		r2 = calc_r2();
	}

	/** @brief Transforms a standard integer into the Montgomery domain (xR mod n). */
	bgint init(bgint x, bool normalize = false) const
	{
		if(normalize) {
			if(x.is_negative() || x >= n) {
				x = mda::mod(x, n);
				if(x.is_negative()) {
					x = bga::add(x, n);
				}
			}
		}
		return mul(x, r2);
	}

	/** @brief Performs Montgomery Multiplication (abR^-1 mod n). */
	bgint mul(const bgint &a, const bgint &b) const
	{
		if constexpr(algo == MONT_ALGO::SOS) {
			return mul_sos(a, b);
		} else if constexpr(algo == MONT_ALGO::CIOS) {
			return mul_cios(a, b);
		} else if constexpr(algo == MONT_ALGO::FIOS) {
			return mul_fios(a, b);
		}
	}

	/** @brief Transforms a Montgomery domain integer back to the standard domain. */
	bgint trans_back(const bgint &x) const
	{
		static const bgint one(1);
		return mul(x, one);
	}

	/** @brief SOS Multiplication implementation. */
	bgint mul_sos(const bgint &a, const bgint &b) const
	{
		auto t = bga::mul(a, b);
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
		uintDouble mask = (static_cast<uintDouble>(1) << Bits) - 1;

		for(size_t i = 0; i < n_chunks; i++) {
			uint m = (uint)(((uintDouble)t.get_safe(i) * (uintDouble)n_p) & mask);

			bgint prod(m);
			prod = bga::mul(prod, n);
			prod = bga::lshift(prod, i * Bits);

			t = bga::add_mag(t, prod);
		}

		t = bga::rshift(t, total_bits);

		return (t >= n) ? bga::sub_mag(t, n) : t;
	}

	/**
	 * @brief Coarsely Integrated Operand Scanning (CIOS) Multiplication.
	 */
	bgint mul_cios(const bgint &a, const bgint &b) const
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

		return (t >= n) ? bga::sub_mag(t, n) : t;
	}

	/**
	 * @brief Finely Integrated Operand Scanning (FIOS) Multiplication.
	 */
	bgint mul_fios(const bgint &a, const bgint &b) const
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
		 *   t[j]      n[j]   a[j]
		 *     \       |      |
		 *      \      |      |
		 *       V     V      V
		 *        +----------------+
		 *        |                |
		 *   <----|                |<---- (c_mul, c_red)
		 *        |                |
		 *   <----|       PE       |<---- b[i]
		 *        |                |
		 *   <----|                |<---- m
		 *        |                |
		 *        +----+------+----+
		 *             |      |     \
		 *             |      |      \
		 *             V      V       \
		 *                             t[j-1]
		 */
		for(size_t i = 0; i < n_chunks; i++) {
			uint b_i = b.get_safe(i);

			uintDouble tmp_mult = (uintDouble)t.get(0) + (uintDouble)a.get_safe(0) * (uintDouble)b_i;

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

		return (t >= n) ? bga::sub_mag(t, n) : t;
	}

	/**
	 * @brief Performs modular exponentiation in Montgomery domain using binary exponentiation.
	 * @param base_m Base in montgomery domain
	 * @param exp Exponent
	 * @return BigInt<Bits> (base_m^exp_m)
	 */
	bgint pow(bgint base_m, const bgint &exp) const
	{
		bgint res_m = init(bgint(1));

		for(const auto &chunk : exp) {
			for(size_t j = 0; j < Bits; j++) {
				if((chunk >> j) & 1) {
					res_m = mul(res_m, base_m);
				}
				base_m = mul(base_m, base_m);
			}
		}
		return res_m;
	}

	/**
	 * @brief Performs modular inverse in Montgomery domain using Fermat's little theorem
	 * @param a_m First operand
	 * @return BigInt<Bits> (a_m^-1 mod N)
	 */
	bgint inv(bgint a_m) const
	{
		// assertm((mda::get_primalityTester<Bits, MONT_ALGO::FIOS>().is_prime(this->n)),
		// 	"Inversion does not work on non prime moduly. Num tested {}", this->n);
		const bgint two(2);
		bgint exp = bga::sub_mag(n, two); // a_m^(n - 2)

		return pow(a_m, exp);
	}
};

/**
 * @brief Performs modular multiplication (ab mod q) using Montgomery multiplication.
 * @tparam Bits Number of bits per chunk.
 * @tparam algo The Montgomery algorithm variant to use.
 * @param a The first operand.
 * @param b The second operand.
 * @param q The modulus.
 * @return BigInt<Bits> representing (ab mod q).
 */
template <size_t Bits, MONT_ALGO algo = MONT_ALGO::FIOS, bool Normalize = false>
BigInt<Bits> mul(BigInt<Bits> a, BigInt<Bits> b, BigInt<Bits> q)
{
	Montgomery<Bits, algo> mont(q);

	auto a_m = mont.init(a, Normalize);
	auto b_m = mont.init(b, Normalize);
	auto c_m = mont.mul(a_m, b_m);

	return mont.trans_back(c_m);
}

/**
 * @brief Performs modular exponentiation using binary exponentiation.
 * @param base Base
 * @param exp  Exponent
 * @param N    Modulus
 * @return BigInt<Bits> (base_m^exp_m mod N)
 */
template <size_t Bits, MONT_ALGO algo = MONT_ALGO::FIOS>
BigInt<Bits> pow(BigInt<Bits> base, BigInt<Bits> exp, BigInt<Bits> N)
{
	Montgomery<Bits, algo> mont(N);

	auto base_m = mont.init(base);
	auto res_m = mont.pow(base_m, exp);

	return mont.trans_back(res_m);
}

/**
 * @brief Performs modular inverse using Fermat's little theorem
 * @param a First operand
 * @param N Modulus (must be prime)
 * @return BigInt<Bits> (a_m^-1 mod N)
 */
template <size_t Bits, MONT_ALGO algo = MONT_ALGO::FIOS>
BigInt<Bits> inverse(BigInt<Bits> a, BigInt<Bits> N)
{
	if(a >= N) {
		a = mda::mod(a, N);
	}
	Montgomery<Bits, algo> mont(N);

	auto a_m = mont.init(a);

	auto res_m = mont.inv(a_m);

	return mont.trans_back(res_m);
}

} // namespace mda
