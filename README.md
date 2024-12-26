mkdir build && cd build

cmake .. && make


ctest --output-on-failure


ctest -R dlna_test --output-on-failure -V | cat