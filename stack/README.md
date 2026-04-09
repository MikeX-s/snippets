---
    cmake -B build
    cmake --build build
    cmake -DENABLE_TSAN=OFF -DENABLE_VALGRIND=ON build
    valgrind --tool=memcheck --leak-check=full ./build/Tests
    ctest --test-dir build --output-on-failure
    ctest --test-dir build -R ValgrindTests
    ./build/Benchmark
---