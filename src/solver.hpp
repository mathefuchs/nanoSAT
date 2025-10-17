#pragma once

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <format>
#include <iostream>
#include <optional>
#include <random>
#include <utility>
#include <vector>

#include "clauses.hpp"
#include "options.hpp"
#include "restart.hpp"

namespace ns::solver {

/// Verbosity level enum
enum class VerbosityLevel : std::uint8_t {
  ONLY_RESULT = 0,
  ALL = 1,
};
/// Verbosity level
constexpr VerbosityLevel VERBOSE = VerbosityLevel::ALL;

/// Enum representing the solver status exit codes
enum class SolverExitCode : std::uint8_t {
  UNKNOWN = 0,
  SAT = 10,
  UNSAT = 20,
};

/// Solver statistics
struct SolverStatistics {
  /// Number of variables
  std::uint64_t num_variables;
  /// Number of clauses
  std::uint64_t num_clauses;
  /// Number of literals in clauses
  std::uint64_t num_literals_in_clauses;
  /// Number of learned clauses
  std::uint64_t num_learned_clauses;
  /// Number of literals in learned clauses
  std::uint64_t num_literals_in_learned_clauses;
  /// Number of search (re-)starts
  std::uint64_t num_restarts;
  /// Number of made decisions
  std::uint64_t num_decisions;
  /// Number of total conflicts
  std::uint64_t num_total_conflicts;
  /// Number of total propagations
  std::uint64_t num_propagations;

  SolverStatistics()
      : num_variables(0),
        num_clauses(0),
        num_literals_in_clauses(0),
        num_learned_clauses(0),
        num_literals_in_learned_clauses(0),
        num_restarts(0),
        num_decisions(0),
        num_total_conflicts(0),
        num_propagations(0) {}
};

/// SAT Solver object
class Solver {
 private:
  // -- Representation of the SAT problem instance
  /// All clauses
  clauses::Clauses clauses;

  // -- Solver data structures
  /// All learned clauses
  clauses::Clauses learned_clauses;
  /// Stack of all decisions currently made
  std::vector<clauses::Literal> trail;
  /// Where the decision levels in `trail` start
  std::vector<std::uint32_t> trail_separators;
  /// Points to the next literal in `trail` to propagate
  std::uint32_t trail_propagation_head;
  /// Current variable assignments (variables can be unset, false, or true)
  std::vector<clauses::VariableValue> variable_values;
  /// Stores the preferred polarity of a variable (phase saving)
  std::vector<bool> variable_polarity;
  /// Stores metadata for all variables
  std::vector<clauses::VariableMetadata> variable_metadata;
  /// Maintains which clauses watch each literal
  std::vector<std::vector<clauses::Watch>> literals_watched_by;
  /// Unset variables
  std::vector<clauses::Variable> unset_variables;

  // -- Solver state
  /// Amount to change clause activity with
  double clause_activity_increment;
  /// Maximum number of learned clauses allowed
  /// (is dynamically scaled, therefore `double`)
  double max_learned_clauses;
  /// By how much to adjust the learned clauses size on conflict
  double learned_size_adjust_on_conflict;
  /// Specifies after how many conflicts to adjust the learned clauses size
  std::uint64_t learned_size_adjust_count;
  /// Random generator
  std::mt19937 random_gen;
  /// Solver statistics
  SolverStatistics stats;

 public:
  Solver()
      : clauses(),
        learned_clauses(),
        trail(),
        trail_separators(),
        trail_propagation_head(0),
        variable_values(),
        variable_polarity(),
        variable_metadata(),
        literals_watched_by(),
        unset_variables(),
        clause_activity_increment(1.0),
        max_learned_clauses(0.0),
        learned_size_adjust_on_conflict(100.0),
        learned_size_adjust_count(100),
        random_gen(42),
        stats() {}

  /// Number of variables
  constexpr const std::uint32_t numVariables() const noexcept {
    return stats.num_variables;
  }

  /// Number of clauses
  constexpr const std::uint32_t numClauses() const noexcept {
    return stats.num_clauses;
  }

  /// Solver statistics
  constexpr const SolverStatistics& statistics() const noexcept {
    return stats;
  }

