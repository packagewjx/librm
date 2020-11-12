add_library(utils container.c pqos_utils.c general.c)
target_link_libraries(utils pqos)