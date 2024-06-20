// https://people.math.sc.edu/Burkardt/cpp_src/brent/brent.html
#pragma once
#ifdef _DLL
#ifdef BRENTLIB_EXPORT
  #ifdef _DLL
    #define GLOBAL __declspec(dllexport)
  #else
    #define GLOBAL
  #endif
#else
#ifdef BRENTLIB_IMPORT
#define GLOBAL __declspec(dllimport)
#else
#define GLOBAL
#endif
#endif
#else
#define GLOBAL
#endif
namespace brent {

class func_base {
public:
   virtual double operator()(double) = 0;
};

//class simplefunc : public func_base {
//public:
//   virtual double operator() (double x);
//   simplefunc()
//};

class monicPoly : public func_base {
public:
   std::vector<double> coeff;
   virtual double operator() (double x);
   // constructors:
   monicPoly(const size_t degree)
      : coeff(degree) {}
   monicPoly(const std::vector<double>& v)
      : coeff(v) {}
   monicPoly(const double* c, size_t degree)
      : coeff(std::vector<double>(c, c + degree)) {}
};

class Poly : public func_base {
public:
   std::vector<double> coeff;    // a vector of size nterms i.e. 1+degree
   virtual double operator() (double x);
   // constructors:
   Poly(const size_t degree)
      : coeff(1 + degree) {}
   Poly(const std::vector<double>& v)
      : coeff(v) {}
   Poly(const double* c, size_t degree)
      : coeff(std::vector<double>(c, 1 + c + degree)) {}
};

//double glomin ( double a, double b, double c, double m, double e, double t, func_base& f, double &x );
//double local_min ( double a, double b, double t, func_base& f, double &x );
//double local_min_rc ( double &a, double &b, int &status, double value );
//double r8_epsilon ( );
//double r8_max ( double x, double y );
//double r8_sign ( double x );
//void timestamp ( );
GLOBAL double zero(double a, double b, double t, const std::function<double(double)>& f);
GLOBAL void zero_rc ( double a, double b, double t, double &arg, int &status, double value );

// === simple wrapper functions
// === for convenience and/or compatibility
GLOBAL double glomin ( double a, double b, double c, double m, double e, double t, double f( double x ), double &x );
GLOBAL double local_min ( double a, double b, double t, double f( double x ), double &x );
//GLOBAL double zero ( double a, double b, double t, double f( double x ) );
}
