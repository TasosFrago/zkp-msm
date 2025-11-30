#include <chrono>
#include <cstdint>
#include <expected>
#include <print>
#include <utility>
#include <variant>

#include "big_arth.hpp"
#include "bigint.hpp"
#include "mod_arth.hpp"

#include "tests.hpp"

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
}

int main(int argc, char *argv[])
{
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

	// bga::BigInt<4> a("-1523415"), p("4");
	// auto test = mda::mod(a, p);
	// std::println("{}", test);

	// test_cmps();
	// test_cmps_with_bc();
	//
	// test_operations();
	// test_operations_with_bc();

	test_mod();
	test_mod_with_bc();

	std::println("");

	// bga::SelectIntType_dbg::debug_loop<8>();

	return 0;
}
