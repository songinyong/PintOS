#ifndef THREADS_FIXED_MATH_H
#define THREADS_FIXED_MATH_H

#include <stdint.h>


#define FIXED_CALC (1 << 14)

int convert_to_fixed_point(int n);
int convert_to_integer_round_toward_zero(int x);
int convert_to_integer_round_to_nearest(int x);
int fixed_point_add(int x, int y);
int fixed_point_subtract(int x, int y);
int fixed_point_add_integer(int x, int n);
int fixed_point_subtract_integer(int x, int n);
int fixed_point_multiply(int x, int y);
int fixed_point_multiply_integer(int x, int n);
int fixed_point_divide(int x, int y);
int fixed_point_divide_integer(int x, int n);

#endif
