/**
 * @brief Extracted functions specifically related to cx::pow sourced from https://github.com/elbeno/constexpr
 *
 * Use permitted under the terms of the MIT License.
 *
 * @author elbeno
 * @date 2024
 * @license MIT
 */
#pragma once

#include <limits>
#include <type_traits>

// -----------------------------------------------------------------------------
// constexpr math functions

// Synopsis: all functions are in the cx namespace

// -----------------------------------------------------------------------------
// Promoted (Arithmetic1 x, Arithmetic2 y);

// Promotion rules:
// When either of Arithmetic1 or Arithmetic2 is long double, Promoted is long
// double. Otherwise Promoted is double.

// -----------------------------------------------------------------------------
// exponent function (e^x)

// float exp(float x);
// double exp(double x);
// long double exp(long double x);
// double exp(Integral x);

// -----------------------------------------------------------------------------
// logarithm functions

// float log(float x);
// double log(double x);
// long double log(long double x);
// double log(Integral x);

// float log10(float x);
// double log10(double x);
// long double log10(long double x);
// double log10(Integral x);

// float log2(float x);
// double log2(double x);
// long double log2(long double x);
// double log2(Integral x);

// -----------------------------------------------------------------------------
// power function

// float pow(float x, float y);
// double pow(double x, double y);
// long double pow(long double x, long double y);
// Promoted pow(Arithmetic1 x, Arithmetic2 y);

namespace cx
{
  namespace err
  {
    namespace
    {
      extern const char* abs_runtime_error;
      extern const char* fabs_runtime_error;
      extern const char* sqrt_domain_error;
      extern const char* cbrt_runtime_error;
      extern const char* exp_runtime_error;
      extern const char* sin_runtime_error;
      extern const char* cos_runtime_error;
      extern const char* tan_domain_error;
      extern const char* atan_runtime_error;
      extern const char* atan2_domain_error;
      extern const char* asin_domain_error;
      extern const char* acos_domain_error;
      extern const char* floor_runtime_error;
      extern const char* ceil_runtime_error;
      extern const char* fmod_domain_error;
      extern const char* remainder_domain_error;
      extern const char* fmax_runtime_error;
      extern const char* fmin_runtime_error;
      extern const char* fdim_runtime_error;
      extern const char* log_domain_error;
      extern const char* tanh_domain_error;
      extern const char* acosh_domain_error;
      extern const char* atanh_domain_error;
      extern const char* pow_runtime_error;
      extern const char* erf_runtime_error;
    }
  }

  //----------------------------------------------------------------------------
  namespace detail
  {
    // test whether values are within machine epsilon, used for algorithm
    // termination
    template <typename T>
    constexpr bool feq(T x, T y)
    {
      return abs(x - y) <= std::numeric_limits<T>::epsilon();
    }
  }

  //----------------------------------------------------------------------------
  // raise to integer power
  namespace detail
  {
    template <typename FloatingPoint>
    constexpr FloatingPoint ipow(
        FloatingPoint x, int n,
        typename std::enable_if<std::is_floating_point<FloatingPoint>::value>::type* = nullptr)
    {
      return (n == 0) ? FloatingPoint{1} :
      n == 1 ? x :
        n > 1 ? ((n & 1) ? x * ipow(x, n-1) : ipow(x, n/2) * ipow(x, n/2)) :
        FloatingPoint{1} / ipow(x, -n);
    }
  }

  //----------------------------------------------------------------------------
  // promoted
  template <typename A1, typename A2>
  struct promoted
  {
    using type = double;
  };
  template <typename A>
  struct promoted<long double, A>
  {
    using type = long double;
  };
  template <typename A>
  struct promoted<A, long double>
  {
    using type = long double;
  };
  template <>
  struct promoted<long double, long double>
  {
    using type = long double;
  };

  template <typename A1, typename A2>
  using promoted_t = typename promoted<A1, A2>::type;

