cmake_minimum_required(VERSION 3.20)
project(stepByStep a)

set(CMAKE_C_STANDARD 99)

include_directories(stepByStep/c3/b/cli)
include_directories(stepByStep/c3/b/include)
include_directories(stepByStep/c3/b/object)
include_directories(stepByStep/c3/b/parser)
include_directories(stepByStep/c3/b/vm)

add_executable(cli-main
        stepByStep/c3/b/cli/cli.c
        stepByStep/c3/b/include/unicodeUtf8.c stepByStep/c3/b/include/utils.c
        stepByStep/c3/b/object/class.c stepByStep/c3/b/object/header_obj.c stepByStep/c3/b/object/meta_obj.c stepByStep/c3/b/object/obj_fn.c stepByStep/c3/b/object/obj_list.c stepByStep/c3/b/object/obj_map.c stepByStep/c3/b/object/obj_range.c stepByStep/c3/b/object/obj_string.c stepByStep/c3/b/object/obj_thread.c
        stepByStep/c3/b/parser/parser.c
        stepByStep/c3/b/vm/core.c stepByStep/c3/b/vm/vm.c
        )

