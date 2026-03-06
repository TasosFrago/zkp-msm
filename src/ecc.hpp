#include <concepts>
#include <cstdlib>

namespace ecc
{
template <size_t Bits>
struct AffinePoint;

template <typename C, typename P>
concept EllipticCurveConcept = requires(C curve, P point) {
	{ C::bits } -> std::convertible_to<size_t>;
	{ P::bits } -> std::convertible_to<size_t>;

	{ curve.add(point, point) } -> std::same_as<P>;
	{ curve.add_m(point, point) } -> std::same_as<P>;
	{ curve.dbl(point) } -> std::same_as<P>;
	{ curve.dbl_m(point) } -> std::same_as<P>;

	{ curve.is_on_curve(point) } -> std::convertible_to<bool>;
	{ curve.to_affine(point) } -> std::same_as<AffinePoint<P::bits>>;

	{ point.mont(curve.mont) } -> std::same_as<P>;
	{ point.demont(curve.mont) } -> std::same_as<P>;
};

} // namespace ecc

#include "big_arth.hpp"
#include "mod_arth.hpp"

namespace ecc
{

template <size_t Bits>
struct AffinePoint {
	using FieldT = bga::BigInt<Bits>;
	static constexpr size_t bits = Bits;

	FieldT x, y;
	bool is_inf = true;

	AffinePoint() = default;
	AffinePoint(FieldT x, FieldT y)
	    : x(std::move(x)), y(std::move(y)), is_inf(false) {};
	AffinePoint(FieldT x, FieldT y, bool isInf)
	    : x(std::move(x)), y(std::move(y)), is_inf(isInf) {};

	template <mda::MONT_ALGO mont_algo = mda::MONT_ALGO::FIOS>
	AffinePoint<Bits> mont(const mda::Montgomery<Bits, mont_algo> &mont) const
	{
		return AffinePoint<Bits>{
			std::move(mont.init(this->x)),
			std::move(mont.init(this->y)),
			is_inf
		};
	}

	template <mda::MONT_ALGO mont_algo = mda::MONT_ALGO::FIOS>
	AffinePoint<Bits> demont(const mda::Montgomery<Bits, mont_algo> &mont) const
	{
		return AffinePoint<Bits>{
			std::move(mont.trans_back(this->x)),
			std::move(mont.trans_back(this->y)),
			is_inf
		};
	}

	bool operator==(const AffinePoint &other) const
	{
		if(is_inf != other.is_inf) return false;
		if(is_inf) return true;

		return bga::sub(this->x, other.x).is_zero() &&
		       bga::sub(this->y, other.y).is_zero();
	}
};

template <size_t Bits>
struct ProjectivePoint {
	using FieldT = bga::BigInt<Bits>;
	static constexpr size_t bits = Bits;

	FieldT X, Y, Z;

	ProjectivePoint() : X(0), Y(1), Z(0) {};
	ProjectivePoint(FieldT x, FieldT y, FieldT z)
	    : X(std::move(x)), Y(std::move(y)), Z(std::move(z)) {};

	template <mda::MONT_ALGO mont_algo = mda::MONT_ALGO::FIOS>
	ProjectivePoint<Bits> mont(const mda::Montgomery<Bits, mont_algo> &mont) const
	{
		return ProjectivePoint<Bits>{
			std::move(mont.init(this->X)),
			std::move(mont.init(this->Y)),
			std::move(mont.init(this->Z))
		};
	}

	template <mda::MONT_ALGO mont_algo = mda::MONT_ALGO::FIOS>
	ProjectivePoint<Bits> demont(const mda::Montgomery<Bits, mont_algo> &mont) const
	{
		return ProjectivePoint<Bits>{
			std::move(mont.trans_back(this->X)),
			std::move(mont.trans_back(this->Y)),
			std::move(mont.trans_back(this->Z))
		};
	}
};

template <size_t Bits>
struct JacobianPoint {
	using FieldT = bga::BigInt<Bits>;
	static constexpr size_t bits = Bits;

	FieldT X, Y, Z;

	JacobianPoint() : X(1), Y(1), Z(0) {};

	JacobianPoint(FieldT x, FieldT y, FieldT z)
	    : X(std::move(x)), Y(std::move(y)), Z(std::move(z)) {};

