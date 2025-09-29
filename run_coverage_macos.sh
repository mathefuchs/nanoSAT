#!/bin/bash
set -euo pipefail

# 1. Clean up old coverage data
rm -f coverage.profraw coverage.profdata
rm -rf coverage_html

# 2. Rebuild with coverage flags
cmake -S . -B build \
  -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_CXX_FLAGS="-fprofile-instr-generate -fcoverage-mapping"
cmake --build build

# 3. Run tests, collect raw profile data
LLVM_PROFILE_FILE="coverage.profraw" ./build/tests/nanosat-test

# 4. Merge profile data
xcrun llvm-profdata merge -sparse coverage.profraw -o coverage.profdata

# 5. Generate HTML report
xcrun llvm-cov show ./build/tests/nanosat-test \
  -instr-profile=coverage.profdata \
  -format=html \
  -output-dir=coverage_html \
  -show-line-counts-or-regions

echo "✅ Coverage report generated at: coverage_html/index.html"