  /// Inits all data structures with the specified number of variables
  void createVariables(std::uint32_t num_variables) {
    stats.num_variables = num_variables;
    variable_values.resize(numVariables());
    variable_polarity.resize(numVariables(), false);
    variable_metadata.resize(numVariables(), {{}, 0});
    trail.reserve(numVariables() + 1);
    unset_variables.reserve(numVariables());
    literals_watched_by.resize(numVariables() * 2);
  }

  /// Add clause; return whether clause was added (true)
  /// or conflict occurred (false)
  bool addClause(const std::vector<clauses::Literal>& literals) {
    assert(decisionLevel() == 0);
    assert(!literals.empty());

    // Copy literals and sort (positive and negative literals
    // of the same variable are consecutive)
    auto copied_literals = literals;
    std::sort(copied_literals.begin(), copied_literals.end());

    // Check for satisfied clauses and duplicate literals
    clauses::Literal last_literal;
    std::size_t num_final_elems = 0;
    for (auto curr_literal : copied_literals) {
      auto var = curr_literal.var();
      auto polarity = curr_literal.polarity();
      assert(var < numVariables());

      // Clause already satisfied
      if (variable_values[var] == polarity) {
        return true;
      }
      // `not A or A` is always true
      if (curr_literal == ~last_literal) {
        return true;
      }
      // Literal false; no need to add
      if (variable_values[var] == !polarity) {
        continue;
      }
      // Duplicate literal, continue
      if (curr_literal == last_literal) {
        continue;
      }

      // Add literal to final output
      last_literal = curr_literal;
      copied_literals[num_final_elems] = curr_literal;
      ++num_final_elems;
    }

    // Update clause size
    copied_literals.resize(num_final_elems);

    // If literals are empty, instance is UNSAT
    if (copied_literals.empty()) {
      return false;
    }

    // Add fact for next propagation if singleton
    if (copied_literals.size() == 1) {
      assignLiteral(copied_literals[0], {});
      return !propagate().valid();  // Check conflicts
    }

    // Add clause
    attachClause(std::move(copied_literals), false);
    return true;
  }

  /// Contains the model if SAT
  const std::vector<clauses::VariableValue>& model() const noexcept {
    return variable_values;
  }

  /// Solves the loaded problem instance
  SolverExitCode solve() {
    // Check that clauses are non-empty
    if (numVariables() == 0 || numClauses() == 0) {
      return SolverExitCode::UNKNOWN;
    }

    // Initial simplification
    if (!simplify()) {
      return SolverExitCode::UNSAT;
    }

    // Update maximum learned clauses size
    max_learned_clauses = numClauses() * options::MAX_LEARNED_CLAUSES_FACTOR;

    // Print header for search statistics
    if constexpr (VERBOSE == VerbosityLevel::ALL) {
      std::cout << "============================[ Search Statistics "
                   "]==============================\n"
                << "| Conflicts |          ORIGINAL         |          LEARNED "
                   "        | Progress |\n"
                << "|           |    Vars  Clauses Literals |    Limit  "
                   "Clauses Lit/Cl |          |\n"
                << "==========================================================="
                   "===================="
                << std::endl;
    }

    // Main loop
    stats.num_restarts = 0;
    auto status = SolverExitCode::UNKNOWN;
    while (status == SolverExitCode::UNKNOWN) {
      // Restart search after reaching a certain number of conflicts
      // using the Luby restart sequence
      double restart_base_value =
          restart::luby(options::RESTART_INC, stats.num_restarts);
      status = search(restart_base_value * options::RESTART_FIRST);
      ++stats.num_restarts;
    }

    // Return solver exit status
    return status;
  }

 private:
  /// Used for analyzing conflicts in `analyzeConflict`
  enum class VariableStatus : std::uint8_t {
    /// Variable does not participate in conflict
    UNSET = 0,
    /// Variable is a source of conflict
    IS_SOURCE = 1,
    /// Variable causes a conflict but could be removed
    REMOVABLE = 2,
    /// Removing variable failed
    REMOVAL_FAILED = 3,
  };

