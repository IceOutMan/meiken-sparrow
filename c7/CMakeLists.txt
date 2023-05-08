cmake_minimum_required(VERSION 3.16)
project(stepByStep C)
include_directories("${PROJECT_SOURCE_DIR}")
set(CMAKE_C_STANDARD 99)


add_executable(cli-main c7/a/cli/cli.c
        c7/a/compiler/compiler.c
        c7/a/include/unicodeUtf8.c c7/a/include/utils.c
        c7/a/object/class.c c7/a/object/header_obj.c c7/a/object/meta_obj.c c7/a/object/obj_fn.c c7/a/object/obj_list.c c7/a/object/obj_map.c c7/a/object/obj_range.c c7/a/object/obj_string.c c7/a/object/obj_thread.c
        c7/a/parser/parser.c
        c7/a/vm/core.c c7/a/vm/vm.c
        )




