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
#include "tests/utils/helpers.hpp"

template <typename T>
consteval std::string_view type_name()
{
	std::string_view name = __PRETTY_FUNCTION__;

	size_t start = name.find("T = ");
	if(start == std::string_view::npos) return "Unknown";
	start += 4;

	size_t end = name.find_last_of('<');

	return name.substr(start, end - start);
}

template <template <size_t> typename PointType, size_t N>
auto make_point(const bga::BigInt<N> &x, const bga::BigInt<N> &y)
{
	using P = PointType<N>;
	if constexpr(std::is_same_v<P, ecc::AffinePoint<N>>) {
		return P(x, y);
	} else if constexpr(std::is_same_v<P, ecc::JacobianPoint<N>>) {
		return P(x, y, bga::BigInt<N>(1));
	} else if constexpr(std::is_same_v<P, ecc::ProjectivePoint<N>>) {
		return P(x, y, bga::BigInt<N>(1));
	} else if constexpr(std::is_same_v<P, ecc::XYZZPoint<N>>) {
		return P(x, y, bga::BigInt<N>(1), bga::BigInt<N>(1));
	}
	std::unreachable();
}

struct TestECCSmall_SWC {
	template <size_t N>
	void operator()(std::mt19937_64 &gen) const noexcept
	{
		using namespace ecc;
		using FieldT = bga::BigInt<N>;

		ShortWeierstrassCurve<N> curve(FieldT(2), FieldT(2), FieldT(17));
		AffinePoint<N> P(FieldT(5), FieldT(1));

		test_assert("SmallCurve on curve test", N,
			    curve.is_on_curve(P), true,
			    "Point (5, 1) should be on curve");

		auto P2 = curve.dbl(P);
		test_assert("SmallCurve doubling", N,
			    (P2.x == FieldT(6) && P2.y == FieldT(3)), true,
			    "2P expected (6, 3) got P2 ({}, {})", P2.x, P2.y);
	}
};

struct TestECCSmall_TE {
	template <size_t N>
	void operator()(std::mt19937_64 &gen) const noexcept
	{
		using namespace ecc;
		using FieldT = bga::BigInt<N>;

		FieldT mod_p(17), a(1), d(1);
		TwistedEdwardsCurve<N> curve(a, d, mod_p);

		FieldT x(1), y(2);

		FieldT t = mda::mul(x, y, mod_p);

		ExtProjPoint<N> P(x, y, FieldT(1), t);

		test_assert("Small TE on curve", N,
			    curve.is_on_curve(P), true,
			    "Point (1, 2) should be on curve");

		auto P_aff = curve.to_affine(P);
		test_assert("Small TE to_affine", N,
			    (P_aff.x == x && P_aff.y == y), true,
			    "Affine conversion mismatch");

		auto P2 = curve.dbl(P);
		auto P2_aff = curve.to_affine(P2);

		test_assert("Small TE doubling", N,
			    (P2_aff.x == FieldT(11) && P2_aff.y == FieldT(16)), true,
			    "2P expected (11, 16) got ({}, {})", P2_aff.x, P2_aff.y);

		auto P_plus_P = curve.add(P, P);
		auto P_plus_P_aff = curve.to_affine(P_plus_P);

		test_assert("Small TE Add vs Dbl", N,
			    (P_plus_P_aff == P2_aff), true,
			    "P+P should equal 2P");

		auto P_mul_2 = scalarMul(curve, P, FieldT(2));
		auto P_mul_2_aff = curve.to_affine(P_mul_2);

		test_assert("Small TE ScalarMul", N,
			    (P_mul_2_aff == P2_aff), true,
			    "scalarMul(P, 2) should equal 2P");
	}
};

template <
    template <size_t> typename CurveT,
    template <size_t> typename PointT>
struct TestECCArithmetic {
	template <size_t N>
	void operator()(std::mt19937_64 &) const
	{
		assertm(false, "You shouldn't run this");
	}
};

template <template <size_t> typename PointT>
struct TestECCArithmetic<ecc::ShortWeierstrassCurve, PointT> {
	static constexpr std::string_view point_name = type_name<PointT<0>>();

	static_assert(ecc::EllipticCurveConcept<ecc::ShortWeierstrassCurve<32>, ecc::AffinePoint<32>>,
		      "Concept Error: ShortWeierstrassCurve and AffinePoint does not meet requirements");

