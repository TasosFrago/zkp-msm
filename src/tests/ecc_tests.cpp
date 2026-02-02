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
	}
	std::unreachable();
}

template <typename Curve, typename Point>
	requires ecc::EllipticCurveConcept<Curve, Point>
auto normalize(const Curve &curve, const Point &point)
{
	constexpr size_t Bits = Curve::bits;
	if constexpr(std::is_same_v<Point, ecc::AffinePoint<Bits>>) {
		return point;
	} else if constexpr(std::is_same_v<Point, ecc::JacobianPoint<Bits>>) {
		return curve.to_affine(point);
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

		auto G2_norm = normalize(curve, G2_test);

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

		auto G3_norm = normalize(curve, G3_test);

		test_assert(std::format("ECC Addition {}", point_name), N,
			    G3_norm == G3_truth, true,
			    "Addition failed (3G) for mod {}", mod_p);

		test_assert(
		    std::format("ECC 3G on curve {}", point_name), N,
		    curve.is_on_curve(G3_norm), true,
		    "Should be on the curve");
	}
};

void register_ecc_tests()
{
	TESTS.register_test<BITS_TEST_MOD___N>(
	    "ECC ShortWeierstrass simple tests",
	    1,
	    TestECCSmall_SWC{});

	TESTS.register_test<BITS_TEST_MOD___N>(
	    "ECC ShortWeierstrass Affine arth",
	    400,
	    TestECCArithmetic<ecc::ShortWeierstrassCurve, ecc::AffinePoint>{});

	TESTS.register_test<BITS_TEST_MOD___N>(
	    "ECC ShortWeierstrass Jacobian arth",
	    400,
	    TestECCArithmetic<ecc::ShortWeierstrassCurve, ecc::JacobianPoint>{});
}
