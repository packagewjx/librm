add_library(utils OBJECT container.c pqos_utils.c general.c example_programs.c example_programs.h)
target_compile_options(utils PRIVATE -fPIC)