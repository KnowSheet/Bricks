/*******************************************************************************
The MIT License (MIT)

Copyright (c) 2015 Dmitry "Dima" Korolev <dmitry.korolev@gmail.com>
          (c) 2015 Maxim Zhurovich <zhurovich@gmail.com>

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*******************************************************************************/

#ifndef FNCAS_OPTIMIZE_H
#define FNCAS_OPTIMIZE_H

#include <algorithm>
#include <numeric>
#include <map>
#include <string>

#include "base.h"
#include "differentiate.h"
#include "exceptions.h"
#include "logger.h"
#include "mathutil.h"
#include "node.h"

#include "../../Bricks/template/decay.h"
#include "../../TypeSystem/struct.h"
#include "../../TypeSystem/optional.h"
#include "../../TypeSystem/helpers.h"

namespace fncas {

// clang-format off
CURRENT_STRUCT(OptimizationResult, ValueAndPoint) {
  CURRENT_CONSTRUCTOR(OptimizationResult)(const ValueAndPoint& p) : SUPER(p) {}
};
// clang-format on

class OptimizerParameters {
 public:
  template <typename T>
  void SetValue(std::string name, T value);
  template <typename T>
  const T GetValue(std::string name, T default_value) const;

 private:
  std::map<std::string, double> params_;
};

template <typename T>
void OptimizerParameters::SetValue(std::string name, T value) {
  static_assert(std::is_arithmetic<T>::value, "Value must be numeric");
  params_[name] = value;
}

template <typename T>
const T OptimizerParameters::GetValue(std::string name, T default_value) const {
  static_assert(std::is_arithmetic<T>::value, "Value must be numeric");
  if (params_.count(name)) {
    return static_cast<T>(params_.at(name));
  } else {
    return default_value;
  }
}

template <class F>
class Optimizer : noncopyable {
 public:
  virtual ~Optimizer() = default;

  Optimizer() : f_instance_(std::make_unique<F>()), f_reference_(*f_instance_) {}
  Optimizer(const OptimizerParameters& parameters)
      : f_instance_(std::make_unique<F>()), f_reference_(*f_instance_), parameters_(parameters) {}

  Optimizer(F& f) : f_reference_(f) {}
  Optimizer(const OptimizerParameters& parameters, F& f) : f_reference_(f), parameters_(parameters) {}

  template <typename ARG,
            class = std::enable_if_t<!std::is_same<current::decay<ARG>, OptimizerParameters>::value>,
            typename... ARGS>
  Optimizer(ARG&& arg, ARGS&&... args)
      : f_instance_(std::make_unique<F>(std::forward<ARG>(arg), std::forward<ARGS>(args)...)),
        f_reference_(*f_instance_) {}

  template <typename ARG, typename... ARGS>
  Optimizer(const OptimizerParameters& parameters, ARG&& arg, ARGS&&... args)
      : f_instance_(std::make_unique<F>(std::forward<ARG>(arg), std::forward<ARGS>(args)...)),
        f_reference_(*f_instance_),
        parameters_(parameters) {}

  F& Function() { return f_reference_; }
  const F& Function() const { return f_reference_; }
  F* operator->() { return &f_reference_; }
  const F* operator->() const { return &f_reference_; }

  const Optional<OptimizerParameters>& Parameters() const { return parameters_; }

  virtual OptimizationResult Optimize(const std::vector<double>& starting_point) const = 0;

 private:
  std::unique_ptr<F> f_instance_;             // The function to optimize: instance if owned by the optimizer.
  F& f_reference_;                            // The function to optimize: reference to work with, owned or not owned.
  Optional<OptimizerParameters> parameters_;  // Optimization parameters.
};

// Naive gradient descent that tries 3 different step sizes in each iteration.
// Searches for a local minimum of `F::ObjectiveFunction` function.
template <class F>
class GradientDescentOptimizer final : public Optimizer<F> {
 public:
  using super_t = Optimizer<F>;
  using super_t::super_t;

