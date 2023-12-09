# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.20.0)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(pub_sub_sub_msgq)

target_include_directories(app PRIVATE ../test_helpers)
target_sources(app PRIVATE
    src/main.c
    ../test_helpers/helpers.c
)