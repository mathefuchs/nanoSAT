#include <gtest/gtest.h>

#include "parse.hpp"
#include "solver.hpp"

namespace nanosat_test {

namespace {
/// Mock for solver type
struct SolverMock {
  std::uint32_t num_variables;
  std::uint32_t num_clauses;
  std::vector<std::vector<ns::clauses::Literal>> clauses;

  SolverMock() : num_variables(0), num_clauses(0), clauses() {}

  void createVariables(std::uint32_t num_variables) {
    this->num_variables = num_variables;
  }
  bool addClause(const std::vector<ns::clauses::Literal>& clause) {
    clauses.push_back(clause);
    ++num_clauses;
    return true;
  }
};
}  // namespace

TEST(nanosat_test_suite, test_small_sat_instance) {
  auto solver = ns::parse::parseCnf<ns::solver::Solver>(
      "tests/examples/success/small_sat.cnf");
  auto mock_solver =
      ns::parse::parseCnf<SolverMock>("tests/examples/success/small_sat.cnf");

  // Check SAT model
  auto res = solver.solve();
  ASSERT_EQ(res, ns::solver::SolverExitCode::SAT);
  for (auto& clause : mock_solver.clauses) {
    bool contains_true_literal = false;
    for (auto lit : clause) {
      auto val = solver.model()[lit.var()];
      // Unset variables are true by default
      if (val == lit.polarity()) {
        contains_true_literal = true;
        break;
      }
    }
    ASSERT_TRUE(contains_true_literal);
  }
}

TEST(nanosat_test_suite, test_medium_sat_instance) {
  auto solver = ns::parse::parseCnf<ns::solver::Solver>(
      "tests/examples/success/medium_sat.cnf");
  auto mock_solver =
      ns::parse::parseCnf<SolverMock>("tests/examples/success/medium_sat.cnf");

  // Check SAT model
  auto res = solver.solve();
  ASSERT_EQ(res, ns::solver::SolverExitCode::SAT);
  for (auto& clause : mock_solver.clauses) {
    bool contains_true_literal = false;
    for (auto lit : clause) {
      auto val = solver.model()[lit.var()];
      // Unset variables are true by default
      if (val == lit.polarity()) {
        contains_true_literal = true;
        break;
      }
    }
    ASSERT_TRUE(contains_true_literal);
  }
}

TEST(nanosat_test_suite, test_big_sat_instance) {
  auto solver = ns::parse::parseCnf<ns::solver::Solver>(
      "tests/examples/success/big_sat_instance.cnf.xz");
  auto mock_solver = ns::parse::parseCnf<SolverMock>(
      "tests/examples/success/big_sat_instance.cnf.xz");

  // Check SAT model
  auto res = solver.solve();
  ASSERT_EQ(res, ns::solver::SolverExitCode::SAT);
  for (auto& clause : mock_solver.clauses) {
    bool contains_true_literal = false;
    for (auto lit : clause) {
      auto val = solver.model()[lit.var()];
      // Unset variables are true by default
      if (val == lit.polarity()) {
        contains_true_literal = true;
        break;
      }
    }
    ASSERT_TRUE(contains_true_literal);
  }
}

}  // namespace nanosat_test