  OptimizationResult Optimize(const std::vector<double>& starting_point) const override {
    size_t max_steps = 5000;                           // Maximum number of optimization steps.
    double step_factor = 1.0;                          // Gradient is multiplied by this factor.
    double min_absolute_per_step_improvement = 1e-25;  // Terminate early if the absolute improvement is less than this.
    double min_relative_per_step_improvement = 1e-25;  // Terminate early if the relative improvement is less than this.
    double no_improvement_steps_to_terminate = 2;      // Wait for this number of consecutive no improvement iterations.

    if (Exists(super_t::Parameters())) {
      const auto& parameters = Value(super_t::Parameters());
      max_steps = parameters.GetValue("max_steps", max_steps);
      step_factor = parameters.GetValue("step_factor", step_factor);
      min_relative_per_step_improvement =
          parameters.GetValue("min_relative_per_step_improvement", min_relative_per_step_improvement);
      min_absolute_per_step_improvement =
          parameters.GetValue("min_absolute_per_step_improvement", min_absolute_per_step_improvement);
      no_improvement_steps_to_terminate =
          parameters.GetValue("no_improvement_steps_to_terminate", no_improvement_steps_to_terminate);
    }

    OptimizerLogger().Log("GradientDescentOptimizer: Begin at " + JSON(starting_point));

    const size_t dim = starting_point.size();
    const fncas::X gradient_helper(dim);
    const fncas::f_intermediate fi(super_t::Function().ObjectiveFunction(gradient_helper));
    const double starting_value = fi(starting_point);
    OptimizerLogger().Log("GradientDescentOptimizer: Original objective function = " +
                          current::ToString(starting_value));
    const fncas::g_intermediate gi(gradient_helper, fi);
    ValueAndPoint current(starting_value, starting_point);

    int no_improvement_steps = 0;
    for (size_t iteration = 0; iteration < max_steps; ++iteration) {
      OptimizerLogger().Log("GradientDescentOptimizer: Iteration " + current::ToString(iteration + 1) + ", OF = " +
                            current::ToString(current.value) + " @ " + JSON(current.point));
      const auto g = gi(current.point);
      auto best_candidate = current;
      auto has_valid_candidate = false;
      for (const double step : {0.01, 0.05, 0.2}) {
        const auto candidate_point(SumVectors(current.point, g, -step));
        const double value = fi(candidate_point);
        if (fncas::IsNormal(value)) {
          has_valid_candidate = true;
          OptimizerLogger().Log("GradientDescentOptimizer: Value " + current::ToString(value) + " at step " +
                                current::ToString(step));
          best_candidate = std::min(best_candidate, ValueAndPoint(value, candidate_point));
        }
      }
      if (!has_valid_candidate) {
        CURRENT_THROW(FnCASOptimizationException("!fncas::IsNormal(value)"));
      }
      if (best_candidate.value / current.value > 1.0 - min_relative_per_step_improvement ||
          current.value - best_candidate.value < min_absolute_per_step_improvement) {
        ++no_improvement_steps;
        if (no_improvement_steps >= no_improvement_steps_to_terminate) {
          OptimizerLogger().Log("GradientDescentOptimizer: Terminating due to no improvement.");
          break;
        }
      } else {
        no_improvement_steps = 0;
      }
      current = best_candidate;
    }

    OptimizerLogger().Log("GradientDescentOptimizer: Result = " + JSON(current.point));
    OptimizerLogger().Log("GradientDescentOptimizer: Objective function = " + current::ToString(current.value));

    return current;
  }
};

// Simple gradient descent optimizer with backtracking algorithm.
// Searches for a local minimum of `F::ObjectiveFunction` function.
template <class F>
class GradientDescentOptimizerBT final : public Optimizer<F> {
 public:
  using super_t = Optimizer<F>;
  using super_t::super_t;

