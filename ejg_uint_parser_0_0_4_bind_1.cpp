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

void check(int a, int b) {    
  if (a != b) {
    std::cout << "Parse Error" << std::endl;
    abort();
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


int main() {
  // Define the nominal buffer size.  We are interested in speedups
  // that may be cache dependent.
  size_t buffer_size(100);
  std::cout << "Enter buffer size: ";
  std::cin >> buffer_size;
  
  
  // Reserve sizes for the required buffers.
  std::vector<int>  src(buffer_size);
  std::vector<std::string>  src_str(buffer_size);
  std::vector<int>  v(buffer_size);
  
  // Prepare the test strings.
  std::cout << "initializing input strings..." << std::endl;
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
  
  // Checking results since there's no way to do that when racing
  // the two in pairs.
  std::cout << "\n\n\nChecking that the parsers are functioning  correctly...\n";
  v.clear(); v.resize(v.capacity());
  run_atoi(src_str,v);
  for (size_t i=0; i < v.size(); check(v[i], src[i]), ++i);
  std::cout << "atoi is behaving itself!" << std::endl;
  
  v.clear(); v.resize(v.capacity());
  run_strtol(src_str,v);
  for (size_t i=0; i < v.size(); check(v[i], src[i]), ++i);
  std::cout << "strtol is behaving itself!" << std::endl;
  
  v.clear(); v.resize(v.capacity());
  run_qi_parse(f,l,v);
  for (size_t i=0; i < v.size(); check(v[i], src[i]), ++i);
  std::cout << "qi is behaving itself!" << std::endl;
  std::cout << "\nProceeding to timing tests.";
  
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
  timer.measure_percentage_speedup(boost::bind(run_qi_parse,f,l,v),
				   boost::bind(run_atoi,src_str,v),
				   min,med,max);
  std::cout << "qi_parse vs atoi     : " << min << " " << med << " " << max 
	    << "% faster." << std::endl;
  
  // Race qi_parse and strtol.
  timer.measure_percentage_speedup(boost::bind(run_qi_parse,f,l,v),
				   boost::bind(run_strtol,src_str,v),
				   min,med,max);
  std::cout << "qi_parse vs strtol  : " << min << " " << med << " " << max 
	    << "% faster." << std::endl;
  
  // For giggles, race strtol and atoi.
  timer.measure_percentage_speedup(boost::bind(run_strtol,src_str,v),
				   boost::bind(run_atoi,src_str,v),
				   min,med,max);
  std::cout << "strtol vs atoi       : " << min << " " << med << " " << max 
	    << "% faster." << std::endl;

  // Finally for the sake of argument race qi_parse and qi_parse,
  // they should not be statistically different.  In other words
  // expect the confidence interval to include zero.
  timer.measure_percentage_speedup(boost::bind(run_qi_parse,f,l,v),
				   boost::bind(run_qi_parse,f,l,v),
				   min, med, max);
  std::cout << "qi_parse vs qi_parse : " << min << " " << med << " " << max 
	    << "% faster." << std::endl;
   
  std::cout << "\nAll done!\n";
  
  return 0;
}

