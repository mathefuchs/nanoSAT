set -ex

cmake -S . -B build \
  -DCMAKE_BUILD_TYPE=RelWithDebInfo \
  -DCMAKE_CXX_FLAGS=""
cmake --build build --config RelWithDebInfo -j
