add_library(utils OBJECT container.c pqos_utils.c general.c)
target_compile_options(utils PRIVATE -fPIC)