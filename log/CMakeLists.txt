add_library(log OBJECT src/log.c)
target_compile_options(log PRIVATE -fPIC)