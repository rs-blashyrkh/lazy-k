cmake_minimum_required(VERSION 3.5)
project(lazy-k)

add_compile_definitions(GC_EVERY=10000000)

add_executable(lazy-k lazy-k.c)
#target_compile_options(lazy-k PUBLIC -Wl,-z,stacksize=0x40000000)