  OptimizationResult Optimize(const std::vector<double>& starting_point) const override {
    size_t min_steps = 3;       // Minimum number of optimization steps (ignoring early stopping).
    size_t max_steps = 5000;    // Maximum number of optimization steps.
    double bt_alpha = 0.5;      // Alpha parameter for backtracking algorithm.
    double bt_beta = 0.8;       // Beta parameter for backtracking algorithm.
    size_t bt_max_steps = 100;  // Maximum number of backtracking steps.
    double grad_eps = 1e-8;     // Magnitude of gradient for early stopping.
    double min_absolute_per_step_improvement = 1e-25;  // Terminate early if the absolute improvement is less than this.
    double min_relative_per_step_improvement = 1e-25;  // Terminate early if the relative improvement is less than this.
    double no_improvement_steps_to_terminate = 2;      // Wait for this number of consecutive no improvement iterations.

    if (Exists(super_t::Parameters())) {
      const auto& parameters = Value(super_t::Parameters());
      min_steps = parameters.GetValue("min_steps", min_steps);
      max_steps = parameters.GetValue("max_steps", max_steps);
      bt_alpha = parameters.GetValue("bt_alpha", bt_alpha);
      bt_beta = parameters.GetValue("bt_beta", bt_beta);
      bt_max_steps = parameters.GetValue("bt_max_steps", bt_max_steps);
      grad_eps = parameters.GetValue("grad_eps", grad_eps);
      min_relative_per_step_improvement =
          parameters.GetValue("min_relative_per_step_improvement", min_relative_per_step_improvement);
      min_absolute_per_step_improvement =
          parameters.GetValue("min_absolute_per_step_improvement", min_absolute_per_step_improvement);
      no_improvement_steps_to_terminate =
          parameters.GetValue("no_improvement_steps_to_terminate", no_improvement_steps_to_terminate);
    }

    const size_t dim = starting_point.size();
    const fncas::X gradient_helper(dim);
    const fncas::f_intermediate fi(super_t::Function().ObjectiveFunction(gradient_helper));
    const fncas::g_intermediate gi(gradient_helper, fi);
    ValueAndPoint current(ValueAndPoint(fi(starting_point), starting_point));

    OptimizerLogger().Log("GradientDescentOptimizerBT: Begin at " + JSON(starting_point));

    int no_improvement_steps = 0;
    for (size_t iteration = 0; iteration < max_steps; ++iteration) {
      OptimizerLogger().Log("GradientDescentOptimizerBT: Iteration " + current::ToString(iteration + 1) + ", OF = " +
                            current::ToString(current.value) + " @ " + JSON(current.point));
      auto direction = gi(current.point);
      // Simple early stopping by the norm of the gradient.
      if (std::sqrt(fncas::L2Norm(direction)) < grad_eps && iteration >= min_steps) {
        OptimizerLogger().Log("GradientDescentOptimizerBT: Terminating due to small gradient norm.");
        break;
      }

      fncas::FlipSign(direction);  // Going against the gradient to minimize the function.
      const auto next = Backtracking(fi, gi, current.point, direction, bt_alpha, bt_beta, bt_max_steps);

      if (next.value / current.value > 1.0 - min_relative_per_step_improvement ||
          current.value - next.value < min_absolute_per_step_improvement) {
        ++no_improvement_steps;
        if (no_improvement_steps >= no_improvement_steps_to_terminate) {
          OptimizerLogger().Log("GradientDescentOptimizerBT: Terminating due to no improvement.");
          break;
        }
      } else {
        no_improvement_steps = 0;
      }

      current = next;
    }

    OptimizerLogger().Log("GradientDescentOptimizerBT: Result = " + JSON(current.point));
    OptimizerLogger().Log("GradientDescentOptimizerBT: Objective function = " + current::ToString(current.value));

    return current;
  }
};

// Optimizer that uses a combination of conjugate gradient method and
// backtracking line search to find a local minimum of `F::ObjectiveFunction` function.
template <class F>
class ConjugateGradientOptimizer final : public Optimizer<F> {
 public:
  using super_t = Optimizer<F>;
  using super_t::super_t;