	static_assert(ecc::EllipticCurveConcept<ecc::ShortWeierstrassCurve<32>, ecc::JacobianPoint<32>>,
		      "Concept Error: ShortWeierstrassCurve and JacobianPoint does not meet requirements");

	static_assert(ecc::EllipticCurveConcept<ecc::ShortWeierstrassCurve<32>, ecc::XYZZPoint<32>>,
		      "Concept Error: ShortWeierstrassCurve and XYZZ does not meet requirements");

	template <size_t N>
	void operator()(std::mt19937_64 &gen) const noexcept
	{
		using namespace ecc;
		using FieldT = bga::BigInt<N>;

		const auto &primes = StaticPrimes<N>::primes_list;

		std::uniform_int_distribution<size_t> p_dist(0, StaticPrimes<N>::count() - 1);
		size_t p_idx = p_dist(gen);

		FieldT mod_p = primes[p_idx];
		size_t mod_p_size = StaticPrimes<N>::get_length(p_idx);

		auto rng = genRandBgN(mod_p_size - 3, gen);
		FieldT a{ rng() };

		FieldT Gx{ rng() };
		FieldT Gy{ rng() };

		ShortWeierstrassCurve<N> tmp_curve{ a, FieldT(0), mod_p };

		auto Gx_m = tmp_curve.mont.init(Gx);
		auto Gy_m = tmp_curve.mont.init(Gy);
		auto Gy2 = tmp_curve.mont.mul(Gy_m, Gy_m);
		auto Gx2 = tmp_curve.mont.mul(Gx_m, Gx_m);
		auto Gx3 = tmp_curve.mont.mul(Gx2, Gx_m);
		auto ax = tmp_curve.mont.mul(tmp_curve.a_m, Gx_m);

		auto rhs = mda::add(Gx3, ax, mod_p);
		auto b_m = mda::sub(Gy2, rhs, mod_p);
		FieldT b = tmp_curve.mont.trans_back(b_m);

		ShortWeierstrassCurve<N> curve{ a, b, mod_p };

		AffinePoint<N> G_truth{ Gx, Gy };

		test_assert("ECC G on curve", N,
			    curve.is_on_curve(G_truth), true, "G should be on curve");

		auto G_test = make_point<PointT, N>(Gx, Gy);

		auto G2_truth = curve.dbl(G_truth);
		auto G2_test = curve.dbl(G_test);

		auto G2_norm = curve.to_affine(G2_test);

		test_assert(
		    std::format("ECC Doubling {}", point_name), N,
		    (G2_norm == G2_truth), true,
		    "Doubling failed for mod {}", mod_p);

		test_assert(
		    std::format("ECC 2G on the curve {}", point_name), N,
		    curve.is_on_curve(G2_norm), true,
		    "Should be on the curve");

		auto G3_truth = curve.add(G2_truth, G_truth);
		auto G3_test = curve.add(G2_test, G_test);

		auto G3_norm = curve.to_affine(G3_test);

		test_assert(std::format("ECC Addition {}", point_name), N,
			    G3_norm == G3_truth, true,
			    "Addition failed (3G) for mod {}", mod_p);

		test_assert(
		    std::format("ECC 3G on curve {}", point_name), N,
		    curve.is_on_curve(G3_norm), true,
		    "Should be on the curve");
	}
};

template <template <size_t> typename PointT>
struct TestECCArithmetic<ecc::TwistedEdwardsCurve, PointT> {

	static constexpr std::string_view point_name = type_name<PointT<0>>();

	static_assert(std::is_same_v<PointT<32>, ecc::ExtProjPoint<32>>,
		      "TwistedEdwards only supports ExtProjPoint for now");