	template <mda::MONT_ALGO mont_algo = mda::MONT_ALGO::FIOS>
	JacobianPoint<Bits> mont(const mda::Montgomery<Bits, mont_algo> &mont) const
	{
		return JacobianPoint<Bits>{
			std::move(mont.init(this->X)),
			std::move(mont.init(this->Y)),
			std::move(mont.init(this->Z))
		};
	}

	template <mda::MONT_ALGO mont_algo = mda::MONT_ALGO::FIOS>
	JacobianPoint<Bits> demont(const mda::Montgomery<Bits, mont_algo> &mont) const
	{
		return JacobianPoint<Bits>{
			std::move(mont.trans_back(this->X)),
			std::move(mont.trans_back(this->Y)),
			std::move(mont.trans_back(this->Z))
		};
	}
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

	auto add_m(const AffinePoint<Bits> &Pm, const AffinePoint<Bits> &Qm) const -> AffinePoint<Bits>
	{
		if(Pm.is_inf) return Qm;
		if(Qm.is_inf) return Pm;

		FieldT diff_x = mda::sub(Qm.x, Pm.x, modulus);
		FieldT diff_y = mda::sub(Qm.y, Pm.y, modulus);

		if(diff_x.is_zero()) {
			if(diff_y.is_zero()) return dbl_m(Pm);
			return AffinePoint<Bits>();
		}

		auto den = mont.inv(diff_x);
		auto lambda_m = mont.mul(diff_y, den);

		// x3 = lambda^2 - x1 (P) - x2 (Q) mod q
		auto x3 = mont.mul(lambda_m, lambda_m);
		x3 = mda::sub(x3, Pm.x, modulus);
		x3 = mda::sub(x3, Qm.x, modulus);

		// -y3 = lambda * (x3 - x1 (P)) + y1 (P)
		// y3 = lambda * (x1 (P) - x3) - y1 (P)
		auto y3 = mda::sub(Pm.x, x3, modulus);
		y3 = mont.mul(lambda_m, y3);
		y3 = mda::sub(y3, Pm.y, modulus);

		return AffinePoint<Bits>{
			std::move(x3), std::move(y3)
		};
	}

	auto add(const AffinePoint<Bits> &P, const AffinePoint<Bits> &Q) const -> AffinePoint<Bits>
	{
		if(P.is_inf) return Q;
		if(Q.is_inf) return P;

		AffinePoint<Bits> Pm{
			std::move(mont.init(P.x)),
			std::move(mont.init(P.y)),
			P.is_inf
		};

		AffinePoint<Bits> Qm{
			std::move(mont.init(Q.x)),
			std::move(mont.init(Q.y)),
			Q.is_inf
		};

		auto Rm = add_m(Pm, Qm);

		// auto Px_m = mont.init(P.x);
		// auto Py_m = mont.init(P.y);
		// auto Qx_m = mont.init(Q.x);
		// auto Qy_m = mont.init(Q.y);

		// FieldT diff_x = mda::sub(Qx_m, Px_m, modulus);
		// FieldT diff_y = mda::sub(Qy_m, Py_m, modulus);
		//
		// if(diff_x.is_zero()) {
		// 	if(diff_y.is_zero()) return dbl(P);
		// 	return AffinePoint<Bits>();
		// }
		//
		// auto den = mont.inv(diff_x);
		// auto lambda_m = mont.mul(diff_y, den);
		//
		// auto R_m = solve_affine(Px_m, Py_m, Qx_m, lambda_m);

		return AffinePoint<Bits>(
		    std::move(mont.trans_back(Rm.x)),
		    std::move(mont.trans_back(Rm.y)),
		    Rm.is_inf);
	}

