#include <concepts>
#include <cstdlib>

namespace ecc
{

template <typename C, typename P>
concept EllipticCurveConcept = requires(C curve, P point) {
	{ C::bits } -> std::convertible_to<size_t>;

	{ curve.add(point, point) } -> std::same_as<P>;
	{ curve.dbl(point) } -> std::same_as<P>;

	// { curve.is_on_curve(P) } -> std::convertible_to<bool>;
};

} // namespace ecc

#include "big_arth.hpp"
#include "mod_arth.hpp"

namespace ecc
{

template <size_t Bits>
struct AffinePoint {
	using FieldT = bga::BigInt<Bits>;

	FieldT x, y;
	bool is_inf = true;

	AffinePoint() = default;
	AffinePoint(FieldT x, FieldT y)
	    : x(std::move(x)), y(std::move(y)), is_inf(false) {};

	bool operator==(const AffinePoint &other) const
	{
		if(is_inf != other.is_inf) return false;
		if(is_inf) return true;

		return bga::sub(this->x, other.x).is_zero() &&
		       bga::sub(this->y, other.y).is_zero();
	}
};

template <size_t Bits>
struct JacobianPoint {
	using FieldT = bga::BigInt<Bits>;

	FieldT X, Y, Z;

	JacobianPoint() : X(1), Y(1), Z(0) {};

	JacobianPoint(FieldT x, FieldT y, FieldT z)
	    : X(std::move(x)), Y(std::move(y)), Z(std::move(z)) {};
};

template <size_t Bits, mda::MONT_ALGO mont_algo = mda::MONT_ALGO::FIOS>
class ShortWeierstrassCurve
{
public:
	using FieldT = bga::BigInt<Bits>;

	static constexpr size_t bits = Bits;

	// coeff of curve: y^2 = x^3 + ax + b mod modulus
	FieldT a, b, modulus;

	mda::Montgomery<Bits, mont_algo> mont;
	FieldT a_m, b_m;

	ShortWeierstrassCurve(FieldT a, FieldT b, FieldT mod)
	    : a(a), b(b), modulus(mod), mont(mod),
	      a_m(mont.init(a)), b_m(mont.init(b)) {};

	auto add(const AffinePoint<Bits> &P, const AffinePoint<Bits> &Q) const -> AffinePoint<Bits>
	{
		if(P.is_inf) return Q;
		if(Q.is_inf) return P;

		auto Px_m = mont.init(P.x);
		auto Py_m = mont.init(P.y);
		auto Qx_m = mont.init(Q.x);
		auto Qy_m = mont.init(Q.y);

		FieldT diff_x = mda::sub(Qx_m, Px_m, modulus);
		FieldT diff_y = mda::sub(Qy_m, Py_m, modulus);

		if(diff_x.is_zero()) {
			if(diff_y.is_zero()) return dbl(P);
			return AffinePoint<Bits>();
		}

		auto den = mont.inv(diff_x);
		auto lambda_m = mont.mul(diff_y, den);

		auto R_m = solve_affine(Px_m, Py_m, Qx_m, lambda_m);

		return AffinePoint<Bits>(
		    std::move(mont.trans_back(R_m.x)),
		    std::move(mont.trans_back(R_m.y)));
	}

	auto dbl(const AffinePoint<Bits> &P) const -> AffinePoint<Bits>
	{
		if(P.is_inf) return P;

		auto Px_m = mont.init(P.x);
		auto Py_m = mont.init(P.y);

		const auto two_m = mont.init(2);
		const auto three_m = mont.init(3);

		auto num = mda::add(
		    mont.mul(three_m, mont.mul(Px_m, Px_m)),
		    a_m, modulus);

		auto den = mont.inv(mont.mul(two_m, Py_m));
		auto lambda_m = mont.mul(num, den);

		auto R_m = solve_affine(Px_m, Py_m, Px_m, lambda_m);

		return AffinePoint<Bits>(
		    std::move(mont.trans_back(R_m.x)),
		    std::move(mont.trans_back(R_m.y)));
	}

	auto add(const JacobianPoint<Bits> &P, const JacobianPoint<Bits> &Q) const -> JacobianPoint<Bits>
	{
		if(P.Z.is_zero()) return Q;
		if(Q.Z.is_zero()) return P;

		JacobianPoint<Bits> Pm{
			std::move(mont.init(P.X)),
			std::move(mont.init(P.Y)),
			std::move(mont.init(P.Z))
		};

		JacobianPoint<Bits> Qm{
			std::move(mont.init(Q.X)),
			std::move(mont.init(Q.Y)),
			std::move(mont.init(Q.Z))
		};

		auto Z1sq_m = mont.mul(Pm.Z, Pm.Z);
		auto Z2sq_m = mont.mul(Qm.Z, Qm.Z);

		auto U1_m = mont.mul(Pm.X, Z2sq_m);
		auto U2_m = mont.mul(Qm.X, Z1sq_m);

		auto S1_m = mont.mul(Pm.Y, mont.mul(Qm.Z, Z2sq_m));
		auto S2_m = mont.mul(Qm.Y, mont.mul(Pm.Z, Z1sq_m));

		auto H_m = mda::sub(U2_m, U1_m, modulus);

		const FieldT four_m = mont.init(4);
		const FieldT two_m = mont.init(2);

		auto I_m = mont.mul(four_m, mont.mul(H_m, H_m));
		auto J_m = mont.mul(H_m, I_m);
		auto r_m = mont.mul(two_m, mda::sub(S2_m, S1_m, modulus));

		auto V_m = mont.mul(U1_m, I_m);

		auto X3_m = mda::sub(mda::sub(mont.mul(r_m, r_m), J_m, modulus), mont.mul(two_m, V_m), modulus);
		auto Y3_m = mda::sub(mont.mul(r_m, mda::sub(V_m, X3_m, modulus)), mont.mul(two_m, mont.mul(S1_m, J_m)), modulus);

		auto Z1pZ2 = mda::add(Pm.Z, Qm.Z, modulus);
		auto Z1pZ2sq = mont.mul(Z1pZ2, Z1pZ2);

		auto Z3_m = mont.mul(mda::sub(mda::sub(Z1pZ2sq, Z1sq_m, modulus), Z2sq_m, modulus), H_m);

		return JacobianPoint<Bits>{
			std::move(mont.trans_back(X3_m)),
			std::move(mont.trans_back(Y3_m)),
			std::move(mont.trans_back(Z3_m))
		};
	}

