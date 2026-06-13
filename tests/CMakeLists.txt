# tests/CMakeLists.txt
#
# Two separate kinds of testing live under tests/ (see each subdir's purpose):
#
#   unit/        Fast C++ unit tests. Run on every build via ctest. No Python.
#                Best first targets: the math primitive, then each scalar kernel.
#                Recommended framework: doctest (single header, vendor it under
#                tests/third_party/ when you add the first test).
#
#   validation/  Cross-language ground-truth sweep. A C++ tool dumps a grid of
#                outputs to CSV; a Python script recomputes the same grid with
#                py_vollib / QuantLib and asserts agreement to tolerance. Runs
#                separately (needs a Python env), NOT on every build.
#
# Empty for now — mentees add test files and wire them here. Example once a unit
# test exists:
#
#   add_executable(onitro_unit_tests unit/test_normal.cpp)
#   target_link_libraries(onitro_unit_tests PRIVATE onitro::onitro)
#   target_include_directories(onitro_unit_tests PRIVATE third_party)
#   add_test(NAME onitro_unit_tests COMMAND onitro_unit_tests)