	auto dbl_m(const AffinePoint<Bits> &Pm) const -> AffinePoint<Bits>
	{
		if(Pm.is_inf) return Pm;

		const auto two_m = mont.init(2);
		const auto three_m = mont.init(3);

		auto num = mda::add(
		    mont.mul(three_m, mont.mul(Pm.x, Pm.x)),
		    a_m, modulus);

		auto den = mont.inv(mont.mul(two_m, Pm.y));
		auto lambda_m = mont.mul(num, den);

		// x3 = lambda^2 - x1 (P) - x2 (Q) mod q
		auto x3 = mont.mul(lambda_m, lambda_m);
		x3 = mda::sub(x3, Pm.x, modulus);
		x3 = mda::sub(x3, Pm.x, modulus);

		// -y3 = lambda * (x3 - x1 (P)) + y1 (P)
		// y3 = lambda * (x1 (P) - x3) - y1 (P)
		auto y3 = mda::sub(Pm.x, x3, modulus);
		y3 = mont.mul(lambda_m, y3);
		y3 = mda::sub(y3, Pm.y, modulus);

		return AffinePoint<Bits>{
			std::move(x3), std::move(y3)
		};
	}

	auto dbl(const AffinePoint<Bits> &P) const -> AffinePoint<Bits>
	{
		if(P.is_inf) return P;
		//
		// auto Px_m = mont.init(P.x);
		// auto Py_m = mont.init(P.y);
		//
		// const auto two_m = mont.init(2);
		// const auto three_m = mont.init(3);
		//
		// auto num = mda::add(
		//     mont.mul(three_m, mont.mul(Px_m, Px_m)),
		//     a_m, modulus);
		//
		// auto den = mont.inv(mont.mul(two_m, Py_m));
		// auto lambda_m = mont.mul(num, den);
		//
		// auto R_m = solve_affine(Px_m, Py_m, Px_m, lambda_m);

		AffinePoint<Bits> Pm{
			std::move(mont.init(P.x)),
			std::move(mont.init(P.y)),
			P.is_inf
		};

		auto Rm = dbl_m(Pm);

		return AffinePoint<Bits>(
		    std::move(mont.trans_back(Rm.x)),
		    std::move(mont.trans_back(Rm.y)),
		    Rm.is_inf);
	}

	auto add_m(const ProjectivePoint<Bits> &Pm, const ProjectivePoint<Bits> &Qm) const
	    -> ProjectivePoint<Bits>
	{
		if(Pm.Z.is_zero()) return Qm;
		if(Qm.Z.is_zero()) return Pm;

		auto X1Z2 = mont.mul(Pm.X, Qm.Z);
		auto X2Z1 = mont.mul(Qm.X, Pm.Z);

		auto Y1Z2 = mont.mul(Pm.Y, Qm.Z);
		auto Y2Z1 = mont.mul(Qm.Y, Pm.Z);

		if(X1Z2 == X2Z1) {
			if(Y1Z2 == Y2Z1) {
				return dbl_m(Pm);
			} else {
				return ProjectivePoint<Bits>{};
			}
		}

		const auto two_m = mont.init(2);

		auto Z1Z2 = mont.mul(Pm.Z, Qm.Z);

		auto u = mda::sub(Y2Z1, Y1Z2, modulus);
		auto uu = mont.mul(u, u);

		auto v = mda::sub(X2Z1, X1Z2, modulus);
		auto vv = mont.mul(v, v);
		auto vvv = mont.mul(vv, v);

		auto R = mont.mul(vv, X1Z2);
		auto A = mda::sub(mda::sub(mont.mul(uu, Z1Z2), vvv, modulus), mont.mul(two_m, R), modulus);

		auto X3 = mont.mul(v, A);
		auto Y3 = mda::sub(mont.mul(u, mda::sub(R, A, modulus)), mont.mul(vvv, Y1Z2), modulus);
		auto Z3 = mont.mul(vvv, Z1Z2);

		return ProjectivePoint<Bits>{
			std::move(X3),
			std::move(Y3),
			std::move(Z3)
		};
	}

	auto add(const ProjectivePoint<Bits> &P, const ProjectivePoint<Bits> Q) const
	    -> ProjectivePoint<Bits>
	{
		if(P.Z.is_zero()) return Q;
		if(Q.Z.is_zero()) return P;

		auto Pm = P.mont(mont);
		auto Qm = Q.mont(mont);

		auto Rm = add_m(Pm, Qm);

		return Rm.demont(mont);
	}

