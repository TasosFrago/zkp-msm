#pragma once

#include <functional>
#include <iterator>
#include <map>
#include <random>
#include <string>

#include "tests/utils/helpers.hpp"

#define progress(i, batches)                                             \
	do {                                                             \
		if((i + 1) % 10 == 0 || i == batches - 1) {              \
			std::print("Processing: {:>4}/{} ({:>3.0f}%)\r", \
				   i + 1,                                \
				   batches,                              \
				   100.0 * (i + 1) / batches);           \
			std::fflush(stdout);                             \
		}                                                        \
	} while(0)

#define __STR_H(...) #__VA_ARGS__
#define _STR_H(...) __STR_H(__VA_ARGS__)
#define STR(X) _STR_H(X)

struct TestSuite {
	std::random_device rd;
	using test_fn = std::function<void(void)>;

	std::vector<test_fn> test_suites;

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

	template <size_t... Ns, typename Func>
	void register_test(std::string name, size_t batches, Func &&test_logic)
	{
		std::string bits_list;
		((bits_list += (bits_list.empty() ? "" : ", ") + std::to_string(Ns)), ...);

		auto test_wrapper = [=, logic = std::forward<Func>(test_logic)]() {
			std::println("Running {} ({} batches) with chunks of ({}) bits...", name, batches, bits_list);

			std::random_device rd;
			std::mt19937_64 gen(rd());

			for(size_t i = 0; i < batches; i++) {
				(logic.template operator()<Ns>(gen), ...);
				progress(i, batches);
			}
			std::println("COMPLETED {}.{}", name, std::string(12, ' '));
		};
		test_suites.push_back(test_wrapper);
	}

	void run_all()
	{
		for(auto &test : test_suites) {
			test();
		}
		print_results();
	}
};

inline TestSuite TESTS;

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
			auto &buff = (TESTS).fail(key);                                                           \
			std::format_to(std::back_inserter(buff),                                                  \
				       "\t[\033[31mFAILED\033[0m]:[{}] ({}): {} == ({}): {},\n\t\t\t" msg "\n\n", \
				       extract_Bits(__PRETTY_FUNCTION__),                                         \
				       #expr1, (expr1), #expr2, (expr2)__VA_OPT__(, ) __VA_ARGS__);               \
                                                                                                                  \
		} else {                                                                                          \
			(TESTS).success(key);                                                                     \
		}                                                                                                 \
	} while(0)

// FIX: Deprecated
void test_cmps();
void test_cmps_with_bc();

void test_operations();
void test_operations_n();
void test_operations_with_bc();

void test_mod();
void test_mod_with_bc();
