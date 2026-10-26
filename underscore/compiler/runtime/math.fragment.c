/*
 * Underscore native runtime fragment: math
 *
 * Source contract: docs/libSyntaxList._, the `math` section.
 * Intended to be emitted into generated C by codegen. These helpers use double
 * for the runtime calculation; the language's declared type checker/codegen
 * controls conversions at the call site.
 *
 * POSIX-like toolchains usually need to link the generated executable with -lm.
 * The project compiler's final link command must add that flag when math is
 * included. NAN/domain/overflow results follow the platform C math library.
 */

#include <math.h>

static double uc_math_abs(double value) { return fabs(value); }
static double uc_math_sqrt(double value) { return sqrt(value); }
static double uc_math_min(double left, double right) { return left < right ? left : right; }
static double uc_math_max(double left, double right) { return left > right ? left : right; }
static double uc_math_round(double value) { return round(value); }
static double uc_math_rdown(double value) { return floor(value); }
static double uc_math_rup(double value) { return ceil(value); }
static double uc_math_sin(double value) { return sin(value); }
static double uc_math_tan(double value) { return tan(value); }
static double uc_math_cos(double value) { return cos(value); }
static double uc_math_asin(double value) { return asin(value); }
static double uc_math_atan(double value) { return atan(value); }
static double uc_math_acos(double value) { return acos(value); }
static double uc_math_log(double value) { return log(value); }

/* logx(value, base): logarithm of value in the requested base. */
static double uc_math_logx(double value, double base) {
    if (value <= 0.0 || base <= 0.0 || base == 1.0) {
        return NAN;
    }
    return log(value) / log(base);
}

static double uc_math_exp(double value) { return exp(value); }
static double uc_math_pi(void) { return 3.14159265358979323846264338327950288; }