	auto dbl_m(const ProjectivePoint<Bits> &Pm) const
	    -> ProjectivePoint<Bits>
	{
		if(Pm.Z.is_zero()) return Pm;

		auto XX = mont.mul(Pm.X, Pm.X);
		auto ZZ = mont.mul(Pm.Z, Pm.Z);

		const auto two_m = mont.init(2);
		const auto three_m = mont.init(3);

		auto w = mda::add(mont.mul(a_m, ZZ), mont.mul(three_m, XX), modulus);
		auto s = mont.mul(mont.mul(two_m, Pm.Y), Pm.Z);
		auto ss = mont.mul(s, s);

		auto R = mont.mul(Pm.Y, s);
		auto RR = mont.mul(R, R);

		auto X1R = mda::add(Pm.X, R, modulus);
		auto B = mda::sub(mda::sub(mont.mul(X1R, X1R), XX, modulus), RR, modulus);

		auto h = mda::sub(mont.mul(w, w), mont.mul(two_m, B), modulus);

		auto X3 = mont.mul(h, s);
		auto Y3 = mda::sub(mont.mul(w, mda::sub(B, h, modulus)), mont.mul(two_m, RR), modulus);
		auto Z3 = mont.mul(ss, s);

		return ProjectivePoint<Bits>{
			std::move(X3),
			std::move(Y3),
			std::move(Z3)
		};
	}

	auto dbl(const ProjectivePoint<Bits> &P) const
	    -> ProjectivePoint<Bits>
	{
		if(P.Z.is_zero()) return P;

		auto Pm = P.mont(mont);

		auto Rm = dbl_m(Pm);

		return Rm.demont(mont);
	}

	auto add_m(const JacobianPoint<Bits> &Pm, const JacobianPoint<Bits> &Qm) const -> JacobianPoint<Bits>
	{
		if(Pm.Z.is_zero()) return Qm;
		if(Qm.Z.is_zero()) return Pm;

		auto Z1sq_m = mont.mul(Pm.Z, Pm.Z);
		auto Z2sq_m = mont.mul(Qm.Z, Qm.Z);

		auto U1_m = mont.mul(Pm.X, Z2sq_m);
		auto U2_m = mont.mul(Qm.X, Z1sq_m);

		auto S1_m = mont.mul(Pm.Y, mont.mul(Qm.Z, Z2sq_m));
		auto S2_m = mont.mul(Qm.Y, mont.mul(Pm.Z, Z1sq_m));

		if(U1_m == U2_m) {
			if(S1_m == S2_m) {
				return dbl_m(Pm);
			} else {
				return JacobianPoint<Bits>{};
			}
		}

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
			std::move(X3_m),
			std::move(Y3_m),
			std::move(Z3_m)
		};
	}

	auto add(const JacobianPoint<Bits> &P, const JacobianPoint<Bits> &Q) const -> JacobianPoint<Bits>
	{
		if(P.Z.is_zero()) return Q;
		if(Q.Z.is_zero()) return P;

		auto Pm = P.mont(mont);
		auto Qm = Q.mont(mont);

		auto Rm = add_m(Pm, Qm);

		return Rm.demont(mont);
	}

