#pragma once

#define test_assert(expr1, expr2, msg, ...)                                                       \
	{                                                                                         \
		if((expr1) != (expr2)) {                                                          \
			std::println(stderr, "FAILED TEST: ({}): {} == ({}): {}, " msg,           \
				     #expr1, (expr1), #expr2, (expr2)__VA_OPT__(, ) __VA_ARGS__); \
			exit(1);                                                                  \
		}                                                                                 \
	}

void test_cmps();
void test_operations();
