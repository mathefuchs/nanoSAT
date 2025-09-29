#pragma once

#include <cassert>
#include <chrono>
#include <format>
#include <iostream>

#include "clauses.hpp"
#include "solver.hpp"

namespace ns::log {

/// Time point type
using TimePoint = std::chrono::high_resolution_clock::time_point;

/// Print basic problem instance statistics
inline void printStats(const solver::Solver& solver, TimePoint start_time,
                       TimePoint parse_end_time) {
  auto elapsed_time = std::chrono::duration_cast<std::chrono::microseconds>(
                          parse_end_time - start_time)
                          .count() /
                      1e6;
  std::cout << "\n============================[ Problem Statistics "
               "]=============================\n"
            << "|                                                              "
               "               |\n"
            << std::format(
                   "|  Number of variables:  {:>12}                            "
                   "             |\n",
                   solver.numVariables())
            << std::format(
                   "|  Number of clauses:    {:>12}                            "
                   "             |\n",
                   solver.numClauses())
            << std::format(
                   "|  Parse time:           {:>12.6f}                         "
                   "                |\n",
                   elapsed_time)
            << "|                                                              "
               "               |"
            << std::endl;
}

/// Print elapsed time to the command line
inline void printElapsedTime(const solver::Solver& solver, TimePoint start_time,
                             TimePoint end_time) {
  auto elapsed_time = std::chrono::duration_cast<std::chrono::microseconds>(
                          end_time - start_time)
                          .count() /
                      1e6;
  std::cout << "============================[      Summary      "
               "]==============================\n"
            << "|                                                              "
               "               |\n"
            << std::format(
                   "|  #Restarts:            {:>12}                         "
                   "                |\n",
                   solver.statistics().num_restarts)
            << std::format(
                   "|  #Conflicts:           {:>12} ({:>12.3f}/sec)      "
                   "                |\n",
                   solver.statistics().num_total_conflicts,
                   solver.statistics().num_total_conflicts / elapsed_time)
            << std::format(
                   "|  #Decisions:           {:>12}                         "
                   "                |\n",
                   solver.statistics().num_decisions)
            << std::format(
                   "|  #Propagations:        {:>12} ({:>12.3f}/sec)      "
                   "                |\n",
                   solver.statistics().num_propagations,
                   solver.statistics().num_propagations / elapsed_time)
            << std::format(
                   "|  Total time:           {:>12.6f}                         "
                   "                |\n",
                   elapsed_time)
            << "|                                                              "
               "               |\n"
            << "==============================================================="
               "================\n"
            << std::endl;
}

/// Print model
inline void printModel(const solver::Solver& solver,
                       solver::SolverExitCode exit_code) {
  switch (exit_code) {
    // Unknown
    case solver::SolverExitCode::UNKNOWN:
      std::cout << "UNKNOWN" << std::endl;
      break;

    // SAT
    case solver::SolverExitCode::SAT:
      std::cout << "SAT";
      for (clauses::Variable var = 0; var < solver.model().size(); ++var) {
        auto val = solver.model()[var];
        assert(val.isTrue() || val.isFalse());
        if (val.isTrue()) {
          std::cout << " " << (var + 1);
        } else {
          std::cout << " -" << (var + 1);
        }
      }
      std::cout << std::endl;
      break;

    // UNSAT
    case solver::SolverExitCode::UNSAT:
      std::cout << "UNSAT" << std::endl;
      break;
  }
}

}  // namespace ns::log
