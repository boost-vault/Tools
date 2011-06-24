/*=============================================================================
    Copyright (c) 2001-2009 Joel de Guzman

    Distributed under the Boost Software License, Version 1.0. (See accompanying
    file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
=============================================================================*/


// 
// Modified by Edward Grace to try out my timer on a real-world
// example of someone else.
//
// The functions we wish to time are run_atoi, run_strtol and
// run_qi_parse.  These and are then bound using boost::bind to form
// nullary functions.
// 
// This example is more about comparing different techniques for
// measuring speedup than it is for comparing the functions
// themselves. It is often tempting to compare absolute times,
// particularly across machines. By using the measure_infinity_time()
// method and the nanoseconds() method this could be a viable
// approach, however one needs to be aware that the errors in the
// slope could be subtly hidden in this model yielding results that
// appear more precise than they actually are.
//
// The measure_infinity_time() method assumes that doubling the number
// of iterations should double the length of time it takes to run the
// function.  With this in mind the linear model, with unity slope, is
// compared against observations until the measured (transformed)
// slope is 1 +/- the given error bound.  The slope is determined from
// a minimum absolute deviation regression and is therefore quite
// robust against outliers from the model due to burst loads but can
// still be skewed due to endemic differences.
//
// On the other hand the Wilcoxon based technique does not assume a
// linear model, that doubling the number of iterations should double
// the observed time.  It assumes very little about the underlying
// relationship between the times and iterations.  Consequently, when
// used with low nominal precision, the estimated speedups can be
// considered as indicative of what may occur when one does not call
// this given function repetitively with the same data.  It could be
// argued that this is a more reasonable reflection of what occurs in
// reality - particularly with fast functions.
//
// For example, when calling this with a buffer size of 1 and nominal
// quantum precision of 15%, a single element, the Wilcoxon test may
// suggest a speedup confidence interval of [ -5 -- 20]% (no
// difference) for strtol vs atoi as compared to [+4.5 -- 14]% (strol
// is faster) for the measure_infinity_time approach.  These are
// clearly different -- which one is correct? The answer to this
// depends on your assumptions.  If you assume that the repeated
// calling of the function over and over again is a reasonable
// reflection of what will happen (infinity time) then the 'correct'
// assumption is that strol is faster, if you do not then the
// 'correct' assumption is that there is no significant difference.
//

#if defined(BOOST_MSVC)
#pragma inline_depth(255)
#pragma inline_recursion(on)
#define _SECURE_SCL 0 
#endif // defined(BOOST_MSVC)

// The proposed timer.
#include <ejg/timer.hpp>
// A chronometer from the FFTW project, used for high precision timing.
#include "cycle.h"
// For binding arguments of functions to time.
#include <boost/bind.hpp>

// For the existing uint code.
#include <boost/spirit/include/qi.hpp>
#include <boost/lexical_cast.hpp>
#include <climits>
#include <cstdlib>
#include <string>
#include <vector>


// Return the ratio of two times.
double ratio(double ta, double tb) {
  return tb/ta;
}

// Given a ratio of speeds return a percentage speedup.
double speedup(double r) {
  return 100.0*(r - 1.0);
}


// Return the expected error in the ratio given the error in the
// fractions on the assumptions that the errors are standard
// deviations of a normal. frac_a == sigma_a/a
//
// The default assumed correlation coefficient is 0.0.  When used for
// the errors based on fitting the linear model this is likely to be
// quite large - though difficult to ascertain directly.
//
//
double frac_ratio(double frac_a, 
		  double frac_b, 
		  double corr=0.0) {
  double var;
  // Notice correlation decreases the actual error.
  var = frac_a*frac_a  + frac_b*frac_b 
    - 2.0*frac_a*frac_b*corr; 
  return std::sqrt(var);
}
  

void check_all(const std::string &function_name, 
	       const std::vector<int> &v,
	       const std::vector<int> &src) {
  assert(v.size() == src.size());
  for (size_t i=0; i < v.size(); ++i) {
    if (v[i] != src[i]) {
      std::cerr << "Parse error with function " << function_name << "!" << std::endl;
      abort();
    }
  }
}

