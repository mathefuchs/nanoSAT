#pragma once

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <string>
#include <utility>
#include <vector>

#include "clauses.hpp"

namespace ns::parse {

namespace {

/// Parsing state
enum class ParseState : std::uint8_t {
  /// Read a new line character; next char is in the next line
  NEW_LINE,
  EXPECT_NEW_LINE,
  /// Comment line
  COMMENT,
  /// `p cnf nv nc` header
  HEADER_P_,          // Expect ' ' next
  HEADER_P_C,         // Expect 'c' next
  HEADER_P_CN,        // Expect 'n' next
  HEADER_P_CNF,       // Expect 'f' next
  HEADER_P_CNF_,      // Expect ' ' next
  HEADER_P_CNF_N,     // Expect 0-9 next
  HEADER_P_CNF_N_,    // Expect 0-9 or space next
  HEADER_P_CNF_N_N,   // Expect 0-9 next
  HEADER_P_CNF_N_N_,  // Expect 0-9 or new line next
  /// Reading clause
  CLAUSE_DIGIT,        // Expect digit (1-9) next
  CLAUSE_DIGIT_SPACE,  // Expect digit (0-9), space, or new line next
  CLAUSE_DIGIT_MINUS,  // Expect digit (0-9) or minus (-) next
};

/// Received unexpected token
inline void unexpected_token() {
  std::cerr << "Failed to parse cnf file." << std::endl;
  std::exit(EXIT_FAILURE);
}

/// Open plain text file
inline auto openPlainFile(const std::string& filename) {
  auto* file = fopen(filename.c_str(), "r");
  if (!file) {
    std::cerr << "Failed to open file \"" << filename
              << "\" using plain text mode." << std::endl;
    std::exit(EXIT_FAILURE);
  }
  return std::make_pair(file, fclose);
}

/// Open xz-compressed file
inline auto openXzFile(const std::string& filename) {
  std::string xz_cmd = "xz -dc " + filename;
  auto* pipe = popen(xz_cmd.c_str(), "r");
  if (!pipe) {
    std::cerr << "Failed to decompress file \"" << filename
              << "\" using \"xz\"." << std::endl;
    std::exit(EXIT_FAILURE);
  }
  return std::make_pair(pipe, pclose);
}

/// Open gzip-compressed file
inline auto openGzipFile(const std::string& filename) {
  std::string gz_cmd = "gzip -dc " + filename;
  auto* pipe = popen(gz_cmd.c_str(), "r");
  if (!pipe) {
    std::cerr << "Failed to decompress file \"" << filename
              << "\" using \"gzip\"." << std::endl;
    std::exit(EXIT_FAILURE);
  }
  return std::make_pair(pipe, pclose);
}

}  // namespace

/// Parse `.cnf`, `.cnf.xz`, or `.cnf.gz`
template <class Solver>
inline Solver parseCnf(const std::string& filename) {
  // Open file
  auto [file, close_fn] =
      filename.ends_with(".xz")
          ? openXzFile(filename)
          : (filename.ends_with(".gz") ? openGzipFile(filename)
                                       : openPlainFile(filename));
  std::array<char, 4096> buffer;

  // Solver
  Solver solver;

  // Current number of variabels and clauses
  std::uint32_t num_variables_header = 0;
  std::uint32_t curr_num_variables = 0;
  std::uint32_t num_clauses_header = 0;
  std::uint32_t curr_num_clauses = 0;
  bool processed_header = false;

  // Current clause and literal
  std::vector<clauses::Literal> clause;
  std::uint32_t variable = 0;
  bool sign = true;

  // Parse file
  auto curr_state = ParseState::NEW_LINE;
  std::size_t n_bytes_read = 0;
  while (true) {
    // Read next chunk
    n_bytes_read = fread(buffer.data(), 1, buffer.size(), file);
    if (n_bytes_read == 0) {
      break;
    }

    // Run state machine across all read bytes
    for (std::size_t i = 0; i < n_bytes_read; ++i) {
      char c = buffer[i];
      switch (curr_state) {
        // Read a new line character; next char is in the next line
        case ParseState::NEW_LINE:
          if (c == '\n' || c == '\r') {
            continue;
          }
          if (!processed_header && c == 'p') {
            curr_state = ParseState::HEADER_P_;
            processed_header = true;
          } else if (c == 'c') {
            curr_state = ParseState::COMMENT;
          } else if (processed_header && c == '-') {
            sign = false;
            curr_state = ParseState::CLAUSE_DIGIT;
            // Prepare for new clause
            clause.clear();
            ++curr_num_clauses;
          } else if (processed_header && c >= '1' && c <= '9') {
            variable = static_cast<std::uint32_t>(c - '0');
            sign = true;
            curr_state = ParseState::CLAUSE_DIGIT_SPACE;
            // Prepare for new clause
            clause.clear();
            ++curr_num_clauses;
          } else {
            unexpected_token();
          }
          break;

        case ParseState::EXPECT_NEW_LINE:
          if (c == '\n' || c == '\r') {
            curr_state = ParseState::NEW_LINE;
          } else {
            unexpected_token();
          }
          break;

        // Comment line
        case ParseState::COMMENT:
          if (c == '\n' || c == '\r') {
            curr_state = ParseState::NEW_LINE;
          }
          break;

        // `p cnf nv nc` header
        case ParseState::HEADER_P_:
          if (c == ' ') {
            curr_state = ParseState::HEADER_P_C;
          } else {
            unexpected_token();
          }
          break;

        case ParseState::HEADER_P_C:
          if (c == 'c') {
            curr_state = ParseState::HEADER_P_CN;
          } else {
            unexpected_token();
          }
          break;

        case ParseState::HEADER_P_CN:
          if (c == 'n') {
            curr_state = ParseState::HEADER_P_CNF;
          } else {
            unexpected_token();
          }
          break;

        case ParseState::HEADER_P_CNF:
          if (c == 'f') {
            curr_state = ParseState::HEADER_P_CNF_;
          } else {
            unexpected_token();
          }
          break;

        case ParseState::HEADER_P_CNF_:
          if (c == ' ') {
            curr_state = ParseState::HEADER_P_CNF_N;
          } else {
            unexpected_token();
          }
          break;

        case ParseState::HEADER_P_CNF_N:
          if (c >= '1' && c <= '9') {
            num_variables_header = static_cast<std::uint32_t>(c - '0');
            curr_state = ParseState::HEADER_P_CNF_N_;
          } else {
            unexpected_token();
          }
          break;

        case ParseState::HEADER_P_CNF_N_:
          if (c == ' ') {
            curr_state = ParseState::HEADER_P_CNF_N_N;
          } else if (c >= '0' && c <= '9') {
            num_variables_header =
                10 * num_variables_header + static_cast<std::uint32_t>(c - '0');
          } else {
            unexpected_token();
          }
          break;

        case ParseState::HEADER_P_CNF_N_N:
          if (c >= '1' && c <= '9') {
            num_clauses_header = static_cast<std::uint32_t>(c - '0');
            curr_state = ParseState::HEADER_P_CNF_N_N_;
          } else {
            unexpected_token();
          }
          break;

        case ParseState::HEADER_P_CNF_N_N_:
          if (c == '\n' || c == '\r') {
            // Completed parsing header
            solver.createVariables(num_variables_header);
            curr_state = ParseState::NEW_LINE;
          } else if (c >= '0' && c <= '9') {
            num_clauses_header =
                10 * num_clauses_header + static_cast<std::uint32_t>(c - '0');
          } else {
            unexpected_token();
          }
          break;

        // Reading clause
        case ParseState::CLAUSE_DIGIT:
          if (c >= '1' && c <= '9') {
            variable = static_cast<std::uint32_t>(c - '0');
            curr_state = ParseState::CLAUSE_DIGIT_SPACE;
          } else {
            unexpected_token();
          }
          break;

        case ParseState::CLAUSE_DIGIT_SPACE:
          if (c == ' ') {
            curr_state = ParseState::CLAUSE_DIGIT_MINUS;
            clause.emplace_back(variable - 1, sign);
            if (variable > curr_num_variables) {
              curr_num_variables = variable;
            }
            sign = true;
          } else if (c >= '0' && c <= '9') {
            variable = 10 * variable + static_cast<std::uint32_t>(c - '0');
          } else {
            unexpected_token();
          }
          break;

        case ParseState::CLAUSE_DIGIT_MINUS:
          if (c == '-') {
            curr_state = ParseState::CLAUSE_DIGIT;
            sign = false;
          } else if (c == '0') {
            curr_state = ParseState::EXPECT_NEW_LINE;
            // Finished clause;
            // if already unsat, no need to add remaining clauses
            bool still_satisfiable = solver.addClause(clause);
            if (!still_satisfiable) {
              close_fn(file);
              return solver;
            }
          } else if (c >= '1' && c <= '9') {
            variable = static_cast<std::uint32_t>(c - '0');
            curr_state = ParseState::CLAUSE_DIGIT_SPACE;
          } else {
            unexpected_token();
          }
          break;
      }
    }
  }

  // Close file or pipe
  int exit_status = close_fn(file);
  if (exit_status != 0) {
    std::cerr << "Failed to read from file or pipe." << std::endl;
    std::exit(EXIT_FAILURE);
  }

  // Invalid if file ended in an intermediate state
  if (curr_state != ParseState::NEW_LINE) {
    unexpected_token();
  }

  // Check number of variables and clauses
  if (curr_num_variables != num_variables_header) {
    std::cerr << "Number of variables in cnf incorrect." << std::endl;
    std::exit(EXIT_FAILURE);
  }
  if (curr_num_clauses != num_clauses_header) {
    std::cerr << "Number of clauses in cnf incorrect." << std::endl;
    std::exit(EXIT_FAILURE);
  }

  // Return solver
  return solver;
}

}  // namespace ns::parse
