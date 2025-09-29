#pragma once

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace ns::clauses {

/// Variable type
class VariableValue {
 public:
  /// Unset variable
  constexpr VariableValue() : value(ValueEnum::UNSET) {}
  /// Variable from bool
  constexpr VariableValue(bool sign) : value(static_cast<ValueEnum>(sign)) {}

  /// Check whether is true
  constexpr bool isTrue() const noexcept { return value == ValueEnum::TRUE; }
  /// Check whether is false
  constexpr bool isFalse() const noexcept { return value == ValueEnum::FALSE; }
  /// Check whether is unset
  constexpr bool isUnset() const noexcept { return value == ValueEnum::UNSET; }

  /// Equality
  constexpr bool operator==(VariableValue var) const noexcept {
    return value == var.value;
  }

 private:
  /// The truth value of a variable is either unset, true, or false
  enum class ValueEnum : std::uint8_t {
    FALSE = 0,
    TRUE = 1,
    UNSET = 2,
  };

  /// Variable value
  ValueEnum value;
};

/// Variable type
using Variable = std::uint32_t;

/// Literal type
class Literal {
 private:
  /// Literal representation; positive and negative literals are consecutive;
  /// `[    0, 1,     2, 3,     4, 5, ...]`
  /// `[not 0, 0, not 1, 1, not 2, 2, ...]`
  Variable x;
  /// Invalid literal
  static constexpr Variable INVALID = -1;

  /// Default constructors private
  constexpr explicit Literal(Variable x) : x(x) {}

 public:
  /// Default constructor creates invalid literal
  constexpr Literal() : x(INVALID) {}
  /// Constructs a literal from a variable (0..n)
  /// with polarity (+ : true, - : false)
  constexpr Literal(Variable variable, bool polarity)
      : x(2 * variable + static_cast<Variable>(polarity)) {}

  /// Equality
  constexpr bool operator==(Literal p) const noexcept { return x == p.x; }
  /// Inequality
  constexpr bool operator!=(Literal p) const noexcept { return x != p.x; }
  /// Less than
  constexpr bool operator<(Literal p) const noexcept { return x < p.x; }
  /// Negation
  constexpr Literal operator~() const noexcept { return Literal(x ^ 1); }
  /// Make type convertible to std::size_t to use as index into vectors
  constexpr operator std::size_t() const noexcept { return x; }

  /// The polarity of the literal (+ : true, - : false)
  constexpr bool polarity() const noexcept { return x & 1; }
  /// The variable used in the literal
  constexpr Variable var() const noexcept { return x >> 1; }
  /// Whether is valid
  constexpr bool valid() const noexcept { return x != INVALID; }
};

/// Reference to a clause
class ClauseRef {
 private:
  /// Invalid representation
  static constexpr std::uint32_t INVALID = -1;
  /// Even indices are original clauses; odd indices are learned clauses
  std::uint32_t x;

 public:
  constexpr ClauseRef() : x(INVALID) {}
  /// New clause reference
  constexpr ClauseRef(std::uint32_t index, bool is_learned)
      : x(2 * index + is_learned) {}

  /// Equality
  constexpr bool operator==(ClauseRef cr) const noexcept { return x == cr.x; }

  /// Index
  constexpr std::uint32_t idx() const noexcept { return x >> 1; }
  /// Whether clause is learned
  constexpr bool isLearned() const noexcept { return x & 1; }
  /// Whether is valid
  constexpr bool valid() const noexcept { return x != INVALID; }
};

/// Class managing the creation, deletion, and access of clauses
class Clauses {
 private:
  /// Stores all clauses
  std::vector<std::vector<Literal>> clauses;
  /// Stores indices of empty vectors
  std::vector<std::uint32_t> free_indices_clauses;
  /// Stores clause activities
  std::vector<double> activities;

 public:
  /// Create clause management
  Clauses() : clauses(), free_indices_clauses(), activities() {}

  /// Size
  constexpr std::uint32_t size() const noexcept { return clauses.size(); }

  /// Copy (lvalue) or move (rvalue) clause into container
  ClauseRef addClause(std::vector<Literal> literals, bool is_learned_clause) {
    // Use free slot
    if (!free_indices_clauses.empty()) {
      auto idx = free_indices_clauses.back();
      free_indices_clauses.pop_back();
      clauses[idx] = std::move(literals);
      activities[idx] = 0.0;
      return {idx, is_learned_clause};
    }

    // Append at the end
    std::uint32_t idx = clauses.size();
    clauses.push_back(std::move(literals));
    activities.push_back(0.0);
    return {idx, is_learned_clause};
  }

  /// Remove clause
  void removeClause(ClauseRef clause_ref) {
    auto idx = clause_ref.idx();
    if (idx == clauses.size() - 1) {
      clauses.pop_back();
      activities.pop_back();
    } else {
      clauses[idx].clear();
      activities[idx] = 0.0;
      free_indices_clauses.push_back(idx);
    }
  }

  /// Clause at given index
  std::vector<Literal>& operator[](ClauseRef clause_ref) {
    assert(clause_ref.valid());
    return clauses.at(clause_ref.idx());
  }

  /// Clause activity at given index
  double& activity(ClauseRef clause_ref) {
    assert(clause_ref.valid());
    return activities.at(clause_ref.idx());
  }
};

/// Literal is watched by `clause_idx`.
/// If `Watch::blocker` is satisfied, clause is not required to be inspected
struct Watch {
  ClauseRef clause_ref;
  Literal blocker;

  constexpr Watch() : clause_ref(), blocker() {}
  constexpr Watch(ClauseRef clause_ref, Literal blocker)
      : clause_ref(clause_ref), blocker(blocker) {}

  /// Equality
  constexpr bool operator==(Watch w) const noexcept {
    return clause_ref == w.clause_ref;
  }
};

/// Store metadata for a variable
struct VariableMetadata {
  /// Link to a clause
  ClauseRef reason_clause_idx;
  /// The associated decision level for a variable assignment
  std::uint32_t decision_level;
};

}  // namespace ns::clauses
