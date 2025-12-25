#pragma once

#include "test_framework.hpp"

#define __BITS_TO_TEST_NORMAL 2, 4, 8, 16, 32, 64
#define __BITS_TO_TEST_BC 32, 40, 64

#define BITS_TEST_MATH__N __BITS_TO_TEST_NORMAL
#define BITS_TEST_MATH_BC __BITS_TO_TEST_BC
#define BITS_TEST_MOD___N __BITS_TO_TEST_NORMAL
#define BITS_TEST_MOD__BC __BITS_TO_TEST_BC
#define BITS_TEST_CMP___N __BITS_TO_TEST_NORMAL
#define BITS_TEST_CMP__BC __BITS_TO_TEST_BC

#define MATH_BATCHES 1000
#define MATH_BC_BATCHES 500
#define CMP_BATCHES 1000
#define CMP_BC_BATCHES 1000
#define MOD_BATCHES 1000
#define MOD_BC_BATCHES 500

void register_math_tests();
void register_cmp_tests();
void register_mod_tests();
