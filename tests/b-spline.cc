// Copyright (C) 2013 by Alexander Werner, DLR.
// Copyright (C) 2014 by Benjamin Chrétien, CNRS-LIRMM.
//
// This file is part of the roboptim.
//
// roboptim is free software: you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// roboptim is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public License
// along with roboptim.  If not, see <http://www.gnu.org/licenses/>.

#include "shared-tests/common.hh"

#include <roboptim/trajectory/cubic-b-spline.hh>
#include <roboptim/trajectory/b-spline.hh>

#include <roboptim/core/visualization/gnuplot.hh>
#include <roboptim/core/visualization/gnuplot-commands.hh>
#include <roboptim/core/visualization/gnuplot-function.hh>

#include <cstdlib>
#include <limits>
#include <cassert>
#include <cmath>
#include <iostream>

using namespace roboptim;
using namespace std;

BOOST_FIXTURE_TEST_SUITE (trajectory, TestSuiteConfiguration)

static double tol = 1e-6;

typedef Function::vector_t vector_t;
typedef DifferentiableFunction::gradient_t gradient_t;
typedef Function::value_type value_type;

template <int N>
struct spline_checks
{
  static void
  check_evaluate (std::pair<value_type, value_type> interval,
		  int dimension, vector_t const& params, int order)
  {
    // do nothing
  };

  static void check_non_uniform
  (std::pair<value_type, value_type> interval, int dimension,
   vector_t const& params, vector_t const& knots, int order);
};


// Evaluate the spline and its derivatives, thus implicitly checking
// basisPolynomials_ and interval() against old CubicBSpline
// implementation.
template <>
void
spline_checks<3>::check_evaluate
(std::pair<value_type, value_type> interval, int dimension,
 vector_t const& params, int order)
{
  CubicBSpline old_spline (interval, dimension, params);
  BSpline<3> new_spline (interval, dimension, params);

  for (value_type t = interval.first; t < interval.second; t += 1e-3)
    {
      gradient_t old_res (dimension);
      gradient_t new_res (dimension);
      gradient_t delta (dimension);

      old_spline.derivative (old_res, t, order);
      new_spline.derivative (new_res, t, order);
      delta = old_res - new_res;

      BOOST_CHECK ((abs (delta.array ()) < tol).any ());
    }
}

template <>
void spline_checks<5>::check_evaluate
(std::pair<value_type, value_type> interval, int dimension,
 vector_t const& params, int order)
{
  BSpline<5> new_spline (interval, dimension, params);

  for (value_type t = interval.first; t < interval.second; t += 1e-3)
    {
      gradient_t new_res (dimension);
      new_spline.derivative (new_res, t, order);
      value_type min = params.minCoeff ();
      value_type max = params.maxCoeff ();
      if (order == 0)
        {
	  // test if spline is in between minimum and maximum
	  // parameter
	  BOOST_CHECK ((new_res.array () > min).all ()
		       && (new_res.array () < max).all ());
	  // FIXME: very generous assumption - especially for
	  // multi-dimensional splines
        }
    }
}

template <int N>
void
spline_checks<N>::check_non_uniform
(std::pair<value_type, value_type> interval, int dimension,
 vector_t const& params, vector_t const& knots, int order)
{
  BSpline<N> new_spline (interval, dimension, params, knots);

  for (value_type t = interval.first; t < interval.second; t += 1e-3)
    {
      gradient_t res (dimension);
      new_spline.derivative (res, t, order);
      value_type min = params.minCoeff ();
      value_type max = params.maxCoeff ();
      if (order == 0)
        {
	  // test if spline is in between minimum and maximum parameter
	  BOOST_CHECK (res.minCoeff() >= min);
	  BOOST_CHECK (res.maxCoeff() <= max);
	  // FIXME: very generous assumption - especially for
	  // multi-dimensional splines
        }
    }
}


template <int N>
void test_instantiate ()
{
  vector_t params (10);
  params.setZero ();

  typename BSpline<N>::interval_t interval = std::make_pair (0., 1.);
  {
    BSpline<N> spline (interval, 1, params);
    std::cout << spline << std::endl;
  }

  vector_t knots (params.size () + N + 1);
  knots.setLinSpaced (0., 1.);
  {
    BSpline<N> spline (interval, 1, params, knots);
    std::cout << spline << std::endl;
  }
}

template <int N>
void test_evaluate ()
{
  for (int derivative = 0; derivative <= 0; derivative++)
    {
      for (int dimension = 1; dimension < 2; dimension++)
        {
	  std::pair<value_type, value_type> interval = std::make_pair (0., 1.);
	  int min_params = N + 1 + 10;
	  vector_t params (dimension * min_params);
	  params.setRandom ();
	  params *= 10.;

	  spline_checks<N>::check_evaluate
            (interval, dimension, params, derivative);
        }
    }
}

template <int N>
void test_non_uniform ()
{
  for (int derivative = 0; derivative <= 2; derivative++)
    {
      for (int dimension = 1; dimension < 4; dimension++)
        {
	  std::pair<value_type, value_type> interval = std::make_pair (0., 1.);
	  int min_params = N + 1;
	  int params_c = N + 1 + N;
	  assert (params_c > min_params);
	  vector_t params (dimension * params_c);
	  params.setRandom ();
	  params *= 10.;

	  vector_t knots (params_c + N + 1);
	  knots.head (N).setConstant (interval.first);
	  knots.segment (N, 2 + N).setLinSpaced (interval.first, interval.second);
	  knots.tail (N).setConstant (interval.second);

	  spline_checks<N>::check_non_uniform
            (interval, dimension, params, knots, derivative);
        }
    }
}


template <int N>
void test_plot ()
{
  using namespace roboptim::visualization;
  using namespace roboptim::visualization::gnuplot;

  std::pair<value_type, value_type> interval = std::make_pair (0., 1.);

  int min_params = N + 1;
  int params_c = N + 1 + N;
  assert (params_c >= min_params);
  vector_t params (params_c);
  params.setRandom();
  params *= 10.;

  BSpline<N> spline (interval, 1, params);

  Gnuplot gnuplot = Gnuplot::make_interactive_gnuplot ();
  discreteInterval_t plot_interval (interval.first, interval.second, 0.01);

  // TODO: use RobOptim's gnuplot functions
  std::cout << "set terminal wxt persist" << std::endl;
  std::cout << "plot '-' title 'B-Spline' with line, \\" << std::endl;
  std::cout << "'-' title 'interval' with line" << std::endl;
  for (value_type t = interval.first; t < interval.second; t += 1e-2)
    std::cout << t << " " << spline (t)[0] << std::endl;

  std::cout << "e" << std::endl;
  for (value_type t = interval.first; t < interval.second; t += 1e-2)
    std::cout << t << " " <<  spline.interval (t) << std::endl;

  std::cout << "e" << std::endl;
}


BOOST_AUTO_TEST_CASE (trajectory_bspline)
{
  test_instantiate<2> ();
  test_instantiate<3> ();
  test_instantiate<4> ();
  test_instantiate<5> ();

  srand (static_cast<unsigned int> (time (0)));
  test_plot<3> ();
  test_plot<5> ();

  test_evaluate<3> ();
  test_evaluate<5> ();

  test_non_uniform<3> ();
  test_non_uniform<4> ();
  test_non_uniform<5> ();
}

BOOST_AUTO_TEST_SUITE_END ()