	auto dbl_m(const JacobianPoint<Bits> &Pm) const -> JacobianPoint<Bits>
	{
		if(Pm.Z.is_zero()) return Pm;

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
			std::move(T_m),
			std::move(Y3_m),
			std::move(Z3_m)
		};
	}

	auto dbl(const JacobianPoint<Bits> &P) const -> JacobianPoint<Bits>
	{
		if(P.Z.is_zero()) return P;

		auto Pm = P.mont(mont);

		auto Rm = dbl_m(Pm);

		return Rm.demont(mont);
	}

	auto to_affine(const AffinePoint<Bits> &p) const -> AffinePoint<Bits>
	{
		return p;
	}

	auto to_affine(const ProjectivePoint<Bits> &p) const
	    -> AffinePoint<Bits>
	{
		if(p.Z.is_zero()) return AffinePoint<Bits>{};

		auto Pm = p.mont(mont);

		auto z_inv = mont.inv(Pm.Z);

		auto x = mont.mul(Pm.X, z_inv);
		auto y = mont.mul(Pm.Y, z_inv);

		return AffinePoint<Bits>{
			std::move(mont.trans_back(x)),
			std::move(mont.trans_back(y))
		};
	}

	auto to_affine(const JacobianPoint<Bits> &p) const -> AffinePoint<Bits>
	{
		if(p.Z.is_zero()) return AffinePoint<Bits>{};

		auto Pm = p.mont(mont);

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

		auto Pm = p.mont(mont);

		return is_on_curve_m(Pm.x, Pm.y);
	}

	auto is_on_curve(const ProjectivePoint<Bits> &p) const -> bool
	{
		if(p.Z.is_zero()) return true;

		auto Pm = p.mont(mont);

		auto z_inv = mont.inv(Pm.Z);

		auto x = mont.mul(Pm.X, z_inv);
		auto y = mont.mul(Pm.Y, z_inv);

		return is_on_curve(x, y);
	}

	auto is_on_curve(const JacobianPoint<Bits> &p) const -> bool
	{
		if(p.Z.is_zero()) return true;

		auto Pm = p.mont(mont);

		auto z2 = mont.mul(Pm.Z, Pm.Z);
		auto z3 = mont.mul(z2, Pm.Z);

		auto x = mont.mul(Pm.X, mont.inv(z2));
		auto y = mont.mul(Pm.Y, mont.inv(z3));

		return is_on_curve_m(x, y);
	}

private:
	auto is_on_curve_m(const FieldT &xm, const FieldT &ym) const
	    -> bool
	{
		auto lhs = mont.mul(ym, ym);

		auto x2 = mont.mul(xm, xm);
		auto x3 = mont.mul(x2, xm);
		auto ax = mont.mul(a_m, xm);
		auto rhs = mda::add(x3, ax, modulus);
		rhs = mda::add(rhs, b_m, modulus);

		return lhs == rhs;
	}
};

static_assert(EllipticCurveConcept<ShortWeierstrassCurve<32>, AffinePoint<32>>,
	      "Concept Error: ShortWeierstrassCurve and AffinePoint does not meet requirements");

static_assert(EllipticCurveConcept<ShortWeierstrassCurve<32>, ProjectivePoint<32>>,
	      "Concept Error: ShortWeierstrassCurve and Projectivepoint does not meet requirements");

static_assert(EllipticCurveConcept<ShortWeierstrassCurve<32>, JacobianPoint<32>>,
	      "Concept Error: ShortWeierstrassCurve and JacobianPoint does not meet requirements");

template <size_t Bits>
struct ExtProjPoint {
	using FieldT = bga::BigInt<Bits>;
	static constexpr size_t bits = Bits;

	FieldT X, Y, Z, T;

	ExtProjPoint() : X(0), Y(1), Z(1), T(0) {};
	ExtProjPoint(FieldT x, FieldT y, FieldT z, FieldT t)
	    : X(std::move(x)), Y(std::move(y)),
	      Z(std::move(z)), T(std::move(t)) {};

	template <mda::MONT_ALGO mont_algo = mda::MONT_ALGO::FIOS>
	ExtProjPoint<Bits> mont(const mda::Montgomery<Bits, mont_algo> &mont) const
	{
		return ExtProjPoint<Bits>{
			std::move(mont.init(this->X)),
			std::move(mont.init(this->Y)),
			std::move(mont.init(this->Z)),
			std::move(mont.init(this->T))
		};
	}

	template <mda::MONT_ALGO mont_algo = mda::MONT_ALGO::FIOS>
	ExtProjPoint<Bits> demont(const mda::Montgomery<Bits, mont_algo> &mont) const
	{
		return ExtProjPoint<Bits>{
			std::move(mont.trans_back(this->X)),
			std::move(mont.trans_back(this->Y)),
			std::move(mont.trans_back(this->Z)),
			std::move(mont.trans_back(this->T))
		};
	}
};

template <size_t Bits, mda::MONT_ALGO mont_algo = mda::MONT_ALGO::FIOS>
class TwistedEdwardsCurve
{
public:
	using FieldT = bga::BigInt<Bits>;
	static constexpr size_t bits = Bits;

	FieldT a, d, modulus;

	const FieldT one{ 1 };

	mda::Montgomery<Bits, mont_algo> mont;
	FieldT a_m, d_m;
	const FieldT one_m;

