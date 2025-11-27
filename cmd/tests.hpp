#pragma once

#include <functional>
#include <string>

#include "bigint.hpp"

#define test_assert(expr1, expr2, msg, ...)                                                       \
	do {                                                                                      \
		if((expr1) != (expr2)) {                                                          \
			std::println(stderr, "FAILED TEST: ({}): {} == ({}): {}, " msg,           \
				     #expr1, (expr1), #expr2, (expr2)__VA_OPT__(, ) __VA_ARGS__); \
			std::println("fun: {}", __PRETTY_FUNCTION__);                             \
			exit(1);                                                                  \
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

/*
template <size_t Bits>
bga::BigInt<Bits> run_bc(bga::BigInt<Bits> a, std::string_view op, bga::BigInt<Bits> b)
{
#ifdef __linux__
	std::string cmd = std::format("echo \"{} {} {}\" | BC_LINE_LENGTH=0 bc", a, op, b);

	auto pipe_del = [](FILE *f) {
		if(f) pclose(f);
	};
	std::unique_ptr<FILE, decltype(pipe_del)> pipe{ popen(cmd.c_str(), "r"), pipe_del };
	assert(pipe && "pipe NULL");

	std::array<char, 128> buf;
	std::string result;
	while(fgets(buf.data(), buf.size(), pipe.get()) != nullptr) {
		result += buf.data();
	}

	if(!result.empty() && result.back() == '\n') {
		result.pop_back();
	}

	return bga::BigInt<Bits>(result);
#else
#warning "WARNING: Can't run the bc test outside of linux"
#endif
	return bga::BigInt<Bits>(); // return zero
}
*/

std::function<std::string()> genRandBgN(size_t digits = 10);

void test_cmps();
void test_cmps_with_bc();
void test_operations();
void test_operations_with_bc();
