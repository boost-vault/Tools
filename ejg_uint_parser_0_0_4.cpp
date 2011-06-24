/*=============================================================================
    Copyright (c) 2001-2009 Joel de Guzman

    Distributed under the Boost Software License, Version 1.0. (See accompanying
    file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
=============================================================================*/


// 
// Modified by Edward Grace to try out my timer on a real-world
// example of someone else.
//
//
// Typical results imply that spirit is ~1000% faster than atoi for
// small buffer sizes that fit in cache.  I modified MAX_ITERATION to
// BUFFER_SIZE as I feel this is more in line with what it represents.
//
// Before MAX_ITERATION presumably had to be large enough to observe
// reliable timings for the functions, now it does not - you should
// now be able to set BUFFER_SIZE to 1 and get reliable comparison
// times.  Of course there one is mainly measuring the function call
// overhead.
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

// For the existing uint code.
#include <boost/spirit/include/qi.hpp>
#include <boost/lexical_cast.hpp>
#include <climits>
#include <cstdlib>
#include <string>
#include <vector>

// For small values the speedup is phenomenal.  At first I thought it
// was the compiler optimizing out the call to spirit, but now I'm not
// so sure.  If the buffers can all fit in cache - spirit can
// presumably fly.
//
// I am still suspicious that somehow large swathes of the test code
// are being optimised out by the compiler.
#define BUFFER_SIZE 100 //00000


void check(int a, int b)
{
    if (a != b)
    {
        std::cout << "Parse Error" << std::endl;
        abort();
    }
}


// Global variables - prevention of compiler optimisation of the void
// atoi() and void strtol() functions below.
std::vector<int>  src(BUFFER_SIZE);
std::vector<std::string>  src_str(BUFFER_SIZE);
std::vector<int>  v(BUFFER_SIZE);

// Build a function that tests atoi
void wrap_atoi() {
  for (int i = 0; i < BUFFER_SIZE; ++i) 
    v[i] = atoi(src_str[i].c_str());   
}

// Build a function that tests strtol
void wrap_strtol() {
  for (int i = 0; i < BUFFER_SIZE; ++i)
    v[i] = strtol(src_str[i].c_str(), 0, 10);
}

// Again, global variables so that the optimizer (hopefully) cannot
// turn the void qi_parse() test function in to (literally) nothing.
//
// In future there would need to be a way of incorporating this type
// of functionality in a simple manner.
std::vector<char const*> f(BUFFER_SIZE);
std::vector<char const*> l(BUFFER_SIZE);

// Build a function that tests qi::parse
namespace qi = boost::spirit::qi;
using qi::int_;
void wrap_qi_parse() {
  for (int i = 0; i < BUFFER_SIZE; ++i) 
    qi::parse(f[i], l[i], int_, v[i]);
    
}


int main()
{
    std::cout << "initializing input strings..." << std::endl;
    for (int i = 0; i < BUFFER_SIZE; ++i)    {
      src[i] = std::rand() * std::rand();
      src_str[i] = boost::lexical_cast<std::string>(src[i]);
    }
    
    // Build the first last iterators. 
    // get the first/last iterators
    for (int i = 0; i < BUFFER_SIZE; ++i) {
      f[i] = src_str[i].c_str();
      l[i] = f[i];
      while (*l[i])
	l[i]++;
    }


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
    // timer.set_nominal_precision_target_percent(1.0);
    std::cout << "Calibrating overhead...";
        timer.calibrate_chrono_overhead();  
    std::cout << "...done\n";
    std::cout << "Timer overhead (t_c) ~= : " << timer.get_chrono_overhead() << std::endl;
    std::cout << "Jitter               ~= : " << timer.get_chrono_sigma() << std::endl;


    // For storing the minimum, median and maximum estimates of
    // percentage speedup
    double min,med,max;

    // Race qi_pare and atoi.
    timer.measure_percentage_speedup(wrap_qi_parse,wrap_atoi,min,med,max);
    std::cout << "qi_parse vs atoi    : " << min << " " << med << " " << max 
	      << "% faster." << std::endl;

    // Race qi_parse and strtol.
    timer.measure_percentage_speedup(wrap_qi_parse,wrap_strtol,min,med,max);
    std::cout << "qi_parse vs strtol  : " << min << " " << med << " " << max 
	      << "% faster." << std::endl;

   // For giggles, race strtol and atoi.
    timer.measure_percentage_speedup(wrap_strtol,wrap_atoi,min,med,max);
    std::cout << "strtol vs atoi      : " << min << " " << med << " " << max 
	      << "% faster." << std::endl;

    // Finally for the sake of argument race qi_parse and qi_parse,
    // they should not be statistically different.  In other words
    // expect the confidence interval to include zero.
     timer.measure_percentage_speedup(wrap_qi_parse,wrap_qi_parse,min,med,max);
    std::cout << "qi_parse vs qi_parse : " << min << " " << med << " " << max 
	      << "% faster." << std::endl;
   

    // Checking results since there's no way to do that when racing
    // the two in pairs.
    std::cout << "\n\n\nChecking that the results are correct...\n";
    v.clear(); v.resize(v.capacity());
    wrap_atoi();
    for (int i=0; i < BUFFER_SIZE; check(v[i], src[i]), ++i);
    std::cout << "atoi is behaving itself!" << std::endl;

    v.clear(); v.resize(v.capacity());
    wrap_strtol();
    for (int i=0; i < BUFFER_SIZE; check(v[i], src[i]), ++i);
    std::cout << "strtol is behaving itself!" << std::endl;

    v.clear(); v.resize(v.capacity());
    for (int i = 0; i < BUFFER_SIZE; ++i) {
      f[i] = src_str[i].c_str();
      l[i] = f[i];
      while (*l[i])
	l[i]++;
    }
    wrap_qi_parse();
    for (int i=0; i < BUFFER_SIZE; check(v[i], src[i]), ++i);
    std::cout << "qi is behaving itself!" << std::endl;


    std::cout << "\n\nAll done!\n";

    return 0;
}