	TwistedEdwardsCurve(FieldT a, FieldT d, FieldT mod)
	    : a(a), d(d), modulus(mod), mont(mod),
	      a_m(mont.init(a)), d_m(mont.init(d)), one_m(mont.init(1)) {};

	auto add_m(
	    const ExtProjPoint<Bits> &Pm,
	    const ExtProjPoint<Bits> &Qm) const
	    -> ExtProjPoint<Bits>
	{
		using mda::add;
		using mda::sub;

		auto A = mont.mul(Pm.X, Qm.X);
		auto B = mont.mul(Pm.Y, Qm.Y);
		auto C = mont.mul(Pm.T, mont.mul(d_m, Qm.T));
		auto D = mont.mul(Pm.Z, Qm.Z);

		auto E = (sub)((sub)(mont.mul(
					 (add)(Pm.X, Pm.Y, modulus),
					 (add)(Qm.X, Qm.Y, modulus)),
				     A, modulus),
			       B, modulus);

		auto F = sub(D, C, modulus);
		auto G = add(D, C, modulus);
		auto H = sub(B, mont.mul(a_m, A), modulus);

		auto X3 = mont.mul(E, F);
		auto Y3 = mont.mul(G, H);
		auto Z3 = mont.mul(F, G);
		auto T3 = mont.mul(E, H);

		return ExtProjPoint<Bits>{
			std::move(X3),
			std::move(Y3),
			std::move(Z3),
			std::move(T3)
		};
	}

	auto add(
	    const ExtProjPoint<Bits> &P,
	    const ExtProjPoint<Bits> &Q) const
	    -> ExtProjPoint<Bits>
	{
		auto Pm = P.mont(mont);
		auto Qm = Q.mont(mont);

		auto Rm = add_m(Pm, Qm);

		return Rm.demont(mont);
	}

	auto dbl_m(const ExtProjPoint<Bits> &Pm) const
	    -> ExtProjPoint<Bits>
	{
		using mda::add;
		using mda::sub;

		auto A = mont.mul(Pm.X, Pm.X);
		auto B = mont.mul(Pm.Y, Pm.Y);

		const auto two_m = mont.init(2);
		auto C = mont.mul(two_m, mont.mul(Pm.Z, Pm.Z));
		auto D = mont.mul(a_m, A);

		auto X1Y1 = add(Pm.X, Pm.Y, modulus);
		auto E = sub(sub(mont.mul(X1Y1, X1Y1), A, modulus), B, modulus);
		auto G = add(D, B, modulus);
		auto F = sub(G, C, modulus);
		auto H = sub(D, B, modulus);

		auto X3 = mont.mul(E, F);
		auto Y3 = mont.mul(G, H);
		auto Z3 = mont.mul(F, G);
		auto T3 = mont.mul(E, H);

		return ExtProjPoint<Bits>{
			std::move(X3),
			std::move(Y3),
			std::move(Z3),
			std::move(T3)
		};
	}

	auto dbl(const ExtProjPoint<Bits> &P) const
	    -> ExtProjPoint<Bits>
	{
		auto Pm = P.mont(mont);

		auto Rm = dbl_m(Pm);

		return Rm.demont(mont);
	}

	auto to_affine(const ExtProjPoint<Bits> &P) const
	    -> AffinePoint<Bits>
	{
		auto Pm = P.mont(this->mont);

		auto z_inv = mont.inv(Pm.Z);
		auto x = mont.mul(Pm.X, z_inv);
		auto y = mont.mul(Pm.Y, z_inv);

		return AffinePoint<Bits>{
			std::move(mont.trans_back(x)),
			std::move(mont.trans_back(y)),
			false
		};
	}

	auto is_on_curve(const AffinePoint<Bits> &P) const -> bool
	{
		if(P.is_inf) return true;

		auto Pm = P.mont(mont);

		return is_on_curve_m(Pm.x, Pm.y);
	}