	template <size_t N>
	void operator()(std::mt19937_64 &gen) const noexcept
	{
		using namespace ecc;
		using FieldT = bga::BigInt<N>;

		const auto &primes = StaticPrimes<N>::primes_list;
		std::uniform_int_distribution<size_t> p_dist(0, StaticPrimes<N>::count() - 1);

		size_t p_idx = p_dist(gen);

		FieldT mod_p = primes[p_idx];
		size_t mod_p_size = StaticPrimes<N>::get_length(p_idx);

		auto rng = genRandBgN(mod_p_size - 1, gen);

		FieldT x = rng();
		FieldT y = rng();
		while(x >= mod_p || x.is_zero()) x = rng();
		while(y >= mod_p || y.is_zero()) y = rng();

		FieldT a = rng();
		while(a >= mod_p) a = rng();

		mda::Montgomery<N> mont(mod_p);

		auto xm = mont.init(x);
		auto ym = mont.init(y);
		auto am = mont.init(a);

		auto x2 = mont.mul(xm, xm);
		auto y2 = mont.mul(ym, ym);
		auto x2y2 = mont.mul(x2, y2);

		if(x2y2.is_zero()) return;

		auto lhs = mda::add(mont.mul(am, x2), y2, mod_p);
		lhs = mda::sub(lhs, mont.init(1), mod_p);

		auto den_inv = mont.inv(x2y2);
		auto dm = mont.mul(lhs, den_inv);
		FieldT d = mont.trans_back(dm);

		TwistedEdwardsCurve<N> curve(a, d, mod_p);

		FieldT t = mda::mul(x, y, mod_p);
		ExtProjPoint<N> P(x, y, FieldT(1), t);

		test_assert(std::format("TE On Curve {}", point_name), N,
			    curve.is_on_curve(P), true,
			    "Generated point should be on curve");

		auto P2 = curve.dbl(P);
		auto P_plus_P = curve.add(P, P);

		auto P2_aff = curve.to_affine(P2);
		auto P_plus_P_aff = curve.to_affine(P_plus_P);

		test_assert(std::format("TE Dbl vs Add {}", point_name), N,
			    P2_aff, P_plus_P_aff,
			    "dbl(P) should be equal to add(P, P)");

		test_assert(std::format("TE 2P On curve {}", point_name), N,
			    (curve.is_on_curve(P2)), true,
			    "2P should be on the curve");
	}
};

template <
    template <size_t> typename CurveT,
    template <size_t> typename PointT>
struct TestECCscalarMul {
	template <size_t N>
	void operator()(std::mt19937_64 &) const
	{
		assertm(false, "You shouldn't run this");
	}
};

template <template <size_t> typename PointT>
struct TestECCscalarMul<ecc::ShortWeierstrassCurve, PointT> {
	static constexpr std::string_view point_name = type_name<PointT<0>>();

	static_assert(ecc::EllipticCurveConcept<ecc::ShortWeierstrassCurve<32>, ecc::AffinePoint<32>>,
		      "Concept Error: ShortWeierstrassCurve and AffinePoint does not meet requirements");

	static_assert(ecc::EllipticCurveConcept<ecc::ShortWeierstrassCurve<32>, ecc::JacobianPoint<32>>,
		      "Concept Error: ShortWeierstrassCurve and JacobianPoint does not meet requirements");

	static_assert(ecc::EllipticCurveConcept<ecc::ShortWeierstrassCurve<32>, ecc::ProjectivePoint<32>>,
		      "Concept Error: ShortWeierstrassCurve and Projective does not meet requirements");

	static_assert(ecc::EllipticCurveConcept<ecc::ShortWeierstrassCurve<32>, ecc::XYZZPoint<32>>,
		      "Concept Error: ShortWeierstrassCurve and XYZZ does not meet requirements");

	template <size_t N>
	void operator()(std::mt19937_64 &gen) const noexcept
	{
		using namespace ecc;
		using FieldT = bga::BigInt<N>;

		const FieldT prime("2824971001");
		FieldT a("22457757");

		FieldT b("1070153296");

		ShortWeierstrassCurve<N> curve{ a, b, prime };

		auto G_test = make_point<PointT, N>(FieldT("1596109"), FieldT("4098011"));

		auto result = scalarMul(curve, G_test, FieldT("594"));

		auto res_norm = curve.to_affine(result);

		test_assert(
		    std::format("ECC scalar mul on curve {}", point_name), N,
		    curve.is_on_curve(res_norm), true,
		    "Should be on the curve ({}, {})", res_norm.x, res_norm.y);

		test_assert(std::format("ECC scalarMul {}", point_name), N,
			    (res_norm.x == FieldT("1934875196")) && (res_norm.y == FieldT("1139617403")), true,
			    "Should have been equal. Got ({}, {})", res_norm.x, res_norm.y);
	}
};

template <template <size_t> typename PointT>
struct TestECCscalarMul<ecc::TwistedEdwardsCurve, PointT> {
	static constexpr std::string_view point_name = type_name<PointT<0>>();

