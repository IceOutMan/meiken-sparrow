cmake_minimum_required(VERSION 3.16)
project(stepByStep C)
include_directories("${PROJECT_SOURCE_DIR}")
set(CMAKE_C_STANDARD 99)



add_executable(cli-main c9/a/cli/cli.c
        c9/a/compiler/compiler.c
        c9/a/gc/gc.c
        c9/a/include/unicodeUtf8.c c9/a/include/utils.c
        c9/a/object/class.c c9/a/object/header_obj.c c9/a/object/meta_obj.c c9/a/object/obj_fn.c c9/a/object/obj_list.c c9/a/object/obj_map.c c9/a/object/obj_range.c c9/a/object/obj_string.c c9/a/object/obj_thread.c
        c9/a/parser/parser.c
        c9/a/vm/core.c c9/a/vm/vm.c
        c9/a/gc/gc.c)