// Execute atoi over the input strings, generating the output values.
void run_atoi(const std::vector<std::string> &src_str,
	      std::vector<int> &v) {
  assert(v.size() == src_str.size());
  for (size_t i = 0; i < v.size(); ++i) 
    v[i] = atoi(src_str[i].c_str());   
}

// Execute strtol over the input strings, generating the output values.
void run_strtol(const std::vector<std::string> &src_str,
		std::vector<int> &v) {
  assert(v.size() == src_str.size());
  for (size_t i = 0; i < v.size(); ++i)
    v[i] = strtol(src_str[i].c_str(), 0, 10);
}


// Execute qi::parse over the input strings, generating the output values.
void run_qi_parse(const std::vector<char const*> &f,
		  const std::vector<char const*> &l,
		  std::vector<int> &v) {
  using namespace boost::spirit::qi;
  assert(v.size() == f.size());
  assert(v.size() == l.size());
  for (size_t i = 0; i < v.size(); ++i) {
    // The following highlights the importance of const correctness.
    // spirit::parse modifies the first iterator, so we must make sure
    // it is reset.  
    //
    // Without specifying &f as const above this would be easy to
    // miss.
    char const *iter = f[i]; // Thanks to OvermindDL1
    parse(iter, l[i], int_, v[i]); 
  }
}


