cmake_minimum_required(VERSION 3.20)
project(stepByStep C)

set(CMAKE_C_STANDARD 99)

include_directories(stepByStep/c4/a/cli)
include_directories(stepByStep/c4/a/include)
include_directories(stepByStep/c4/a/object)
include_directories(stepByStep/c4/a/parser)
include_directories(stepByStep/c4/a/vm)

add_executable(cli-main
        stepByStep/c4/a/cli/cli.c
        stepByStep/c4/a/include/unicodeUtf8.c stepByStep/c4/a/include/utils.c
        stepByStep/c4/a/object/class.c stepByStep/c4/a/object/header_obj.c stepByStep/c4/a/object/meta_obj.c stepByStep/c4/a/object/obj_fn.c stepByStep/c4/a/object/obj_list.c stepByStep/c4/a/object/obj_map.c stepByStep/c4/a/object/obj_range.c stepByStep/c4/a/object/obj_string.c stepByStep/c4/a/object/obj_thread.c
        stepByStep/c4/a/parser/parser.c
        stepByStep/c4/a/vm/core.c stepByStep/c4/a/vm/vm.c
        )