	auto is_on_curve(const ExtProjPoint<Bits> &P) const -> bool
	{
		auto Pm = P.mont(mont);

		auto z_inv = mont.inv(Pm.Z);
		auto x = mont.mul(Pm.X, z_inv);
		auto y = mont.mul(Pm.Y, z_inv);

		return is_on_curve_m(x, y);
	}

private:
	auto is_on_curve_m(const FieldT &xm, const FieldT &ym) const
	    -> bool
	{
		auto x2 = mont.mul(xm, xm);
		auto y2 = mont.mul(ym, ym);

		auto ax2 = mont.mul(a_m, x2);
		auto lhs = mda::add(ax2, y2, modulus);

		auto dx2y2 = mont.mul(d_m, mont.mul(x2, y2));
		auto rhs = mda::add(one_m, dx2y2, modulus);

		return lhs == rhs;
	}
};

static_assert(EllipticCurveConcept<TwistedEdwardsCurve<32>, ExtProjPoint<32>>,
	      "Concept Error: TwistedEdwardsCuve and ExtendedProjectivePoint does not meet requirements");

template <typename CurveT, typename PointT>
	requires EllipticCurveConcept<CurveT, PointT>
auto scalarMul(
    const CurveT &curve,
    const PointT &point,
    const bga::BigInt<CurveT::bits> &scalar) -> PointT
{
	PointT res_P;
	auto Pm = point.mont(curve.mont);

	for(auto it = scalar.rbegin(); it != scalar.rend(); it++) {
		const auto chunk = *it;

		for(int j = CurveT::bits - 1; j >= 0; j--) {
			res_P = curve.dbl_m(res_P);

			if((chunk >> j) & 1) {
				res_P = curve.add_m(res_P, Pm);
			}
		}
	}
	return res_P.demont(curve.mont);
}

template <typename CurveT, typename PointT>
	requires EllipticCurveConcept<CurveT, PointT>
auto msm(
    const CurveT &curve,
    const std::vector<PointT> &points,
    const std::vector<bga::BigInt<CurveT::bits>> &scalars)
    -> PointT
{
	using uint_t = typename bga::SelectIntType_t<CurveT::bits>::uint_t;
	using uint_d = typename bga::SelectIntType_t<CurveT::bits>::uintd_t;

	constexpr size_t c = 4;
	constexpr size_t num_buckets = (1 << c) - 1;
	constexpr size_t num_windows = (CurveT::bits + c - 1) / c;

	auto get_window_val = [](const bga::BigInt<CurveT::bits> &scalar, int window_idx)
	    -> int {
		constexpr uint_t mask = (static_cast<uint_d>(1) << c) - 1;

		size_t start_bit = window_idx * c;

		size_t chunk_idx = start_bit / CurveT::bits;
		size_t bit_offset = start_bit % CurveT::bits;

		uint_t val = scalar.get(chunk_idx) >> bit_offset;

		if(bit_offset + c > CurveT::bits) {
			uint_t next_chunk = scalar.get(chunk_idx + 1);
			val |= (next_chunk << (CurveT::bits - bit_offset));
		}

		return val & mask;
	};

	PointT result{};

	for(int w = num_windows - 1; w >= 0; w--) {
		std::vector<PointT> buckets(num_buckets + 1, PointT{});

		for(size_t i = 0; i < points.size(); i++) {
			int bucket_idx = get_window_val(scalars[i], w);
			buckets[bucket_idx] = curve.add_m(buckets[bucket_idx], points[i].mont(curve.mont));
		}

		PointT acc{};
		PointT running{};

		for(int k = num_buckets; k >= 1; k--) {
			running = curve.add_m(running, buckets[k]);
			acc = curve.add_m(acc, running);
		}

		if(w != num_windows - 1) {
			for(int i = 0; i < c; i++) {
				result = curve.dbl_m(result);
			}
		}
		result = curve.add_m(result, acc);
	}

	return result.demont(curve.demont);
}

} // namespace ecc

#include <print>

template <size_t Bits>
struct std::formatter<ecc::AffinePoint<Bits>> {
	constexpr auto parse(std::format_parse_context &ctx)
	{
		return ctx.begin();
	}

	auto format(const ecc::AffinePoint<Bits> &p, auto &ctx) const
	{
		if(p.is_inf) return std::format_to(ctx.out(), "Infinity");
		return std::format_to(ctx.out(), "({}, {})", p.x, p.y);
	}
};
