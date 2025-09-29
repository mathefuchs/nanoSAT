#include <chrono>
#include <cstdlib>
#include <iostream>

#include "logging.hpp"
#include "parse.hpp"
#include "solver.hpp"

int main(int argc, char** argv) {
  // Start time recording
  auto start_time = std::chrono::high_resolution_clock::now();

  // Check CLI args
  if (argc != 2) {
    std::cout << "Expects `nanosat file.cnf`, `nanosat file.cnf.gz`, or "
                 "`nanosat file.cnf.xz`."
              << std::endl;
    std::exit(EXIT_FAILURE);
  }
  std::string filename(argv[1]);

  // Create solver and parse clauses
  auto solver = ns::parse::parseCnf<ns::solver::Solver>(filename);
  if constexpr (ns::solver::VERBOSE == ns::solver::VerbosityLevel::ALL) {
    auto parse_end_time = std::chrono::high_resolution_clock::now();
    ns::log::printStats(solver, start_time, parse_end_time);
  }
  auto exit_code = solver.solve();

  // End time recording; print elapsed time
  if constexpr (ns::solver::VERBOSE == ns::solver::VerbosityLevel::ALL) {
    auto end_time = std::chrono::high_resolution_clock::now();
    ns::log::printElapsedTime(solver, start_time, end_time);
  }

  // Print model
  ns::log::printModel(solver, exit_code);

  // Return unknown (0), sat (10), or unsat (20)
  return static_cast<int>(exit_code);
}