  /// Search for a model with the given number of allowed conflicts
  SolverExitCode search(std::uint32_t allowed_num_of_conflicts) {
    // Number of levels to backtrack
    std::uint32_t backtrack_level = 0;
    // Number of conflicts
    std::uint32_t num_conflicts = 0;
    // Currently learned clause
    std::vector<clauses::Literal> learned_clause;

    // Search until finding model or reaching allowed number of conflicts
    while (true) {
      // Propagate currently selected variables
      auto conflict = propagate();

      // Check if conflict found
      if (conflict.valid()) {
        // Found conflict
        ++stats.num_total_conflicts;
        ++num_conflicts;

        // Conflict reached outer-most layer; UNSAT
        if (decisionLevel() == 0) {
          return SolverExitCode::UNSAT;
        }

        // Analyze conflict
        learned_clause.clear();
        backtrack_level = analyzeConflict(conflict, learned_clause);
        revertTrail(backtrack_level);

        if (learned_clause.size() == 1) {
          // Found single-literal reason for conflict, propagate
          assignLiteral(learned_clause[0], {});
        } else {
          // Else, learn clause and propagate first literal
          auto clause_ref = attachClause(learned_clause, true);
          increaseClauseActivity(clause_ref);
          assignLiteral(learned_clause[0], clause_ref);
        }

        // Decay clause activities
        clause_activity_increment *= 1 / options::CLAUSE_ACTIVITY_DECAY;

        // Update maximum number of learned clauses
        --learned_size_adjust_count;
        if (learned_size_adjust_count == 0) {
          learned_size_adjust_on_conflict *=
              options::MAX_LEARNED_ADJUST_INCREMENT;
          learned_size_adjust_count =
              static_cast<std::uint64_t>(learned_size_adjust_on_conflict);
          max_learned_clauses *= options::MAX_LEARNED_CLAUSES_INCREMENT;

          // Log progress
          if constexpr (VERBOSE == VerbosityLevel::ALL) {
            auto free_variables =
                stats.num_variables - (trail_separators.size() == 0
                                           ? trail.size()
                                           : trail_separators[0]);
            auto literals_per_learned =
                static_cast<double>(stats.num_literals_in_learned_clauses) /
                static_cast<double>(stats.num_learned_clauses);
            auto progress_estimate_percent = progressEstimate() * 100.0;
            std::cout << std::format(
                             "| {:9d} | {:7d} {:8d} {:8d} | {:8d} {:8d} "
                             "{:6.0f} | {:6.3f} % |",
                             stats.num_total_conflicts, free_variables,
                             stats.num_clauses, stats.num_literals_in_clauses,
                             static_cast<std::uint64_t>(max_learned_clauses),
                             stats.num_learned_clauses, literals_per_learned,
                             progress_estimate_percent)
                      << std::endl;
          }
        }
      } else {
        // No conflict
        if (num_conflicts >= allowed_num_of_conflicts) {
          // Reached bound on number of conflicts; revert complete trail
          revertTrail(0);
          return SolverExitCode::UNKNOWN;
        }

        // Simplify the set of clauses
        if (decisionLevel() == 0 && !simplify()) {
          return SolverExitCode::UNSAT;
        }

        // Reduce the set of learned clauses if too many
        if (learned_clauses.size() >= max_learned_clauses + trail.size()) {
          pruneLearnedClauses();
        }

        // New variable decision
        ++stats.num_decisions;
        std::optional<clauses::Literal> next_literal = pickBranchLiteral();
        if (!next_literal.has_value()) {
          // Model found if all variables assigned without conflict
          return SolverExitCode::SAT;
        }

        // Increase decision level
        trail_separators.push_back(trail.size());

        // Enqueue next branch literal
        assignLiteral(*next_literal, {});
      }
    }

    // Unreachable
    return SolverExitCode::UNKNOWN;
  }

