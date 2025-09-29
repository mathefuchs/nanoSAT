set -ex

sh build_relwithdebinfo.sh

xctrace record --output . --template "Time Profiler" --launch -- ./build/nanosat ./tests/examples/success/big_sat_instance.cnf.xz
