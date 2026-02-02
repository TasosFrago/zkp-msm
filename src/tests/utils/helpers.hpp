#pragma once

#include <chrono>
#include <functional>
#include <random>
#include <string>

#include "bigint.hpp"

class BcExec
{
private:
	int pipe_in_[2];
	int pipe_out_[2];
	pid_t pid_ = -1;

	BcExec();

public:
	static BcExec &instance()
	{
		thread_local BcExec instance;
		return instance;
	}

	BcExec(const BcExec &) = delete;
	BcExec &operator=(const BcExec &) = delete;

	~BcExec();
	std::string calculate(std::string_view a, std::string_view op, std::string_view b);
};

template <size_t Bits>
bga::BigInt<Bits> run_bc(bga::BigInt<Bits> a, std::string_view op, bga::BigInt<Bits> b)
{
#ifdef __linux__
	std::string A = std::format("{}", a);
	std::string B = std::format("{}", b);
	std::string res_str = BcExec::instance().calculate(A, op, B);
	return bga::BigInt<Bits>(res_str);
#else
	return bga::BigInt<Bits>();
#endif
}

template <size_t Bits>
bga::BigInt<Bits> shift_bc(bga::BigInt<Bits> a, std::string_view op, bga::BigInt<Bits> b)
{
#ifdef __linux__
	std::string A = std::format("{}", a);
	std::string B = std::format("(2^{})", b);
	std::string res_str = BcExec::instance().calculate(A, op, B);
	return bga::BigInt<Bits>(res_str);
#else
	return bga::BigInt<Bits>();
#endif
}

template <size_t Bits>
bga::BigInt<Bits> run_mod_bc(bga::BigInt<Bits> a, std::string_view op, bga::BigInt<Bits> b, bga::BigInt<Bits> m)
{
#ifdef __linux__
	std::string A = std::format("{}", a);
	std::string B = std::format("{}", b);
	std::string M = std::format("{}", m);

	std::string expr = std::format("(({0} {1} {2}) % {3} + {3})", A, op, B, M);
	std::string res = BcExec::instance().calculate(expr, "%", M);

	return bga::BigInt<Bits>(res);
#else
	return bga::BigInt<Bits>();
#endif
}

template <typename URBG>
std::function<std::string()> genRandBgN(size_t digits, URBG &rng)
{
	if(digits == 0) return []() { return "0"; };

	std::uniform_int_distribution<> digitDist(0, 9);

	return [&rng, digitDist, digits]() mutable -> std::string {
		std::string bgn;
		bgn.reserve(digits);

		int first;
		do {
			first = digitDist(rng);

		} while(first == 0);
		bgn.push_back('0' + first);

		for(size_t i = 0; i < digits - 1; i++) {
			bgn.push_back('0' + digitDist(rng));
		}
		return bgn;
	};
}

inline std::function<std::string()> genRandBgN(size_t digits = 10)
{
	if(digits == 0) return []() { return "0"; };

	std::random_device rd;
	std::mt19937 gen(rd());
	std::uniform_int_distribution<> digitDist(0, 9);

	auto generator = [gen, digitDist, digits]() mutable -> std::string {
		std::string bgn;
		bgn.reserve(digits);

		int first;
		do {
			first = digitDist(gen);
		} while(first == 0);
		bgn.push_back('0' + first);

		for(size_t i = 0; i < digits - 1; i++) {
			int nextDigit = digitDist(gen);
			bgn.push_back('0' + nextDigit);
		}

		return bgn;
	};
	return generator;
}

template <typename Func>
auto measure_time(Func &&func)
{
	auto start = std::chrono::high_resolution_clock::now();
	func();
	auto end = std::chrono::high_resolution_clock::now();

	return std::chrono::duration<double, std::milli>(end - start).count();
}

template <typename Func, typename... Args>
auto time_it(Func &&func, Args &&...args)
{
	using ReturnType = decltype(std::invoke(std::forward<Func>(func),
						std::forward<Args>(args)...));

	auto start = std::chrono::high_resolution_clock::now();

	if constexpr(std::is_void_v<ReturnType>) {
		std::invoke(std::forward<Func>(func), std::forward<Args>(args)...);
		auto end = std::chrono::high_resolution_clock::now();
		auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
		auto seconds = std::chrono::duration_cast<std::chrono::duration<double>>(duration);
		return std::make_pair(std::monostate{}, seconds);
	} else {
		ReturnType result = std::invoke(std::forward<Func>(func),
						std::forward<Args>(args)...);
		auto end = std::chrono::high_resolution_clock::now();
		auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
		auto seconds = std::chrono::duration_cast<std::chrono::duration<double>>(duration);
		return std::make_pair(std::move(result), seconds);
	}
	// std::chrono::duration<double> total_duration;
	// for(int i = 0; i < 20; i++) {
	// 	auto [_, duration] = time_it([a]() {
	// 		std::println("control: {}", bga::add(a, bga::BigInt<64>("1")));
	// 	});
	// 	total_duration += duration;
	// }
	// std::println("Control duration: {}s", total_duration.count());

	// bga::BigInt<64> test = bga::lshift(a, 3510950764);
	// std::println("Hello");
	//
	// {
	// 	auto [_, duration2] = time_it([test]() {
	// 		std::println("shift: {}", test);
	// 	});
	// 	std::println("shift duration: {}s", duration2.count());
	// }
}
