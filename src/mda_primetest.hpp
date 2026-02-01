#pragma once
#include <bit>
#include <mutex>
#include <random>
#include <shared_mutex>
#include <unordered_map>
#include <utility>

#include "big_arth.hpp"
#include "bigint.hpp"

namespace mda
{

enum class MONT_ALGO;
template <size_t Bits, MONT_ALGO algo>
struct Montgomery;

template <size_t Bits>
bga::BigInt<Bits> mod(bga::BigInt<Bits> a, bga::BigInt<Bits> q);

template <size_t Bits, MONT_ALGO algo>
class PrimalityTestCached
{
private:
	struct BigIntHasher {
		size_t operator()(const bga::BigInt<Bits> &n) const
		{
			size_t hash = 0;

			for(auto chunk : n.get()) {
				hash ^= std::hash<uint64_t>{}(chunk) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
			}
			return hash;
		}
	};

	std::unordered_map<bga::BigInt<Bits>, bool, BigIntHasher> cache;
	mutable std::shared_mutex rw_lock;

	static std::mt19937_64 &get_rng()
	{
		static thread_local std::random_device rd;
		static thread_local std::mt19937_64 engine(rd());
		return engine;
	}

	bga::BigInt<Bits> generate_rand_a(const bga::BigInt<Bits> &n) const
	{
		auto &rng = get_rng();
		auto n_minus_4 = bga::sub(n, bga::BigInt<Bits>(4));
		std::uniform_int_distribution<uint64_t> dist;
		size_t chunks = n.get_chunks();

		while(true) {
			bga::BigInt<Bits> a;
			a.reserve(chunks);

			for(size_t i = 0; i < chunks; i++) {
				// bga::BigInt<Bits> chunk_val(dist(rng));
				uint64_t r_val = rng();

				if(i == chunks - 1) {
					uint64_t top = n.get(i);
					if(top > 0) {
						int leading_zeros = std::countl_zero(top);
						uint64_t mask = (0xFFFFFFFFFFFFFFFFULL >> leading_zeros);
						r_val &= mask;
					}
				}
				a.push_bits(static_cast<typename bga::SelectIntType_t<Bits>::uint_t>(r_val));
				// a = bga::add(a, bga::lshift(bga::BigInt<Bits>(r_val), i * Bits));
			}
			a.trim();
			if(a > 0 && a <= n_minus_4) return bga::add(a, bga::BigInt<Bits>(2));
		}
		std::unreachable();
	}

	bool fail_trial_div(const bga::BigInt<Bits> &n)
	{
		static constexpr std::array<uint32_t, 25> primes = {
			2, 3, 5, 7, 11, 13, 17, 19, 23, 29, 31, 37, 41,
			43, 47, 53, 59, 61, 67, 71, 73, 79, 83, 89, 97
		};

		for(auto p : primes) {
			bga::BigInt<Bits> bp(p);
			if(n == bp) return false;
			if(mda::mod(n, bp).is_zero()) return true;
		}
		return false;
	}

	void store_res(const bga::BigInt<Bits> &n, bool result)
	{
		std::unique_lock lock(rw_lock);
		cache[n] = result;
	}

public:
	PrimalityTestCached() = default;
	~PrimalityTestCached() = default;

	PrimalityTestCached(const PrimalityTestCached &) = delete;
	PrimalityTestCached &operator=(const PrimalityTestCached &) = delete;
	PrimalityTestCached(PrimalityTestCached &&) = delete;
	PrimalityTestCached &operator=(PrimalityTestCached &&) = delete;

	bool is_prime(const bga::BigInt<Bits> &n, size_t t = 40)
	{
		{
			std::shared_lock lock(rw_lock);
			auto it = cache.find(n);
			if(it != cache.end()) return it->second;
		}

		if(n < bga::BigInt<Bits>(2)) return false;
		if(n == bga::BigInt<Bits>(2) || n == bga::BigInt<Bits>(3)) return true;
		if((n.get(0) & 1) == 0 || fail_trial_div(n)) {
			store_res(n, false);
			return false;
		}
		bool result = run_miller_rabin(n, t);

		store_res(n, result);
		return result;
	}

	bool is_prime_raw(const bga::BigInt<Bits> &n, size_t t = 40)
	{
		if(n < bga::BigInt<Bits>(2)) return false;
		if(n == bga::BigInt<Bits>(2) || n == bga::BigInt<Bits>(3)) return true;
		if((n.get(0) & 1) == 0) return false;

		if(fail_trial_div(n)) return false;

		return run_miller_rabin(n, t);
	}

	bga::BigInt<Bits> generate_prime(const bga::BigInt<Bits> &range)
	{
		while(true) {
			bga::BigInt<Bits> candidate = generate_rand_a(range);
			candidate.get(0) |= 1;

			assertm((candidate.get(0) & 1) == 1, "Candidate must be odd");

			if(is_prime_raw(candidate)) {
				return candidate;
			}
		}
	}

private:
	bool run_miller_rabin(const bga::BigInt<Bits> &n, size_t t) const
	{

		auto n_minus_1 = bga::sub(n, bga::BigInt<Bits>(1));
		auto r = n_minus_1;
		size_t s = 0;

		while(!r.is_zero() && (r.get(0) & 1) == 0) {
			r = bga::rshift(r, 1);
			s++;
		}

		mda::Montgomery<Bits, algo> mont(n);
		auto one_m = mont.init(bga::BigInt<Bits>(1));
		auto n_minus_1_m = mont.init(n_minus_1);

		for(size_t i = 0; i < t; i++) {
			auto a = generate_rand_a(n);

			auto a_m = mont.init(a);
			auto y = mont.pow(a_m, r);

			if(y != one_m && y != n_minus_1_m) {
				size_t j = 1;

				while(j < s && y != n_minus_1_m) {
					y = mont.mul(y, y);

					if(y == one_m) return false;
					j++;
				}

				if(y != n_minus_1_m) return false;
			}
		}
		return true;
	}
};

template <size_t Bits, MONT_ALGO algo>
inline PrimalityTestCached<Bits, algo> &get_primalityTester()
{
	static PrimalityTestCached<Bits, algo> instance;
	return instance;
}

} // namespace mda
