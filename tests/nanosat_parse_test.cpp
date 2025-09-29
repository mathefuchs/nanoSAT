#include <gtest/gtest.h>

#include <cstdint>
#include <vector>

#include "clauses.hpp"
#include "parse.hpp"

namespace nanosat_test {

namespace {
/// Mock for solver type
struct SolverMock {
  std::uint32_t num_variables;
  std::uint32_t num_clauses;
  std::vector<ns::clauses::Literal> last_clause;

  SolverMock() : num_variables(0), num_clauses(0), last_clause() {}

  void createVariables(std::uint32_t num_variables) {
    this->num_variables = num_variables;
  }
  bool addClause(const std::vector<ns::clauses::Literal>& clause) {
    last_clause = clause;
    ++num_clauses;
    return true;
  }
};
}  // namespace

TEST(nanosat_test_suite, test_parse_cnf) {
  auto solver =
      ns::parse::parseCnf<SolverMock>("tests/examples/success/medium_sat.cnf");
  ASSERT_EQ(solver.num_variables, 403);
  ASSERT_EQ(solver.num_clauses, 2029);
  ASSERT_EQ(solver.last_clause,
            (std::vector<ns::clauses::Literal>{{402, false}, {22, true}}));
}

TEST(nanosat_test_suite, test_parse_cnf_file_not_existing) {
  ASSERT_DEATH(
      { ns::parse::parseCnf<SolverMock>("file_not_existing.cnf"); },
      "Failed to open file \"file_not_existing.cnf\" using plain text mode\\.");
}

TEST(nanosat_test_suite, test_parse_cnf_xz) {
  auto solver = ns::parse::parseCnf<SolverMock>(
      "tests/examples/success/medium_sat.cnf.xz");
  ASSERT_EQ(solver.num_variables, 403);
  ASSERT_EQ(solver.num_clauses, 2029);
  ASSERT_EQ(solver.last_clause,
            (std::vector<ns::clauses::Literal>{{402, false}, {22, true}}));
}

TEST(nanosat_test_suite, test_parse_cnf_xz_file_not_existing) {
  ASSERT_DEATH(
      { ns::parse::parseCnf<SolverMock>("file_not_existing.cnf.xz"); },
      "Failed to read from file or pipe\\.");
}

TEST(nanosat_test_suite, test_parse_cnf_gz) {
  auto solver = ns::parse::parseCnf<SolverMock>(
      "tests/examples/success/medium_sat.cnf.gz");
  ASSERT_EQ(solver.num_variables, 403);
  ASSERT_EQ(solver.num_clauses, 2029);
  ASSERT_EQ(solver.last_clause,
            (std::vector<ns::clauses::Literal>{{402, false}, {22, true}}));
}

TEST(nanosat_test_suite, test_parse_cnf_gz_file_not_existing) {
  ASSERT_DEATH(
      { ns::parse::parseCnf<SolverMock>("file_not_existing.cnf.gz"); },
      "Failed to read from file or pipe\\.");
}

TEST(nanosat_test_suite, test_parse_cnf_missing_clause) {
  ASSERT_DEATH(
      {
        ns::parse::parseCnf<SolverMock>(
            "tests/examples/fail/missing_clause.cnf");
      },
      "Number of clauses in cnf incorrect\\.");
}

TEST(nanosat_test_suite, test_parse_cnf_too_many_vars) {
  ASSERT_DEATH(
      {
        ns::parse::parseCnf<SolverMock>(
            "tests/examples/fail/too_many_vars.cnf");
      },
      "Number of variables in cnf incorrect\\.");
}

TEST(nanosat_test_suite, test_parse_cnf_missing_zero) {
  ASSERT_DEATH(
      {
        ns::parse::parseCnf<SolverMock>("tests/examples/fail/missing_zero.cnf");
      },
      "Failed to parse cnf file\\.");
}

TEST(nanosat_test_suite, test_parse_cnf_too_many_spaces) {
  ASSERT_DEATH(
      {
        ns::parse::parseCnf<SolverMock>(
            "tests/examples/fail/too_many_spaces.cnf");
      },
      "Failed to parse cnf file\\.");
}

TEST(nanosat_test_suite, test_parse_cnf_double_minus) {
  ASSERT_DEATH(
      {
        ns::parse::parseCnf<SolverMock>("tests/examples/fail/double_minus.cnf");
      },
      "Failed to parse cnf file\\.");
}

TEST(nanosat_test_suite, test_parse_cnf_empty_clause) {
  ASSERT_DEATH(
      {
        ns::parse::parseCnf<SolverMock>("tests/examples/fail/empty_clause.cnf");
      },
      "Failed to parse cnf file\\.");
}

TEST(nanosat_test_suite, test_parse_cnf_leading_zero) {
  ASSERT_DEATH(
      {
        ns::parse::parseCnf<SolverMock>("tests/examples/fail/leading_zero.cnf");
      },
      "Failed to parse cnf file\\.");
}

TEST(nanosat_test_suite, test_parse_cnf_unknown_line) {
  ASSERT_DEATH(
      {
        ns::parse::parseCnf<SolverMock>("tests/examples/fail/unknown_line.cnf");
      },
      "Failed to parse cnf file\\.");
}

}  // namespace nanosat_test