	template <size_t N>
	void operator()(std::mt19937_64 &gen) const noexcept
	{
		using namespace ecc;
		using FieldT = bga::BigInt<N>;

		const auto &primes = StaticPrimes<N>::primes_list;
		std::uniform_int_distribution<size_t> p_dist(0, StaticPrimes<N>::count() - 1);
		size_t p_idx = p_dist(gen);
		FieldT mod_p = primes[p_idx];

		auto rng = genRandBgN(StaticPrimes<N>::get_length(p_idx) - 1, gen);
		FieldT x = rng(), y = rng(), a = rng();
		while(x >= mod_p || x.is_zero()) x = rng();
		while(y >= mod_p || y.is_zero()) y = rng();
		while(a >= mod_p) a = rng();

		mda::Montgomery<N> mont(mod_p);
		auto xm = mont.init(x);
		auto ym = mont.init(y);
		auto am = mont.init(a);

		auto x2y2 = mont.mul(mont.mul(xm, xm), mont.mul(ym, ym));
		if(x2y2.is_zero()) return;

		auto lhs = mda::sub(mda::add(mont.mul(am, mont.mul(xm, xm)), mont.mul(ym, ym), mod_p), mont.init(1), mod_p);

		FieldT d = mont.trans_back(mont.mul(lhs, mont.inv(x2y2)));

		TwistedEdwardsCurve<N> curve(a, d, mod_p);

		FieldT t = mda::mul(x, y, mod_p);
		ExtProjPoint<N> P(x, y, FieldT(1), t);

		auto P_mul_2 = scalarMul(curve, P, FieldT(2));
		auto P_dbl = curve.dbl(P);

		auto aff1 = curve.to_affine(P_mul_2);
		auto aff2 = curve.to_affine(P_dbl);

		test_assert(std::format("TE ScalarMul vs DBl {}", point_name), N,
			    aff1, aff2,
			    "scalarMul(P, 2) should match dbl(P)");
	}
};

void register_ecc_tests()
{
	TESTS.register_test<BITS_TEST_MOD___N>(
	    "ECC ShortWeierstrass simple tests",
	    1,
	    TestECCSmall_SWC{});

	TESTS.register_test<BITS_TEST_MOD___N>(
	    "ECC TwistedEdwards simple tests",
	    1,
	    TestECCSmall_TE{});

	TESTS.register_test<BITS_TEST_MOD___N>(
	    "ECC ShortWeierstrass Affine arth",
	    400,
	    TestECCArithmetic<ecc::ShortWeierstrassCurve, ecc::AffinePoint>{});

	TESTS.register_test<BITS_TEST_MOD___N>(
	    "ECC ShortWeierstrass Projective arth",
	    400,
	    TestECCArithmetic<ecc::ShortWeierstrassCurve, ecc::ProjectivePoint>{});

	TESTS.register_test<BITS_TEST_MOD___N>(
	    "ECC ShortWeierstrass Jacobian arth",
	    400,
	    TestECCArithmetic<ecc::ShortWeierstrassCurve, ecc::JacobianPoint>{});

	TESTS.register_test<BITS_TEST_MOD___N>(
	    "ECC ShortWeierstrass XYZZ arth",
	    400,
	    TestECCArithmetic<ecc::ShortWeierstrassCurve, ecc::XYZZPoint>{});

	TESTS.register_test<BITS_TEST_MOD___N>(
	    "ECC TwistedEdwards ExtProj arth",
	    400,
	    TestECCArithmetic<ecc::TwistedEdwardsCurve, ecc::ExtProjPoint>{});

	TESTS.register_test<BITS_TEST_MOD___N>(
	    "ECC SWC Affine scalarMull",
	    1,
	    TestECCscalarMul<ecc::ShortWeierstrassCurve, ecc::AffinePoint>{});

	TESTS.register_test<BITS_TEST_MOD___N>(
	    "ECC SWC Projective scalarMull",
	    1,
	    TestECCscalarMul<ecc::ShortWeierstrassCurve, ecc::ProjectivePoint>{});

	TESTS.register_test<BITS_TEST_MOD___N>(
	    "ECC SWC Jacobian scalarMull",
	    1,
	    TestECCscalarMul<ecc::ShortWeierstrassCurve, ecc::JacobianPoint>{});

	TESTS.register_test<BITS_TEST_MOD___N>(
	    "ECC SWC XYZZ scalarMull",
	    1,
	    TestECCscalarMul<ecc::ShortWeierstrassCurve, ecc::XYZZPoint>{});

	TESTS.register_test<BITS_TEST_MOD___N>(
	    "ECC SWC TwistedEdwards ExtProj scalarMull",
	    1,
	    TestECCscalarMul<ecc::TwistedEdwardsCurve, ecc::ExtProjPoint>{});
}