	auto dbl(const JacobianPoint<Bits> &P) const -> JacobianPoint<Bits>
	{
		if(P.Z.is_zero()) return P;

		auto Pm = JacobianPoint<Bits>{
			std::move(mont.init(P.X)),
			std::move(mont.init(P.Y)),
			std::move(mont.init(P.Z))
		};

		auto Xsq_m = mont.mul(Pm.X, Pm.X);
		auto Ysq_m = mont.mul(Pm.Y, Pm.Y);
		auto Y4_m = mont.mul(Ysq_m, Ysq_m);
		auto Zsq_m = mont.mul(Pm.Z, Pm.Z);

		const auto two_m = mont.init(2);
		const auto three_m = mont.init(3);
		const auto eight = mont.init(8);

		auto X1Y_m = mda::add(Pm.X, Ysq_m, modulus);
		auto X1Ysq_m = mont.mul(X1Y_m, X1Y_m);

		auto S_m = mont.mul(two_m, mda::sub(mda::sub(X1Ysq_m, Xsq_m, modulus), Y4_m, modulus));

		auto M_m = mda::add(mont.mul(three_m, Xsq_m), mont.mul(a_m, mont.mul(Zsq_m, Zsq_m)), modulus);

		auto T_m = mda::sub(mont.mul(M_m, M_m), mont.mul(two_m, S_m), modulus);

		auto Y3_m = mda::sub(mont.mul(M_m, mda::sub(S_m, T_m, modulus)), mont.mul(eight, Y4_m), modulus);

		auto Y1Z1 = mda::add(Pm.Y, Pm.Z, modulus);
		auto Y1Z1sq = mont.mul(Y1Z1, Y1Z1);
		auto Z3_m = mda::sub(mda::sub(Y1Z1sq, Ysq_m, modulus), Zsq_m, modulus);

		return JacobianPoint<Bits>{
			std::move(mont.trans_back(T_m)),
			std::move(mont.trans_back(Y3_m)),
			std::move(mont.trans_back(Z3_m))
		};
	}

	auto to_affine(const JacobianPoint<Bits> &p) const -> AffinePoint<Bits>
	{
		if(p.Z.is_zero()) return AffinePoint<Bits>{};

		auto Pm = JacobianPoint<Bits>{
			std::move(mont.init(p.X)),
			std::move(mont.init(p.Y)),
			std::move(mont.init(p.Z))
		};

		auto z2 = mont.mul(Pm.Z, Pm.Z);
		auto z3 = mont.mul(z2, Pm.Z);

		auto x = mont.mul(Pm.X, mont.inv(z2));
		auto y = mont.mul(Pm.Y, mont.inv(z3));

		return AffinePoint<Bits>{
			std::move(mont.trans_back(x)),
			std::move(mont.trans_back(y))
		};
	}

	auto is_on_curve(const AffinePoint<Bits> &p) const -> bool
	{
		if(p.is_inf) return true;

		auto Pm = AffinePoint<Bits>{
			std::move(mont.init(p.x)),
			std::move(mont.init(p.y)),
		};

		auto lhs = mont.mul(Pm.y, Pm.y);

		auto x2 = mont.mul(Pm.x, Pm.x);
		auto x3 = mont.mul(x2, Pm.x);
		auto ax = mont.mul(a_m, Pm.x);
		auto rhs = mda::add(x3, ax, modulus);
		rhs = mda::add(rhs, b_m, modulus);

		return lhs == rhs;
	}

private:
	auto solve_affine(const FieldT &Px_m, const FieldT &Py_m,
			  const FieldT &Qx_m, const FieldT &lam_m)
	    const -> AffinePoint<Bits>
	{
		// x3 = lambda^2 - x1 (P) - x2 (Q) mod q
		auto x3 = mont.mul(lam_m, lam_m);
		x3 = mda::sub(x3, Px_m, modulus);
		x3 = mda::sub(x3, Qx_m, modulus);

		// -y3 = lambda * (x3 - x1 (P)) + y1 (P)
		// y3 = lambda * (x1 (P) - x3) - y1 (P)
		auto y3 = mda::sub(Px_m, x3, modulus);
		y3 = mont.mul(lam_m, y3);
		y3 = mda::sub(y3, Py_m, modulus);

		return AffinePoint<Bits>(
		    std::move(x3), std::move(y3));
	}
};

static_assert(EllipticCurveConcept<ShortWeierstrassCurve<32>, AffinePoint<32>>,
	      "Concept Error: ShortWeierstrassCurve and AffinePoint does not meet requirements");

static_assert(EllipticCurveConcept<ShortWeierstrassCurve<32>, JacobianPoint<32>>,
	      "Concept Error: ShortWeierstrassCurve and JacobianPoint does not meet requirements");

} // namespace ecc
