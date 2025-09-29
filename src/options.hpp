#pragma once

namespace ns::options {

/// Variable activity decay
constexpr double VARIABLE_ACTIVITY_DECAY = 0.95;
/// Clause activity decay
constexpr double CLAUSE_ACTIVITY_DECAY = 0.999;
/// Fraction of learned clauses compared to original clauses
constexpr double MAX_LEARNED_CLAUSES_FACTOR = 1.0 / 3.0;
/// Increment of the maximum number of learned clauses
constexpr double MAX_LEARNED_CLAUSES_INCREMENT = 1.1;
/// After how many conflicts to adjust the
/// maximum number of learned clauses again
constexpr double MAX_LEARNED_ADJUST_INCREMENT = 1.5;
/// The base restart interval
constexpr int RESTART_FIRST = 100;
/// The restart interval increase factor
constexpr double RESTART_INC = 2.0;

}  // namespace ns::options
