#include "Math.h"

#include <math.h>

f64 Math__map(f64 n, f64 input_start, f64 input_end, f64 output_start, f64 output_end) {
  f64 range = 1.0 * (output_end - output_start) / (input_end - input_start);
  return output_start + range * (n - input_start);
}

f64 Math__mod(f64 a, f64 b) {
  while (isnormal(a) && a > b) {
    a = MATH_MAX(0, a - b);
  }
  return a;
}

f64 Math__sin(f64 x) {
  return sin(x);
}

f64 Math__pow(f64 x, f64 y) {
  return pow(x, y);
}