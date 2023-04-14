#include "threads/fixed_math.h"
#define FIXED_CALC (1 << 14)

// Converts an integer n to fixed-point format
int convert_to_fixed_point(int n) {
  return n * FIXED_CALC;
}

// Converts a fixed-point number x to integer format (rounding toward zero)
int convert_to_integer_round_toward_zero(int x) {
  return x / FIXED_CALC;
}

// Converts a fixed-point number x to integer format (rounding to nearest)
int convert_to_integer_round_to_nearest(int x) {
  if (x >= 0)
    return (x + FIXED_CALC/ 2) / FIXED_CALC;
  else
    return (x - FIXED_CALC / 2) / FIXED_CALC;
}

// Adds two fixed-point numbers x and y
int fixed_point_add(int x, int y) {
  return x + y;
}

// Subtracts fixed-point number y from x
int fixed_point_subtract(int x, int y) {
  return x - y;
}

// Adds integer n to fixed-point number x
int fixed_point_add_integer(int x, int n) {
  return x + n * FIXED_CALC;
}

// Subtracts integer n from fixed-point number x
int fixed_point_subtract_integer(int x, int n) {
  return x - n * FIXED_CALC;
}

// Multiplies two fixed-point numbers x and y
int fixed_point_multiply(int x, int y) {
  return ((int64_t)x) * y / FIXED_CALC;
}

// Multiplies fixed-point number x by integer n
int fixed_point_multiply_integer(int x, int n) {
  return x * n;
}

// Divides fixed-point number x by y
int fixed_point_divide(int x, int y) {
  return ((int64_t)x) * FIXED_CALC / y;
}

// Divides fixed-point number x by integer n
int fixed_point_divide_integer(int x, int n) {
  return x / n;
}