  OptimizationResult Optimize(const std::vector<double>& starting_point) const override {
    size_t min_steps = 3;       // Minimum number of optimization steps (ignoring early stopping).
    size_t max_steps = 5000;    // Maximum number of optimization steps.
    double bt_alpha = 0.5;      // Alpha parameter for backtracking algorithm.
    double bt_beta = 0.8;       // Beta parameter for backtracking algorithm.
    size_t bt_max_steps = 100;  // Maximum number of backtracking steps.
    double grad_eps = 1e-8;     // Magnitude of gradient for early stopping.
    double min_absolute_per_step_improvement = 1e-25;  // Terminate early if the absolute improvement is less than this.
    double min_relative_per_step_improvement = 1e-25;  // Terminate early if the relative improvement is less than this.
    double no_improvement_steps_to_terminate = 2;      // Wait for this number of consecutive no improvement iterations.

    if (Exists(super_t::Parameters())) {
      const auto& parameters = Value(super_t::Parameters());
      min_steps = parameters.GetValue("min_steps", min_steps);
      max_steps = parameters.GetValue("max_steps", max_steps);
      bt_alpha = parameters.GetValue("bt_alpha", bt_alpha);
      bt_beta = parameters.GetValue("bt_beta", bt_beta);
      bt_max_steps = parameters.GetValue("bt_max_steps", bt_max_steps);
      grad_eps = parameters.GetValue("grad_eps", grad_eps);
      min_relative_per_step_improvement =
          parameters.GetValue("min_relative_per_step_improvement", min_relative_per_step_improvement);
      min_absolute_per_step_improvement =
          parameters.GetValue("min_absolute_per_step_improvement", min_absolute_per_step_improvement);
      no_improvement_steps_to_terminate =
          parameters.GetValue("no_improvement_steps_to_terminate", no_improvement_steps_to_terminate);
    }

    // TODO(mzhurovich): Implement a more sophisticated version.
    const size_t dim = starting_point.size();
    const fncas::X gradient_helper(dim);
    const fncas::f_intermediate fi(super_t::Function().ObjectiveFunction(gradient_helper));
    const fncas::g_intermediate gi(gradient_helper, fi);
    ValueAndPoint current(fi(starting_point), starting_point);

    std::vector<double> current_gradient = gi(current.point);
    std::vector<double> s(current_gradient);  // Direction to search for a minimum.
    fncas::FlipSign(s);                       // Trying first step against the gradient to minimize the function.

    OptimizerLogger().Log("ConjugateGradientOptimizer: Begin at " + JSON(starting_point));
    int no_improvement_steps = 0;
    for (size_t iteration = 0; iteration < max_steps; ++iteration) {
      OptimizerLogger().Log("ConjugateGradientOptimizer: Iteration " + current::ToString(iteration + 1) + ", OF = " +
                            current::ToString(current.value) + " @ " + JSON(current.point));
      // Backtracking line search.
      const auto next = fncas::Backtracking(fi, gi, current.point, s, bt_alpha, bt_beta, bt_max_steps);
      const auto new_gradient = gi(next.point);

      // Calculating direction for the next step.
      const double omega = std::max(fncas::PolakRibiere(new_gradient, current_gradient), 0.0);
      s = SumVectors(s, new_gradient, omega, -1.0);

      if (next.value / current.value > 1.0 - min_relative_per_step_improvement ||
          current.value - next.value < min_absolute_per_step_improvement) {
        ++no_improvement_steps;
        if (no_improvement_steps >= no_improvement_steps_to_terminate) {
          OptimizerLogger().Log("ConjugateGradientOptimizer: Terminating due to no improvement.");
          break;
        }
      } else {
        no_improvement_steps = 0;
      }

      current = next;
      current_gradient = new_gradient;

      // Simple early stopping by the norm of the gradient.
      if (std::sqrt(L2Norm(s)) < grad_eps && iteration >= min_steps) {
        break;
      }
    }
    OptimizerLogger().Log("ConjugateGradientOptimizer: Result = " + JSON(current.point));
    OptimizerLogger().Log("ConjugateGradientOptimizer: Objective function = " + current::ToString(current.value));

    return current;
  }
};

}  // namespace fncas

#endif  // #ifndef FNCAS_OPTIMIZE_H