  /// Analyze the given conflict; returns the backtrack level
  /// and the learned clause
  std::uint32_t analyzeConflict(
      clauses::ClauseRef conflict,
      std::vector<clauses::Literal>& out_learned_clause) {
    // Leave room for the asserting literal
    out_learned_clause.emplace_back();
    std::int64_t index = trail.size() - 1;
    std::int64_t path_length = 0;
    clauses::Literal asserting_literal;
    std::vector<VariableStatus> variable_seen(numVariables(),
                                              VariableStatus::UNSET);

    // Build learned conflict clause
    do {
      assert(conflict.valid());
      auto& conflict_clause = clauseAt(conflict);

      // Increase activity if learned clause
      if (conflict.isLearned()) {
        increaseClauseActivity(conflict);
      }

      auto start = static_cast<std::size_t>(asserting_literal.valid());
      for (std::size_t j = start; j < conflict_clause.size(); ++j) {
        auto conflict_literal = conflict_clause[j];

        // Check unseen variables in clause
        if (variable_seen[conflict_literal.var()] == VariableStatus::UNSET &&
            variable_metadata[conflict_literal.var()].decision_level > 0) {
          variable_seen[conflict_literal.var()] = VariableStatus::IS_SOURCE;

          if (variable_metadata[conflict_literal.var()].decision_level >=
              decisionLevel()) {
            ++path_length;
          } else {
            out_learned_clause.push_back(conflict_literal);
          }
        }
      }

      // Select next clause to look at
      while (variable_seen[trail[index--].var()] == VariableStatus::UNSET);
      asserting_literal = trail[index + 1];
      conflict = variable_metadata[asserting_literal.var()].reason_clause_idx;
      variable_seen[asserting_literal.var()] = VariableStatus::UNSET;
      --path_length;

    } while (path_length > 0);
    out_learned_clause[0] = ~asserting_literal;

    // Simplify conflict clause
    std::size_t i, j;
    for (i = j = 1; i < out_learned_clause.size(); ++i) {
      // Literal needed if it has top-level assignment or is not redundant
      if (!variable_metadata[out_learned_clause[i].var()]
               .reason_clause_idx.valid() ||
          !isLiteralRedundantInConflictClause(variable_seen,
                                              out_learned_clause[i])) {
        out_learned_clause[j] = out_learned_clause[i];
        ++j;
      }
    }
    out_learned_clause.resize(j);

    // Find correct backtrack level
    std::uint32_t out_btlevel = 0;
    if (out_learned_clause.size() != 1) {
      // Find the first literal assigned at the next-highest level
      std::size_t max_i = 1;
      for (std::size_t i = 2; i < out_learned_clause.size(); i++) {
        if (variable_metadata[out_learned_clause[i].var()].decision_level >
            variable_metadata[out_learned_clause[max_i].var()].decision_level) {
          max_i = i;
        }
      }

      // Swap-in this literal at index 1
      auto literal = out_learned_clause[max_i];
      out_learned_clause[max_i] = out_learned_clause[1];
      out_learned_clause[1] = literal;
      out_btlevel = variable_metadata[literal.var()].decision_level;
    }

    return out_btlevel;
  }

  /// Checks whether literal is redundant in the conflict
  bool isLiteralRedundantInConflictClause(
      std::vector<VariableStatus>& variable_seen, clauses::Literal literal) {
    assert(variable_seen[literal.var()] == VariableStatus::UNSET ||
           variable_seen[literal.var()] == VariableStatus::IS_SOURCE);
    assert(variable_metadata[literal.var()].reason_clause_idx.valid());
    auto* clause =
        &clauseAt(variable_metadata[literal.var()].reason_clause_idx);
    std::vector<std::pair<std::uint32_t, clauses::Literal>> stack;

    for (std::uint32_t i = 1;; i++) {
      if (i < clause->size()) {
        // Checking `literal`'s parent `l` (in reason graph)
        auto parent = (*clause)[i];

        // Variable at level 0 or previously removable
        if (variable_metadata[parent.var()].decision_level == 0 ||
            variable_seen[parent.var()] == VariableStatus::IS_SOURCE ||
            variable_seen[parent.var()] == VariableStatus::REMOVABLE) {
          continue;
        }

        // Check variable can not be removed for some local reason
        if (!variable_metadata[parent.var()].reason_clause_idx.valid() ||
            variable_seen[parent.var()] == VariableStatus::REMOVAL_FAILED) {
          stack.emplace_back(0, literal);
          for (std::size_t i = 0; i < stack.size(); ++i) {
            if (variable_seen[stack[i].second.var()] == VariableStatus::UNSET) {
              variable_seen[stack[i].second.var()] =
                  VariableStatus::REMOVAL_FAILED;
            }
          }

          return false;
        }

        // Recursively check `parent`
        stack.emplace_back(i, literal);
        i = 0;
        literal = parent;
        clause = &clauseAt(variable_metadata[literal.var()].reason_clause_idx);
      } else {
        // Finished with current element `literal` and reason `clause`
        if (variable_seen[literal.var()] == VariableStatus::UNSET) {
          variable_seen[literal.var()] = VariableStatus::REMOVABLE;
        }

        // Terminate with success if stack is empty
        if (stack.size() == 0) {
          break;
        }

        // Continue with top element on stack
        i = stack.back().first;
        literal = stack.back().second;
        clause = &clauseAt(variable_metadata[literal.var()].reason_clause_idx);
        stack.pop_back();
      }
    }

    return true;
  }

