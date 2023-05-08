cmake_minimum_required(VERSION 3.16)
project(stepByStep C)
include_directories("${PROJECT_SOURCE_DIR}")
set(CMAKE_C_STANDARD 99)

add_definitions("-DEBUG")

add_executable(cli-main c10/a/cli/cli.c
        c10/a/compiler/compiler.c c10/a/compiler/debug.c
        c10/a/gc/gc.c
        c10/a/include/unicodeUtf8.c c10/a/include/utils.c
        c10/a/object/class.c c10/a/object/header_obj.c c10/a/object/meta_obj.c c10/a/object/obj_fn.c c10/a/object/obj_list.c c10/a/object/obj_map.c c10/a/object/obj_range.c c10/a/object/obj_string.c c10/a/object/obj_thread.c
        c10/a/parser/parser.c
        c10/a/vm/core.c c10/a/vm/vm.c
        c10/a/gc/gc.c)
