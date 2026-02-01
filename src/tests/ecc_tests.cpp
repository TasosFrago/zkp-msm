#include <cstdint>
#include <print>
#include <random>
#include <string>

#include "Primes.hpp"
#include "mda_primetest.hpp"

#include "bigint.hpp"
#include "ecc.hpp"

#include "test_framework.hpp"
#include "tests.hpp"

struct TestECCLogic {
	template <size_t N>
	void operator()(std::mt19937_64 &gen) const noexcept
	{
		// StaticPrimes<N>::primes_list;
		using namespace ecc;
		using FieldT = bga::BigInt<N>;

		// ----------------------------------------------------------------
		// 1. Small Curve Test: y^2 = x^3 + 2x + 2 mod 17
		// Known points: (5, 1), (5, 16), (6, 3), (6, 14), ...
		// ----------------------------------------------------------------
		FieldT a(2), b(2), p(17);
		ShortWeierstrassCurve<N> small_curve(a, b, p);

		// P = (5, 1)
		AffinePoint<N> P(FieldT(5), FieldT(1));
		AffinePoint<N> Inf;

		// Test 1: Is P on curve?
		// Note: We need to pass P with coords in Mont domain to is_on_curve because it uses mont.mul
		test_assert("ECC small on_curve", N, small_curve.is_on_curve(P), true, "Point (5,1) should be on curve");

		// test_assert("Test", true, false, "Should fail");

		// Test 2: P + Inf = P
		auto R1 = small_curve.add(P, Inf);
		test_assert("ECC small identity", N, R1 == P, true, "P + Inf should be P");

		// Test 3: Doubling P(5,1)
		// Slope lambda = (3x^2 + a) / 2y
		// x = 5, y = 1
		// num = 3(25) + 2 = 77 = 9 mod 17
		// den = 2(1) = 2 mod 17
		// inv(2) mod 17 = 9
		// lambda = 9 * 9 = 81 = 13 mod 17
		// x3 = lambda^2 - 2x = 169 - 10 = 16 - 10 = 6
		// y3 = lambda(x - x3) - y = 13(5 - 6) - 1 = 13(-1) - 1 = -13 - 1 = -14 = 3
		// Expected 2P = (6, 3)
		auto P2 = small_curve.dbl(P);
		test_assert("ECC small doubling", N,
			    (P2.x == FieldT(6) && P2.y == FieldT(3)), true,
			    "2P should be (6, 3). Got ({}, {})", P2.x, P2.y);

		// ----------------------------------------------------------------
		// 2. Jacobian vs Affine Consistency (Secp256k1-like parameters)
		// ----------------------------------------------------------------
		// Curve: y^2 = x^3 + 7 mod P_secp
		// P = 2^256 - 2^32 - 977 (Standard secp256k1 prime)
		// We will simulate a random curve if N is small, or use standard parameters if N >= 256
		// For generic testing, we'll generate random a, b, mod.

		std::uniform_int_distribution<uint64_t> dist;

		// Generate random modulus (ensure it's odd)
		// To make it simple for the test framework, we generate 3 64-bit chunks for parameters if N allows
		// uint64_t rand_MOD = dist(gen);
		// FieldT rand_mod = FieldT(rand_MOD);
		//
		// if constexpr(N > 64) rand_mod = bga::OR(bga::lshift(rand_mod, 64), FieldT(dist(gen)));
		// rand_mod = bga::OR(rand_mod, FieldT(1)); // Ensure odd
		// if(rand_mod < FieldT(5)) rand_mod = FieldT(17);
		// std::println("is prime: {}", mda::get_primalityTester<N, mda::MONT_ALGO::FIOS>().is_prime(FieldT("602965189904783654649202646197")));
		FieldT rand_mod("602965189904783654649202646197");

		FieldT rand_a = mda::mod(FieldT(dist(gen)), rand_mod);
		FieldT rand_b = mda::mod(FieldT(dist(gen)), rand_mod);

		ShortWeierstrassCurve<N> curve(rand_a, rand_b, rand_mod);

		// Generate a random valid point?
		// Generating a valid point on a random curve is hard (needs square root).
		// Instead: Pick x, y, then calculate b such that it is on curve.
		// y^2 = x^3 + ax + b  =>  b = y^2 - x^3 - ax
		FieldT Gx = mda::mod(FieldT(dist(gen)), rand_mod);
		FieldT Gy = mda::mod(FieldT(dist(gen)), rand_mod);

		// Calculate b to force point to be on curve
		auto Gx_m = curve.mont.init(Gx);
		auto Gy_m = curve.mont.init(Gy);
		auto Gy2 = curve.mont.mul(Gy_m, Gy_m);
		auto Gx2 = curve.mont.mul(Gx_m, Gx_m);
		auto Gx3 = curve.mont.mul(Gx2, Gx_m);
		auto ax = curve.mont.mul(curve.a_m, Gx_m);

		// b = y^2 - x^3 - ax
		auto rhs_part = mda::add(Gx3, ax, rand_mod);
		auto valid_b_m = mda::sub(Gy2, rhs_part, rand_mod);
		FieldT valid_b = curve.mont.trans_back(valid_b_m);

		// Re-initialize curve with the valid b
		ShortWeierstrassCurve<N> valid_curve(rand_a, valid_b, rand_mod);
		AffinePoint<N> G(Gx, Gy);

		// Verify generation
		test_assert("ECC gen point", N,
			    valid_curve.is_on_curve(AffinePoint<N>(Gx, Gy)), true,
			    "Generated G should be on curve");

		// ----------------------------------------------------------------
		// 3. Compare Affine vs Jacobian Addition/Doubling
		// ----------------------------------------------------------------

		// Jacobian Point G (Z=1)
		JacobianPoint<N> G_jac(Gx, Gy, FieldT(1));

		// Test: 2G (Affine) == 2G (Jacobian)
		auto G2_aff = valid_curve.dbl(G);
		auto G2_jac = valid_curve.dbl(G_jac);

		// The Jacobian result stores X, Y, Z in MONTGOMERY domain based on your impl
		// But let's check your impl:
		// JacobianPoint constructor takes (x,y,z).
		// Curve::dbl(Jacobian) -> returns JacobianPoint with raw values.
		// Your Jacobian dbl implementation does:
		// 		auto Pm = JacobianPoint<Bits>{ mont.init(P.X), ... }
		//      ... math ...
		//      return JacobianPoint{ mont.trans_back(T_m), ... }
		// So it accepts Standard inputs and returns Standard outputs. Perfect.

		auto G2_jac_as_aff = valid_curve.to_affine(
		    JacobianPoint<N>{
			G2_jac.X,
			G2_jac.Y,
			G2_jac.Z });

		test_assert("ECC dbl consistency", N,
			    G2_aff == G2_jac_as_aff, true,
			    "Affine and Jacobian doubling should match. Modululo = {}\n"
			    "{: ^8}Aff(x: {}, y: {}),\n"
			    "{: ^8}Jac_to_aff(x: {}, y: {})\n"
			    "{: ^8}Jac(X: {}, Y: {}, Z: {})",
			    valid_curve.modulus,
			    "", G2_aff.x, G2_aff.y,
			    "", G2_jac_as_aff.x, G2_jac_as_aff.y,
			    "", G2_jac.X, G2_jac.Y, G2_jac.Z);

		// Check if 2G is on curve
		test_assert("ECC 2G on curve", N,
			    valid_curve.is_on_curve(G2_aff), true,
			    "2G should be on curve");

		// Test: 3G = 2G + G
		auto G3_aff = valid_curve.add(G, G2_aff);
		auto G3_jac = valid_curve.add(G_jac, G2_jac);

		auto G3_jac_as_aff = valid_curve.to_affine(
		    JacobianPoint<N>{
			G3_jac.X,
			G3_jac.Y,
			G3_jac.Z });

		test_assert("ECC add consistency", N,
			    G3_aff == G3_jac_as_aff, true,
			    "Affine and Jacobian addition should match (3G)");

		// Check if 3G is on curve
		test_assert("ECC 3G on curve", N,
			    valid_curve.is_on_curve(G3_aff), true,
			    "3G should be on curve");
	}
};

void register_ecc_tests()
{
	TESTS.register_test<BITS_TEST_MOD___N>(
	    "Elliptic Curve Arithmetic",
	    MOD_BATCHES,
	    TestECCLogic{});
}
