cmake_minimum_required(VERSION 3.20)
project(stepByStep C)

set(CMAKE_C_STANDARD 99)

include_directories(stepByStep/c4/b/cli)
include_directories(stepByStep/c4/b/compiler)
include_directories(stepByStep/c4/b/include)
include_directories(stepByStep/c4/b/object)
include_directories(stepByStep/c4/b/parser)
include_directories(stepByStep/c4/b/vm)

add_executable(cli-main
        stepByStep/c4/b/cli/cli.c
        stepByStep/c4/b/compiler/compiler.c
        stepByStep/c4/b/include/unicodeUtf8.c stepByStep/c4/b/include/utils.c
        stepByStep/c4/b/object/class.c stepByStep/c4/b/object/header_obj.c stepByStep/c4/b/object/meta_obj.c stepByStep/c4/b/object/obj_fn.c stepByStep/c4/b/object/obj_list.c stepByStep/c4/b/object/obj_map.c stepByStep/c4/b/object/obj_range.c stepByStep/c4/b/object/obj_string.c stepByStep/c4/b/object/obj_thread.c
        stepByStep/c4/b/parser/parser.c
        stepByStep/c4/b/vm/core.c stepByStep/c4/b/vm/vm.c
        )

