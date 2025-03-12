cmake_minimum_required(VERSION 3.5)
project(lazy-k)

add_compile_definitions(GC_EVERY=10000000)

add_compile_definitions(OPTIMIZE_SI_LOAD)
#add_compile_definitions(OPTIMIZE_SI_RUN)
add_compile_definitions(OPTIMIZE_SxI_RUN)

add_compile_definitions(OPTIMIZE_KI_LOAD)
#add_compile_definitions(OPTIMIZE_KI_RUN)

#add_compile_definitions(OPTIMIZE_M_LOAD)
#add_compile_definitions(OPTIMIZE_M_RUN)

add_executable(lazy-k lazy-k.c)
#target_compile_options(lazy-k PUBLIC -Wl,-z,stacksize=0x40000000)