  /// Prune learned clauses if too many
  void pruneLearnedClauses() {
    // Sort learned clauses by activity
    std::vector<std::uint32_t> learned_clause_ind;
    learned_clause_ind.reserve(learned_clauses.size());
    for (std::uint32_t i = 0; i < learned_clauses.size(); ++i) {
      if (!learned_clauses[{i, true}].empty()) {
        learned_clause_ind.push_back(i);
      }
    }
    std::sort(learned_clause_ind.begin(), learned_clause_ind.end(),
              [this](std::uint32_t i, std::uint32_t j) {
                return learned_clauses.activity({i, true}) <
                       learned_clauses.activity({j, true});
              });

    // Threshold for pruning
    double median = learned_clauses.activity(
        {learned_clause_ind[learned_clause_ind.size() / 2], true});
    double threshold = clause_activity_increment / learned_clauses.size();
    double pruneThresh = std::min(median, threshold);

    for (std::uint32_t i = 0; i < learned_clauses.size(); ++i) {
      clauses::ClauseRef clause_ref(i, true);
      auto& clause = learned_clauses[clause_ref];

      // Skip deleted clauses
      if (clause.empty()) {
        continue;
      }

      // Delete learned clauses if below `threshold` or smaller than median
      // activity; do not delete binary or referenced clauses
      if (clause.size() > 2 &&
          learned_clauses.activity(clause_ref) < pruneThresh &&
          !isLockedClause(clause_ref)) {
        detachClause(clause_ref);
      }
    }
  }

  /// Progress estimate
  double progressEstimate() const {
    double progress = 0.0;
    double base = 1.0 / numVariables();

    for (std::uint32_t i = 0; i <= decisionLevel(); ++i) {
      // Discount progress based on decision level
      std::uint32_t beg = i == 0 ? 0 : trail_separators[i - 1];
      std::uint32_t end =
          i == decisionLevel() ? trail.size() : trail_separators[i];
      progress += pow(base, i) * (end - beg);
    }

    return progress / numVariables();
  }

  /// Pick next literal to branch on
  std::optional<clauses::Literal> pickBranchLiteral() {
    // Random decision
    while (!unset_variables.empty()) {
      // Select random unset variable
      std::uniform_int_distribution<std::size_t> dist(
          0, unset_variables.size() - 1);
      auto idx = dist(random_gen);
      auto var = unset_variables[idx];
      unset_variables[idx] = unset_variables.back();
      unset_variables.pop_back();

      // Check whether variable is unset
      if (variable_values[var].isUnset()) {
        return {{var, variable_polarity[var]}};
      }
    }

    return {};
  }

  /// Accesses an original or learned clause
  std::vector<clauses::Literal>& clauseAt(clauses::ClauseRef clause_ref) {
    if (clause_ref.isLearned()) {
      return learned_clauses[clause_ref];
    }
    return clauses[clause_ref];
  }

  /// Whether literal is satisfied
  bool literalTrue(clauses::Literal literal) const noexcept {
    return variable_values[literal.var()] == literal.polarity();
  }

  /// Whether literal is not satisfied
  bool literalFalse(clauses::Literal literal) const noexcept {
    return variable_values[literal.var()] == !literal.polarity();
  }

  /// Check that clause is not the reason of some propagation
  bool isLockedClause(clauses::ClauseRef clause_ref) {
    auto& clause = clauseAt(clause_ref);
    return literalTrue(clause[0]) &&
           variable_metadata[clause[0].var()].reason_clause_idx.valid() &&
           variable_metadata[clause[0].var()].reason_clause_idx == clause_ref;
  }

  /// Increases the clause activity of a learned clause
  void increaseClauseActivity(clauses::ClauseRef clause_ref) {
    assert(clause_ref.isLearned());
    auto& activity = learned_clauses.activity(clause_ref);
    activity += clause_activity_increment;

    // Rescale if `activity` exceeds `1e20`
    if (activity > 1e20) {
      for (std::uint32_t i = 0; i < learned_clauses.size(); ++i) {
        learned_clauses.activity({i, true}) *= 1e-20;
      }
      clause_activity_increment *= 1e-20;
    }
  }

