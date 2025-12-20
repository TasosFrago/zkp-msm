#pragma once

#include <chrono>
#include <functional>
#include <iterator>
#include <map>
#include <string>

#include "bigint.hpp"

#define TEST_ASSERT_OLD(expr1, expr2, msg, ...)                                                   \
	do {                                                                                      \
		if((expr1) != (expr2)) {                                                          \
			std::println(stderr, "FAILED TEST: ({}): {} == ({}): {}, " msg,           \
				     #expr1, (expr1), #expr2, (expr2)__VA_OPT__(, ) __VA_ARGS__); \
			std::println("fun: {}", __PRETTY_FUNCTION__);                             \
			std::abort();                                                             \
		}                                                                                 \
	} while(0)

#define progress(i, batches)                                             \
	do {                                                             \
		if((i + 1) % 10 == 0 || i == batches - 1) {              \
			std::print("Processing: {:>4}/{} ({:>3.0f}%)\r", \
				   i + 1,                                \
				   BATCHES,                              \
				   100.0 * (i + 1) / batches);           \
			std::fflush(stdout);                             \
		}                                                        \
	} while(0)

#define __BITS_TO_TEST_NORMAL 2, 4, 8, 16, 32, 64
#define __BITS_TO_TEST_BC 32, 40, 64

#define BITS_TEST_MATH__N __BITS_TO_TEST_NORMAL
#define BITS_TEST_MATH_BC __BITS_TO_TEST_BC
#define BITS_TEST_MOD___N __BITS_TO_TEST_NORMAL
#define BITS_TEST_MOD__BC __BITS_TO_TEST_BC
#define BITS_TEST_CMP___N __BITS_TO_TEST_NORMAL
#define BITS_TEST_CMP__BC __BITS_TO_TEST_BC

#define __STR_H(...) #__VA_ARGS__
#define _STR_H(...) __STR_H(__VA_ARGS__)
#define STR(X) _STR_H(X)

struct TestResults {
	struct metrics {
		int passed = 0;
		int failed = 0;
		std::string msg;
	};

	std::map<std::string, metrics> error_table;

	void success(const std::string &key)
	{
		error_table[key].passed++;
	}

	std::string &fail(const std::string &key)
	{
		error_table[key].failed++;
		return error_table[key].msg;
	}

	void print_results()
	{
		int total_passed = 0;
		int total_failed = 0;

		std::string buf;

		std::println("\n==== RESULTS =====");
		for(const auto &[key, value] : error_table) {
			total_passed += value.passed;
			total_failed += value.failed;

			std::format_to(std::back_inserter(buf),
				       "[{}] {:<28} [ \033[32mPassed: {:5}\033[0m ] [ \033[31mFailed: {:5}\033[0m ]\n",
				       ((value.failed > 0) ? "\033[31mTEST\033[0m" : "\033[32mTEST\033[0m"),
				       key, value.passed, value.failed);

			if(value.failed > 0) {
				std::println("{:-^7} Errors: {} {:-^7}", "", key, "");
				std::println("{}", value.msg);
				std::println("{:-^50}", "");
			}
		}
		std::print("{}", buf);
		std::println("");
		std::println("{:=^10} TOTAL SUMMARY {:=^10}", "", "");
		std::println("Passed: {}", total_passed);
		std::println("Failed: {}", total_failed);

		if(total_failed == 0) {
			std::println("RESULT: SUCCESS");
		} else {
			std::println("RESULT: FAILED");
		}
		std::println("{:=^35}\n", "");
	}
};

inline TestResults results;

constexpr std::string_view extract_Bits(std::string_view sig)
{
	if(sig.empty()) return "";
	size_t pos = sig.find_last_of("N =");
	pos -= 3;
	size_t end = sig.rfind("]");

	if(pos != std::string_view::npos) {
		return sig.substr(pos, end - pos);
	}
	return "";
}

#define test_assert(key, expr1, expr2, msg, ...)                                                                  \
	do {                                                                                                      \
		if((expr1) != (expr2)) {                                                                          \
			auto &buff = results.fail(key);                                                           \
			std::format_to(std::back_inserter(buff),                                                  \
				       "\t[\033[31mFAILED\033[0m]:[{}] ({}): {} == ({}): {},\n\t\t\t" msg "\n\n", \
				       extract_Bits(__PRETTY_FUNCTION__),                                         \
				       #expr1, (expr1), #expr2, (expr2)__VA_OPT__(, ) __VA_ARGS__);               \
                                                                                                                  \
		} else {                                                                                          \
			(results).success(key);                                                                   \
		}                                                                                                 \
	} while(0)

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
		static BcExec instance;
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

std::function<std::string()> genRandBgN(size_t digits = 10);

void test_cmps();
void test_cmps_with_bc();

void test_operations();
void test_operations_with_bc();

void test_mod();
void test_mod_with_bc();
