#pragma once

#include <execution>
#include <functional>
#include <iostream>
#include <iterator>
#include <map>
#include <mutex>
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
	bool parallel_exec = true;
	using test_fn = std::function<void(void)>;
	std::vector<test_fn> test_suites;

	std::mutex mtx;
	std::mutex console_mtx;
	int total_suites = 0;

	std::string make_progress_bar(std::string_view name, int p, int width = 50)
	{
		int bar_width = width - 20;
		int filled = (p * bar_width) / 100;

		std::string bar = "[";
		bar.append(filled, '#');
		bar.append(bar_width - filled, '-');
		bar += "]";

		return std::format("{:<30} {} {:3}%", name, bar, p);
	}

	void update_line(int row_index, const std::string &text)
	{
		std::lock_guard<std::mutex> lock(console_mtx);

		// std::print("\0337");

		int lines_up = total_suites - row_index;
		std::print("\033[{}A", lines_up);

		std::print("\r\033[K{}", text);
		std::print("\033[{}B", lines_up);
		// std::print("\0338");
		std::cout << std::flush;
	}

	struct metrics {
		int passed = 0;
		int failed = 0;
		std::string msg;
	};

	std::map<std::string, metrics> error_table;

	void success(const std::string &key)
	{
		std::lock_guard<std::mutex> lock(mtx);
		error_table[key].passed++;
	}

	std::string &fail(const std::string &key)
	{
		std::lock_guard<std::mutex> lock(mtx);
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
		total_suites += 2;
		int my_id = total_suites - 1;
		std::string bits_list;
		((bits_list += (bits_list.empty() ? "" : ", ") + std::to_string(Ns)), ...);

		auto test_wrapper = [=, this, logic = std::forward<Func>(test_logic)]() {
			std::string start_msg = std::format("Running {} ({} batches) with chunks of ({}) bits...", name, batches, bits_list);
			update_line(my_id, start_msg);

			std::random_device rd;
			std::mt19937_64 gene(rd());

			auto start_time = std::chrono::high_resolution_clock::now();

			for(size_t i = 0; i < batches; i++) {
				(logic.template operator()<Ns>(gene), ...);

				if(batches > 100 && i % (batches / 100) == 0) {
					int percent = (i * 100) / batches;
					update_line(my_id + 1, make_progress_bar(name, percent, 100));
				}
			}
			auto end_time = std::chrono::high_resolution_clock::now();
			auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);
			auto seconds = std::chrono::duration_cast<std::chrono::duration<double>>(duration);

			std::string msg = std::format("COMPLETED {:<50} [ \033[94m{:.3f}s\033[0m ]",
						      name, seconds.count());
			update_line(my_id + 1, msg);
		};
		test_suites.push_back(test_wrapper);
	}

	void run_all()
	{
		std::println("Starting execution across {} threads...", std::thread::hardware_concurrency());
		total_suites++;

		for(int i = 0; i < total_suites; i++) {
			std::println("");
		}

		if(parallel_exec) {
			std::print("\033[?25l");
			std::cout << std::flush;

			std::for_each(
			    std::execution::par,
			    test_suites.begin(),
			    test_suites.end(),
			    [](auto &suite) { suite(); });

			std::print("\033[?25h");
			std::cout << std::flush;
		} else {
			for(auto &test : test_suites) {
				test();
			}
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