  /// Propagate all facts in `trail` starting from `trail_propagation_head`;
  /// returns conflicting clause index or `UNDEF_CLAUSE` if none
  clauses::ClauseRef propagate() {
    // Current conflict
    clauses::ClauseRef conflict;

    // Propagates all enqueued facts
    while (trail_propagation_head < trail.size()) {
      // Get literal and watches to propagate
      auto literal_to_propagate = trail[trail_propagation_head];
      ++trail_propagation_head;
      auto& watches = literals_watched_by[literal_to_propagate];
      ++stats.num_propagations;

      // Check all watches
      std::size_t i = 0;
      std::size_t j = 0;
      while (i < watches.size()) {
        // Clause can be skipped if `clauses::Watch::blocker` literal is true
        auto blocker = watches[i].blocker;
        if (literalTrue(blocker)) {
          watches[j] = watches[i];
          ++i;
          ++j;
          continue;
        }

        // Make sure the false literal is at position 2
        auto clause_ref = watches[i].clause_ref;
        auto& clause = clauseAt(clause_ref);
        auto not_literal = ~literal_to_propagate;
        if (clause[0] == not_literal) {
          clause[0] = clause[1];
          clause[1] = not_literal;
        }
        assert(clause[1] == not_literal);
        ++i;

        // If first watch is true, then clause is already satisfied
        auto first_literal = clause[0];
        clauses::Watch new_watch(clause_ref, first_literal);
        if (first_literal != blocker && literalTrue(first_literal)) {
          watches[j] = new_watch;
          ++j;
          continue;
        }

        // Look for new watch that is not already false
        bool found_new_watch = false;
        for (std::size_t k = 2; k < clause.size(); ++k) {
          if (!literalFalse(clause[k])) {
            clause[1] = clause[k];
            clause[k] = not_literal;
            literals_watched_by[~clause[1]].push_back(new_watch);
            found_new_watch = true;
            break;
          }
        }
        if (found_new_watch) {
          continue;
        }

        // Did not find new watch; clause must be unit
        watches[j] = new_watch;
        ++j;
        if (literalFalse(first_literal)) {
          // Found conflict
          conflict = clause_ref;
          trail_propagation_head = trail.size();
          while (i < watches.size()) {
            watches[j] = watches[i];
            ++i;
            ++j;
          }
        } else {
          // Found fact
          assignLiteral(first_literal, clause_ref);
        }
      }

      // Resize `watches`
      watches.resize(j);
    }

    // Return current conflict
    return conflict;
  }

  /// Returns the current decision level
  constexpr std::uint32_t decisionLevel() const noexcept {
    return trail_separators.size();
  }

  /// Reverts the assignment trail until the given decision level
  void revertTrail(std::uint32_t level) {
    // Reverting to `level` only necessary if current level higher
    if (decisionLevel() > level) {
      for (std::int64_t c = trail.size() - 1; c >= trail_separators[level];
           --c) {
        // Literal to revert
        auto literal_to_revert = trail[c];
        auto variable = literal_to_revert.var();
        bool polarity = literal_to_revert.polarity();

        // Unset assignment and save preferred polarity
        variable_values[variable] = {};
        variable_polarity[variable] = polarity;
        unset_variables.push_back(variable);
      }

      // Shrink `trail` and `trail_separators` to specified `level`
      trail_propagation_head = trail_separators[level];
      trail.resize(trail_propagation_head);
      trail_separators.resize(level);
    }
  }

  /// Assigns the given literal (must be unset previously)
  void assignLiteral(clauses::Literal literal,
                     clauses::ClauseRef reason_clause_idx) {
    // Assigned literal must be unset previously
    auto var = literal.var();
    assert(variable_values[var].isUnset());

    // Assign literal
    variable_values[var] = literal.polarity();
    variable_metadata[var].decision_level = decisionLevel();
    variable_metadata[var].reason_clause_idx = reason_clause_idx;
    trail.push_back(literal);
  }