  //----------------------------------------------------------------------------
  // exp by Taylor series expansion
  namespace detail
  {
    template <typename T>
    constexpr T exp(T x, T sum, T n, int i, T t)
    {
      return feq(sum, sum + t/n) ?
        sum :
        exp(x, sum + t/n, n * i, i+1, t * x);
    }
  }
  template <typename FloatingPoint>
  constexpr FloatingPoint exp(
      FloatingPoint x,
      typename std::enable_if<std::is_floating_point<FloatingPoint>::value>::type* = nullptr)
  {
    return true ? detail::exp(x, FloatingPoint{1}, FloatingPoint{1}, 2, x) :
      throw err::exp_runtime_error;
  }
  template <typename Integral>
  constexpr double exp(
      Integral x,
      typename std::enable_if<std::is_integral<Integral>::value>::type* = nullptr)
  {
    return detail::exp<double>(x, 1.0, 1.0, 2, x);
  }

  //----------------------------------------------------------------------------
  // natural logarithm using
  // https://en.wikipedia.org/wiki/Natural_logarithm#High_precision
  // domain error occurs if x <= 0
  namespace detail
  {
    template <typename T>
    constexpr T log_iter(T x, T y)
    {
      return y + T{2} * (x - cx::exp(y)) / (x + cx::exp(y));
    }
    template <typename T>
    constexpr T log(T x, T y)
    {
      return feq(y, log_iter(x, y)) ? y : log(x, log_iter(x, y));
    }
    constexpr long double e()
    {
      return 2.71828182845904523536l;
    }
    // For numerical stability, constrain the domain to be x > 0.25 && x < 1024
    // - multiply/divide as necessary. To achieve the desired recursion depth
    // constraint, we need to account for the max double. So we'll divide by
    // e^5. If you want to compute a compile-time log of huge or tiny long
    // doubles, YMMV.

    // if x <= 1, we will multiply by e^5 repeatedly until x > 1
    template <typename T>
    constexpr T logGT(T x)
    {
      return x > T{0.25} ? log(x, T{0}) :
        logGT<T>(x * e() * e() * e() * e() * e()) - T{5};
    }
    // if x >= 2e10, we will divide by e^5 repeatedly until x < 2e10
    template <typename T>
    constexpr T logLT(T x)
    {
      return x < T{1024} ? log(x, T{0}) :
        logLT<T>(x / (e() * e() * e() * e() * e())) + T{5};
    }
  }
  template <typename FloatingPoint>
  constexpr FloatingPoint log(
      FloatingPoint x,
      typename std::enable_if<std::is_floating_point<FloatingPoint>::value>::type* = nullptr)
  {
    return x < 0 ? throw err::log_domain_error :
      x >= FloatingPoint{1024} ? detail::logLT(x) :
      detail::logGT(x);
  }
  template <typename Integral>
  constexpr double log(
      Integral x,
      typename std::enable_if<std::is_integral<Integral>::value>::type* = nullptr)
  {
    return log(static_cast<double>(x));
  }


  //----------------------------------------------------------------------------
  // pow: compute x^y
  // a = x^y = (exp(log(x)))^y = exp(log(x)*y)
  template <typename FloatingPoint>
  constexpr FloatingPoint pow(
      FloatingPoint x, FloatingPoint y,
      typename std::enable_if<std::is_floating_point<FloatingPoint>::value>::type* = nullptr)
  {
    return true ? exp(log(x)*y) :
      throw err::pow_runtime_error;
  }
  template <typename FloatingPoint>
  constexpr FloatingPoint pow(
      FloatingPoint x, int y,
      typename std::enable_if<std::is_floating_point<FloatingPoint>::value>::type* = nullptr)
  {
    return true ? detail::ipow(x, y) :
      throw err::pow_runtime_error;
  }

  // pow for general arithmetic types
  template <typename Arithmetic1, typename Arithmetic2>
  constexpr promoted_t<Arithmetic1, Arithmetic2> pow(
      Arithmetic1 x, Arithmetic2 y,
      typename std::enable_if<
        std::is_arithmetic<Arithmetic1>::value
        && std::is_arithmetic<Arithmetic2>::value>::type* = nullptr)
  {
    using P = promoted_t<Arithmetic1, Arithmetic2>;
    return pow(static_cast<P>(x), static_cast<P>(y));
  }
  template <typename Integral>
  constexpr promoted_t<Integral, int> pow(
      Integral x, int y,
      typename std::enable_if<
        std::is_integral<Integral>::value>::type* = nullptr)
  {
    return true ? detail::ipow(static_cast<double>(x), y) :
      throw err::pow_runtime_error;
  }
}