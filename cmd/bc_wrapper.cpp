#include <functional>
#include <random>
#include <stdexcept>
#include <string>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "tests.hpp"

std::function<std::string()> genRandBgN(size_t digits)
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

		for(size_t i = 0; i < digits; i++) {
			int nextDigit = digitDist(gen);
			bgn.push_back('0' + nextDigit);
		}

		return bgn;
	};
	return generator;
}

BcExec::BcExec()
{
	if(pipe(pipe_in_) < 0 || pipe(pipe_out_) < 0) {
		throw std::runtime_error("Failed to create pipes");
	}

	pid_ = fork();
	if(pid_ < 0) {
		throw std::runtime_error("Failed to fork");
	}

	if(pid_ == 0) {
		dup2(pipe_in_[0], STDIN_FILENO);
		dup2(pipe_out_[1], STDOUT_FILENO);

		close(pipe_in_[1]);
		close(pipe_out_[0]);
		close(pipe_in_[0]);
		close(pipe_out_[1]);

		setenv("BC_LINE_LENGTH", "0", 1);

		execlp("bc", "bc", "-q", nullptr);

		perror("execlp failed");
		exit(1);
	} else {
		close(pipe_in_[0]);
		close(pipe_out_[1]);
	}
}

BcExec::~BcExec()
{
	if(pid_ > 0) {
		close(pipe_in_[1]);
		close(pipe_out_[0]);
		waitpid(pid_, nullptr, 0);
	}
}

std::string BcExec::calculate(std::string_view a, std::string_view op, std::string_view b)
{
	std::string cmd = std::format("{} {} {}\n", a, op, b);

	if(write(pipe_in_[1], cmd.data(), cmd.size()) == -1) {
		throw std::runtime_error("Failed to write to bc");
	}

	std::string res;
	char buf;
	while(true) {
		ssize_t bytes_read = read(pipe_out_[0], &buf, 1);
		if(bytes_read == '\n') break;
		if(buf == '\n') break;
		if(buf == '\\') {
			char next;
			read(pipe_out_[0], &next, 1);
			continue;
		}
		res += buf;
	}
	return res;
}
