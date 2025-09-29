set -ex

mkdir build || true
wget https://github.com/google/googletest/releases/download/v1.17.0/googletest-1.17.0.tar.gz
tar xvf googletest-1.17.0.tar.gz
mv googletest-1.17.0 external/googletest
rm googletest-1.17.0.tar.gz
