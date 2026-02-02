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

inline thread_local std::map<std::string, int> tls_success_buffer;
struct TestSuite {
	bool parallel_exec = true;
	using test_fn = std::function<void(void)>;
	std::vector<test_fn> test_suites;

	double total_time_taken_s = 0;
	std::mutex time_mtx;

	std::mutex mtx;
	std::map<std::string, std::mutex> mtx_map;
	std::mutex console_mtx;
	int total_suites = 0;

	std::string error_log_file = "";

	std::string make_progress_bar(std::string_view name, int p, int width = 50)
	{
		int bar_width = width - 20;
		int filled = (p * bar_width) / 100;

		std::string bar = "[";
		bar.append(filled, '#');
		bar.append(bar_width - filled, '-');
		bar += "]";

		return std::format("{:<40} {} {:3}%", name, bar, p);
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

	int *get_local_success_ptr(const std::string &key)
	{
		return &tls_success_buffer[key];
	}

	void merge_thread_results()
	{
		std::lock_guard<std::mutex> lock(mtx);
		for(const auto &[key, count] : tls_success_buffer) {
			error_table[key].passed += count;
		}
		tls_success_buffer.clear();
	}

	void success(const std::string &key)
	{
		std::lock_guard<std::mutex> lock(mtx);
		error_table[key].passed++;
		// tls_success_buffer[key]++;
	}

	std::string &fail(const std::string &key)
	{
		std::lock_guard<std::mutex> lock(mtx);
		error_table[key].failed++;
		return error_table[key].msg;
	}

	void add_time(double time)
	{
		std::lock_guard<std::mutex> lock(time_mtx);
		total_time_taken_s += time;
	}

	void print_results()
	{
		int total_passed = 0;
		int total_failed = 0;

		std::string buf, error_buf;

		size_t max_length = 0;
		for(const auto &[key, _] : error_table) {
			max_length = std::max(max_length, key.length());
		}
		max_length += 2;

		std::println("\n==== RESULTS =====");
		for(const auto &[key, value] : error_table) {
			total_passed += value.passed;
			total_failed += value.failed;

			std::format_to(std::back_inserter(buf),
				       "[{}] {:<{}} [ \033[32mPassed: {:5}\033[0m ] [ \033[31mFailed: {:5}\033[0m ]\n",
				       ((value.failed > 0) ? "\033[31mTEST\033[0m" : "\033[32mTEST\033[0m"),
				       key, max_length,
				       value.passed, value.failed);

			if(value.failed > 0) {
				std::format_to(std::back_inserter(error_buf), "{{{{{:-^7} Errors: {} {:-^7}\n", "", key, "");
				std::format_to(std::back_inserter(error_buf), "{}\n", value.msg);
				std::format_to(std::back_inserter(error_buf), "}}}}{:-^50}\n", "");
			}
		}

		if(total_failed > 0) {
			std::unique_ptr<FILE, decltype(&std::fclose)> file_guard(nullptr, &std::fclose);
			FILE *output_target = stderr;

			if(!error_log_file.empty()) {
				FILE *f = std::fopen(error_log_file.c_str(), "w");
				if(f) {
					file_guard.reset(f);
					output_target = f;
				} else {
					std::println(stderr, "Failed to open log file {}, defaulting to stderr", error_log_file);
				}
			}
			std::println(output_target, "{}", error_buf);
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
		std::println("Total time: {:.3f}s", total_time_taken_s);

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

			this->merge_thread_results();

			std::string msg = std::format("COMPLETED {:<50} [ \033[94m{:.3f}s\033[0m ]",
						      name, seconds.count());
			update_line(my_id + 1, msg);

			add_time(seconds.count());
		};
		test_suites.push_back(test_wrapper);
	}

	void run_all()
	{
		if(parallel_exec) {
			std::println("Starting execution across {} threads...", std::thread::hardware_concurrency());
		} else {
			std::println("Starting execution serially...");
		}
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

/*
#define test_assert(key, N, expr1, expr2, msg, ...)                                                                   \
	do {                                                                                                          \
		if((expr1) != (expr2)) {                                                                              \
			auto &buff = (TESTS).fail(key);                                                               \
			std::format_to(std::back_inserter(buff),                                                      \
				       "{: ^4}[\033[31mFAILED\033[0m]:[{}] ({}): {} == ({}): {},\n{: ^8}" msg "\n\n", \
				       "", (N),                                                                       \
				       #expr1, (expr1), #expr2, (expr2), "" __VA_OPT__(, ) __VA_ARGS__);              \
														      \
		} else {                                                                                              \
			(TESTS).success(key);                                                                         \
		}                                                                                                     \
	} while(0)
*/

// Note: We use static thread_local to cache the pointer.
// The expensive map lookup 'get_local_success_ptr' happens only
// the FIRST time this specific line of code is hit by a thread.
// Afterward, it just increments the integer directly.
#define test_assert(key, N, expr1, expr2, msg, ...)                                                                   \
	do {                                                                                                          \
		static thread_local int *_p_success_counter = nullptr;                                                \
		if(__builtin_expect(!_p_success_counter, 0)) {                                                        \
			_p_success_counter = (TESTS).get_local_success_ptr(key);                                      \
		}                                                                                                     \
                                                                                                                      \
		if((expr1) != (expr2)) {                                                                              \
			/* SLOW PATH: Failure */                                                                      \
			auto &buff = (TESTS).fail(key);                                                               \
			std::format_to(std::back_inserter(buff),                                                      \
				       "{: ^4}[\033[31mFAILED\033[0m]:[{}] ({}): {} == ({}): {},\n{: ^8}" msg "\n\n", \
				       "", N,                                                                         \
				       #expr1, (expr1), #expr2, (expr2), "" __VA_OPT__(, ) __VA_ARGS__);              \
		} else {                                                                                              \
			++(*_p_success_counter);                                                                      \
		}                                                                                                     \
	} while(0)

// FIX: Deprecated
void test_cmps();
void test_cmps_with_bc();

void test_operations();
void test_operations_n();
void test_operations_with_bc();

void test_mod();
void test_mod_with_bc();
