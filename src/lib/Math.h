#ifndef MATH_H
#define MATH_H

#include "Base.h"

#define Math__PI M_PI

f64 Math__map(f64 n, f64 input_start, f64 input_end, f64 output_start, f64 output_end);
f64 Math__mod(f64 a, f64 b);
f64 Math__sin(f64 x);
f64 Math__pow(f64 x, f64 y);

#endif