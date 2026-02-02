#include <fcntl.h>
#include <functional>
#include <random>
#include <stdexcept>
#include <string>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "tests/utils/helpers.hpp"

BcExec::BcExec()
{
	if(pipe2(pipe_in_, O_CLOEXEC) < 0 || pipe2(pipe_out_, O_CLOEXEC) < 0) {
		throw std::runtime_error("Failed to create pipes");
	}

	pid_ = fork();
	if(pid_ < 0) {
		throw std::runtime_error("Failed to fork");
	}

	if(pid_ == 0) {
		dup2(pipe_in_[0], STDIN_FILENO);
		dup2(pipe_out_[1], STDOUT_FILENO);

		// close(pipe_in_[1]);
		// close(pipe_out_[0]);
		// close(pipe_in_[0]);
		// close(pipe_out_[1]);

		setenv("BC_LIE_LENGTH", "0", 1);

		execlp("bc", "bc", "-q", nullptr);

		// perror("execlp failed");
		_exit(1);
	} else {
		close(pipe_in_[0]);
		close(pipe_out_[1]);
	}
}

BcExec::~BcExec()
{
	if(pid_ > 0) {
		write(pipe_in_[1], "quit\n", 5);

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

		if(bytes_read <= 0) throw std::runtime_error("bc process closed pipe unexpectedly");

		// if(bytes_read == '\n') break;
		if(buf == '\n') break;
		if(buf == '\\') {
			ssize_t next_read = read(pipe_out_[0], &buf, 1);
			if(next_read <= 0) break;
			if(buf == '\n') continue;
			res += buf;
			continue;
			// char next;
			// read(pipe_out_[0], &next, 1);
			// continue;
		}
		res += buf;
	}
	return res;
}