  /// Removes a watch from `literals_watched_by`
  void removeWatch(std::vector<clauses::Watch>& watches,
                   clauses::Watch watch_to_remove) {
    // Find watch
    std::size_t i = 0;
    while (i < watches.size() && watches[i] != watch_to_remove) {
      ++i;
    }

    // Assert that watch found
    assert(i < watches.size());

    // Move remaining watches and pop back
    while (i < watches.size() - 1) {
      watches[i] = watches[i + 1];
      ++i;
    }
    watches.pop_back();
  }

  /// Attaches a clause by creating watches
  clauses::ClauseRef attachClause(std::vector<clauses::Literal> literals,
                                  bool is_learned) {
    // Add clause
    auto first_literal = literals[0];
    auto second_literal = literals[1];
    clauses::ClauseRef clause_ref;
    if (is_learned) {
      ++stats.num_learned_clauses;
      stats.num_literals_in_learned_clauses += literals.size();
      clause_ref = learned_clauses.addClause(std::move(literals), true);
    } else {
      ++stats.num_clauses;
      stats.num_literals_in_clauses += literals.size();
      clause_ref = clauses.addClause(std::move(literals), false);
    }

    // Keep two watches per clause
    literals_watched_by[~first_literal].emplace_back(clause_ref,
                                                     second_literal);
    literals_watched_by[~second_literal].emplace_back(clause_ref,
                                                      first_literal);
    return clause_ref;
  }

  /// Removes a clause by removing watches and clearing literals
  void detachClause(clauses::ClauseRef clause_ref) {
    auto& clause = clauseAt(clause_ref);
    removeWatch(literals_watched_by[~clause[0]], {clause_ref, clause[1]});
    removeWatch(literals_watched_by[~clause[1]], {clause_ref, clause[0]});
    if (isLockedClause(clause_ref)) {
      variable_metadata[clause.front().var()].reason_clause_idx = {};
    }

    if (clause_ref.isLearned()) {
      --stats.num_learned_clauses;
      stats.num_literals_in_learned_clauses -= clause.size();
      learned_clauses.removeClause(clause_ref);
    } else {
      --stats.num_clauses;
      stats.num_literals_in_clauses -= clause.size();
      clauses.removeClause(clause_ref);
    }
  }

  /// Remove the satisfied clauses in the given container
  void removeSatisfiedClauses(clauses::Clauses& clauses_to_check,
                              bool is_learned) {
    for (std::uint32_t clause_idx = 0; clause_idx < clauses_to_check.size();
         ++clause_idx) {
      clauses::ClauseRef clause_ref(clause_idx, is_learned);
      auto& clause = clauses_to_check[clause_ref];

      // Skip deleted clauses
      if (clause.empty()) {
        continue;
      }

      if (isClauseSatisfied(clause)) {
        // Remove clause
        detachClause(clause_ref);
      } else {
        // Trim clause; first two literals cannot be true since otherwise
        // `isClauseSatisfied()` and cannot be false by invariant
        assert(clause.size() > 1);
        assert(variable_values[clause[0].var()].isUnset());
        assert(variable_values[clause[1].var()].isUnset());
        for (std::size_t i = 2; i < clause.size(); ++i) {
          if (variable_values[clause[i].var()].isFalse()) {
            clause[i] = clause.back();
            clause.pop_back();
            --i;
          }
        }
      }
    }
  }

  /// Simplify by removing satisfied clauses
  bool simplify() {
    // Only top-level simplifications
    assert(decisionLevel() == 0);

    // Check that top-level propagation does not produce a conflict
    if (propagate().valid()) {
      return false;
    }

    // Remove satisfied clauses
    removeSatisfiedClauses(learned_clauses, true);
    removeSatisfiedClauses(clauses, false);

    // Update unset variables
    unset_variables.clear();
    for (clauses::Variable var = 0; var < variable_values.size(); ++var) {
      if (variable_values[var].isUnset()) {
        unset_variables.push_back(var);
      }
    }
    std::shuffle(unset_variables.begin(), unset_variables.end(), random_gen);

    // Problem instance still satisfiable
    return true;
  }

  /// Checks whether the given clause is satisfied
  bool isClauseSatisfied(const std::vector<clauses::Literal>& clause) const {
    for (auto literal : clause) {
      if (literalTrue(literal)) {
        return true;
      }
    }
    return false;
  }
};

}  // namespace ns::solver