// In terms of making incorrect decisions (identifying something as
// faster when it is not etc.), the linear fit point estimate appears
// to be good enough - at least at first sight. It is quite robust
// against machine loading and other effects since infinity_time makes
// use of a least absolute deviation fit rather than a least squares
// fit.
//
// The relative measurements using the point estimates are less
// influenced by an incorrectly calibrated clock than the Wilcoxon
// based ones since they simply iterate until any nonlinearities in
// the slope of the expected fit (whatever they are) are reduced.
//
// As a consequence of these effects, for a given precision in the
// confidence bound, it appears that the Wilcoxon technique is more
// efficient - in that it takes less time to obtain the same precision
// result.  This is inconclusive however.
int main() {
  using std::setw;
  using std::cout;
  using std::cin;
  using std::endl;

  // Define the nominal buffer size.  Large buffers, with a wide
  // variety of input, allows one to average over any data dependent
  // run-time effects.
  size_t buffer_size(100);
  cout << "Enter buffer size: ";
  cin >> buffer_size;
  
  // The nominal precision for a measurement quantum should be +- this
  // percentage.  For a given measured value of the clock time the
  // number of iterations should be chosen so that the value is at
  // least within this precision.
  double nominal_precision(10);
  cout << "Enter nominal precision (%): ";
  cin >> nominal_precision;
  
  
  // Reserve sizes for the required buffers.
  std::vector<int>  src(buffer_size);
  std::vector<std::string>  src_str(buffer_size);
  std::vector<int>  v(buffer_size);
  
  // Prepare the test strings.
  cout << "initializing input strings..." << endl;
  assert(src.size() == src_str.size());
  for (size_t i = 0; i < src.size(); ++i)    {
    src[i] = std::rand() * std::rand();
    src_str[i] = boost::lexical_cast<std::string>(src[i]);
  }

  // Populate first-last iterator lists. Now that the call to
  // run_qi_parse specifies these as const references we know calls to
  // run_qi_parse will not modify them.
  std::vector<char const*> f(buffer_size);
  std::vector<char const*> l(buffer_size);
  for (size_t i = 0; i < buffer_size; ++i) {
    f[i] = src_str[i].c_str();
    l[i] = f[i];
    while (*l[i]) l[i]++;
  }
  
  // Checking results since there's no  way to do that when racing the
  // two in  pairs. This  does not report  success, only  failure.  No
  // news is good news.
  v.clear(); v.resize(v.capacity());
  run_atoi(src_str,v);
  check_all("atoi",v,src);
  
  v.clear(); v.resize(v.capacity());
  run_strtol(src_str,v);
  check_all("strtol",v,src);
  
  v.clear(); v.resize(v.capacity());
  run_qi_parse(f,l,v);
  check_all("qi_parse",v,src);
  
  // It's preferable to use a CPU clock tick counter if you can,
  // they are generally much higher frequency.  This means the
  // timing experiment will be quicker - it shouldn't change the
  // result.  
  ejg::generic_timer<ticks> timer(getticks,elapsed);
  
  // The following should work instead.  It is the default, based on
  // the prevailing (slow) std::clock() function. You should get
  // similar results however it will take far longer as the clock
  // operates at a much lower frequency.
  //    ejg::crude_timer timer;
  
  // If the returned confidence bounds (min,max) include zero, you
  // may need to improve the nominal precision. The default should
  // be fine!  
  //
  timer.set_nominal_precision_target_percent(nominal_precision);
  cout << "\n\n"
       << "Nominal precision of quantum: " 
       << timer.get_nominal_precision_target_percent() 
       << "%" << endl;

  cout << "Timer overhead (t_c) (ticks): " << std::flush;
  timer.calibrate_chrono_overhead();  
  cout << timer.get_chrono_overhead() << endl;
  cout << "Jitter               (ticks): " 
       << timer.get_chrono_sigma() << endl;
  cout << "Approx clock frequency (GHz): " << std::flush;
  timer.calibrate_seconds();
  cout << 1.0/timer.nanoseconds(1.0) << std::endl;
  cout << "\n";

  // For storing the minimum, median and maximum estimates of
  // percentage speedup
  double min,med,max;
  
  // Determine 'infinity time', this is a point estimate of the run
  // time in clock ticks (what ever they are) that should be accurate
  // to at least the same precision as defined by
  // .set_nominal_precision_target().
  //
  // To attempt to convert clock ticks to real seconds you must use
  // .seconds(), or a similar method.  This is not guaranteed since
  // there is not guarantee that (for example) the clock frequency is
  // constant.  On architectures that implement dynamic frequency
  // scaling it clearly won't be.  For this reason:
  //
  //  * If you use a timer that guarantees to represent wall clock
  //    (real) time then your measurements may be inaccurate if the
  //    clock frequency changes during the measurement simply because
  //    'clock time' is more appropriate for measuring the function
  //    than actual wall clock time.
  //
  //  * If you use a timer that represents the clock frequency then
  //    you will end up with a better estimate of the function speed
  //    in terms of clock cycles however if you convert it to real
  //    seconds you may find that, because the clock frequency is not
  //    guaranteed to be fixed, the resulting time is misleading.
  //
  // Either way, unless one can address the above points, absolute
  // times measured should be used with great care.
  //
  // Far better is to use a ratio scale that will account for common
  // errors (e.g. measure_percentage_speedup.
  //
  //

  // Obtain a point estimate of the qi_parse time.
  double t_infinity; // Point estimate of the time taken to execute
		     // one iteration in the limit of a large number
		     // of iterations.  This should be accurate to
		     // (nominally 10%).
  double mad;        // Median Absolute Deviation of the linear fit
		     // from the expected model.
  double intercept; // Intercept of the linear fit.


  // First set the output precision to approximately match
  // .nominal_precision_target(). For example +/- 10% means that
  // anything after the first significant figure is definitely to be
  // viewed with suspicion!
  int out_precision = std::ceil(-1*std::log10(timer.get_nominal_precision_target_percent()/100.0));
  out_precision = out_precision < 0 ? 1 : out_precision;
  // We deliberately show one more digit than the nominal precision.
  out_precision++;
  


  
  cout << "Direct point estimates of actual run time.\n";
  // Set the output precision (sig figs) to approximately the same as
  // the quantum precision.
  cout.precision(out_precision);
  cout << setw(10) << " " 
       << setw(3)  << " "
       << setw(10) << "Function"
       << setw(13) << "T - delta" 
       << setw(13) << "T (best)" 
       << setw(13) << "T + delta" 
       << endl;
  cout << setw(10) << " "
       << setw(3)  << " "
       << setw(10) << " " 
       << setw(13) << "(ns/char)" 
       << setw(13) << "(ns/char)"
       << setw(13) << "(ns/char)" 
       << endl;
    
  // If the error is normal the following would approximately yield
  // the sigma confidence interval such that 95.4% of observations
  // would be within +/- 2 sigma of the central value.  This is in
  // keeping with the confidence intervals of the Wilcoxon technique,
  // which default to 95%.
  //
  // We do not know what the error is, it consists of two parts:
  // systematic and random.  While the random part might be determined
  // from a bootstrap estimate, the systematic part is more difficult
  // to ascertain.  This could show up in the intercept of the linear
  // model or structure in the residuals.  In the case of a pure
  // linear relationship the intercept should be zero.
  //
  // For now, any inferred error from this should be consumed with a
  // large pinch of cheap salt since it is based on guesswork.
  double frac = timer.get_nominal_precision_target_percent()/100.0;


  // In principle if we are fitting to a linear model, which is not a
  // bad approach, errors on the t_infinity time could be determined
  // more rigorously by a bootstrapping procedure.  
  timer.measure_infinity_time(boost::bind(run_qi_parse,f,l,v),t_infinity,intercept,mad);
  t_infinity = timer.nanoseconds(t_infinity)/double(buffer_size);
  double t_qi_parse(t_infinity);
  cout << setw(10) << " "
       << setw(3)  << " "
       << setw(10) << "qi_parse" 
       << setw(13) << t_infinity*(1.0 - 2.0*frac)
       << setw(13) << t_infinity
       << setw(13) << t_infinity*(1.0 + 2.0*frac)
       << endl;

  timer.measure_infinity_time(boost::bind(run_strtol,src_str,v),t_infinity,intercept,mad);
  t_infinity = timer.nanoseconds(t_infinity)/double(buffer_size);
  double t_strtol(t_infinity);
  cout << setw(10) << " "
       << setw(3)  << " "
       << setw(10) << "strtol"    
       << setw(13) << t_infinity*(1.0 - 2.0*frac)
       << setw(13) << t_infinity
       << setw(13) << t_infinity*(1.0 + 2.0*frac)
       << endl;

  timer.measure_infinity_time(boost::bind(run_atoi,src_str,v),t_infinity,intercept,mad);
  t_infinity = timer.nanoseconds(t_infinity)/double(buffer_size);
  double t_atoi(t_infinity);
  cout << setw(10) << " " 
       << setw(3)  << " " 
       << setw(10) << "atoi"
       << setw(13) << t_infinity*(1.0 - 2.0*frac)
       << setw(13) << t_infinity
       << setw(13) << t_infinity*(1.0 + 2.0*frac)
       << endl;
  cout << "\n";


  cout << "Speedup percentages based on Wilcoxon matched pair confidence intervals.\n";
  cout << setw(10) << "Func. A" 
       << setw(3)  << "vs"
       << setw(10) << "Func. B"
       << setw(13) << "Minimum"
       << setw(13) << "Median"
       << setw(13) << "Maximum"
       << endl;
  cout << setw(10) << " "
       << setw(3)  << " "
       << setw(10) << " "
       << setw(13) << "(% faster)"
       << setw(13) << "(% faster)"
       << setw(13) << "(% faster)"
       << endl;

  // Race qi_pare and atoi.
  timer.measure_percentage_speedup(boost::bind(run_qi_parse,f,l,v),
				   boost::bind(run_atoi,src_str,v),
				   min,med,max);

  // The precision reported in the percentage speedups can be better
  // than the precision of any individual spot estimate simply because
  // of the way the (bias) errors combine.  Crudely we report an extra
  // place of precision in the percentage speedups.  
  out_precision++;
  cout.precision(out_precision);
  cout << setw(10) << "qi_parse"
       << setw(3)  << ""
       << setw(10) << "atoi"
       << setw(13) << min 
       << setw(13) << med 
       << setw(13) << max
       << endl;

  // Race qi_parse and strtol.
  timer.measure_percentage_speedup(boost::bind(run_qi_parse,f,l,v),
				   boost::bind(run_strtol,src_str,v),
				   min,med,max);
   cout << setw(10) << "qi_parse"
	<< setw(3)  << ""
	<< setw(10) << "strtol"
	<< setw(13) << min 
	<< setw(13) << med 
	<< setw(13) << max
	<< endl;


  // For giggles, race strtol and atoi.
  timer.measure_percentage_speedup(boost::bind(run_strtol,src_str,v),
				   boost::bind(run_atoi,src_str,v),
				   min,med,max);
  cout << setw(10) << "strtol"
       << setw(3)  << ""
       << setw(10) << "atoi"
       << setw(13) << min 
       << setw(13) << med 
       << setw(13) << max
       << endl;

  // Finally for the sake of argument race qi_parse and qi_parse,
  // they should not be statistically different.  In other words
  // expect the confidence interval to include zero.
  timer.measure_percentage_speedup(boost::bind(run_qi_parse,f,l,v),
				   boost::bind(run_qi_parse,f,l,v),
				   min, med, max);
  cout << setw(10) << "qi_parse"
       << setw(3)  << ""
       << setw(10) << "qi_parse"
       << setw(13) << min 
       << setw(13) << med 
       << setw(13) << max
       << endl; 


  

  // Compare the confidence intervals on the percentage speedup above
  // with the percentage speedups determined from the (fairly good)
  // point estimates.
  cout << "\n"
       << "Speedup percentages based on the point estimates.\n";
  cout << setw(10) << "Func. A" 
       << setw(3)  << "vs"
       << setw(10) << "Func. B"
       << setw(13) << "Best - err"
       << setw(13) << "Best"
       << setw(13) << "Best + err"
       << endl;
  cout << setw(10) << " "
       << setw(3)  << " "
       << setw(10) << " "
       << setw(13) << "(% faster)"
       << setw(13) << "(% faster)"
       << setw(13) << "(% faster)"
       << endl;


  // Obtain a second infinity-time point estimate for qi parse.
  timer.measure_infinity_time(boost::bind(run_qi_parse,f,l,v),t_infinity,intercept,mad);
  t_infinity = timer.nanoseconds(t_infinity)/double(buffer_size);
  double t_qi_parse_2(t_infinity);
  

  double r;
  // This is notionally a constant since we demand a given level of
  // fractional error as a minimum.  The correlation between the two
  // fractional errors is probably quite high.  For me, the value of
  // 0.92 yields qualitatively consistent results to the Wilcoxon
  // technique providing care is taken not to load the machine. 
  //
  double r_err(frac_ratio(frac,frac,
			  0.92)); /**< Guessed correlation of errors. */

  r = ratio(t_qi_parse,t_atoi);
  cout << setw(10) << "qi_parse"
       << setw( 3) << " "
       << setw(10) << "atoi" 
       << setw(13) << speedup(r - r_err)
       << setw(13) << speedup(r)
       << setw(13) << speedup(r + r_err)
       << endl;

  r = ratio(t_qi_parse,t_strtol);
  cout << setw(10) << "qi_parse"
       << setw( 3) << " "
       << setw(10) << "strtol" 
       << setw(13) << speedup(r - r_err)
       << setw(13) << speedup(r)
       << setw(13) << speedup(r + r_err)
       << endl;
  
  r = ratio(t_strtol,t_atoi);
  cout << setw(10) << "strtol"
       << setw( 3) << " "
       << setw(10) << "atoi" 
       << setw(13) << speedup(r - r_err)
       << setw(13) << speedup(r)
       << setw(13) << speedup(r + r_err)
       << endl;

  r = ratio(t_qi_parse,t_qi_parse_2);
  cout << setw(10) << "qi_parse"
       << setw(3)  << " "
       << setw(10) << "qi_parse"
       << setw(13) << speedup(r - r_err)
       << setw(13) << speedup(r)
       << setw(13) << speedup(r + r_err)
       << endl; 


  cout << "\nAll done!\n";
  
  return 0;
}
