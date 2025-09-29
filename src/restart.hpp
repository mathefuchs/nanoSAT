#pragma once

#include <cmath>

namespace ns::solver::restart {

/// Luby restart sequence (Luby, Sinclair, Zuckerman 1993);
/// `1,1,2,1,1,2,4,1,1,2,1,1,2,4,8,...`
inline double luby(double y, int x) {
  int size, seq;
  for (size = 1, seq = 0; size < x + 1; seq++, size = 2 * size + 1);

  while (size - 1 != x) {
    size = (size - 1) >> 1;
    seq--;
    x = x % size;
  }

  return std::pow(y, seq);
}

}  // namespace ns::solver::restart
