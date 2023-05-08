cmake_minimum_required(VERSION 3.16)
project(stepByStep C)
include_directories("${PROJECT_SOURCE_DIR}")
set(CMAKE_C_STANDARD 99)



#
#add_executable(cli-main c8/a/cli/cli.c
#        c8/a/compiler/compiler.c
#        c8/a/include/unicodeUtf8.c c8/a/include/utils.c
#        c8/a/object/class.c c8/a/object/header_obj.c c8/a/object/meta_obj.c c8/a/object/obj_fn.c c8/a/object/obj_list.c c8/a/object/obj_map.c c8/a/object/obj_range.c c8/a/object/obj_string.c c8/a/object/obj_thread.c
#        c8/a/parser/parser.c
#        c8/a/vm/core.c c8/a/vm/vm.c
#        )


#add_executable(cli-main c8/a/cli/cli.c
#        c8/a/compiler/compiler.c
#        c8/a/include/unicodeUtf8.c c8/a/include/utils.c
#        c8/a/object/class.c c8/a/object/header_obj.c c8/a/object/meta_obj.c c8/a/object/obj_fn.c c8/a/object/obj_list.c c8/a/object/obj_map.c c8/a/object/obj_range.c c8/a/object/obj_string.c c8/a/object/obj_thread.c
#        c8/a/parser/parser.c
#        c8/a/vm/core.c c8/a/vm/vm.c
#        )
#
#add_executable(cli-main c8/c/cli/cli.c
#        c8/c/compiler/compiler.c
#        c8/c/include/unicodeUtf8.c c8/c/include/utils.c
#        c8/c/object/class.c c8/c/object/header_obj.c c8/c/object/meta_obj.c c8/c/object/obj_fn.c c8/c/object/obj_list.c c8/c/object/obj_map.c c8/c/object/obj_range.c c8/c/object/obj_string.c c8/c/object/obj_thread.c
#        c8/c/parser/parser.c
#        c8/c/vm/core.c c8/c/vm/vm.c
#        )

#add_executable(cli-main c8/d/cli/cli.c
#        c8/d/compiler/compiler.c
#        c8/d/include/unicodeUtf8.c c8/d/include/utils.c
#        c8/d/object/class.c c8/d/object/header_obj.c c8/d/object/meta_obj.c c8/d/object/obj_fn.c c8/d/object/obj_list.c c8/d/object/obj_map.c c8/d/object/obj_range.c c8/d/object/obj_string.c c8/d/object/obj_thread.c
#        c8/d/parser/parser.c
#        c8/d/vm/core.c c8/d/vm/vm.c
#        )

#add_executable(cli-main c8/e/cli/cli.c
#        c8/e/compiler/compiler.c
#        c8/e/include/unicodeUtf8.c c8/e/include/utils.c
#        c8/e/object/class.c c8/e/object/header_obj.c c8/e/object/meta_obj.c c8/e/object/obj_fn.c c8/e/object/obj_list.c c8/e/object/obj_map.c c8/e/object/obj_range.c c8/e/object/obj_string.c c8/e/object/obj_thread.c
#        c8/e/parser/parser.c
#        c8/e/vm/core.c c8/e/vm/vm.c
#        )


#
#add_executable(cli-main c8/f/cli/cli.c
#        c8/f/compiler/compiler.c
#        c8/f/include/unicodeUtf8.c c8/f/include/utils.c
#        c8/f/object/class.c c8/f/object/header_obj.c c8/f/object/meta_obj.c c8/f/object/obj_fn.c c8/f/object/obj_list.c c8/f/object/obj_map.c c8/f/object/obj_range.c c8/f/object/obj_string.c c8/f/object/obj_thread.c
#        c8/f/parser/parser.c
#        c8/f/vm/core.c c8/f/vm/vm.c
#        )


#add_executable(cli-main c8/g/cli/cli.c
#        c8/g/compiler/compiler.c
#        c8/g/include/unicodeUtf8.c c8/g/include/utils.c
#        c8/g/object/class.c c8/g/object/header_obj.c c8/g/object/meta_obj.c c8/g/object/obj_fn.c c8/g/object/obj_list.c c8/g/object/obj_map.c c8/g/object/obj_range.c c8/g/object/obj_string.c c8/g/object/obj_thread.c
#        c8/g/parser/parser.c
#        c8/g/vm/core.c c8/g/vm/vm.c
#        )

#add_executable(cli-main c8/h/cli/cli.c
#        c8/h/compiler/compiler.c
#        c8/h/include/unicodeUtf8.c c8/h/include/utils.c
#        c8/h/object/class.c c8/h/object/header_obj.c c8/h/object/meta_obj.c c8/h/object/obj_fn.c c8/h/object/obj_list.c c8/h/object/obj_map.c c8/h/object/obj_range.c c8/h/object/obj_string.c c8/h/object/obj_thread.c
#        c8/h/parser/parser.c
#        c8/h/vm/core.c c8/h/vm/vm.c
#        )

#add_executable(cli-main c8/i/cli/cli.c
#        c8/i/compiler/compiler.c
#        c8/i/include/unicodeUtf8.c c8/i/include/utils.c
#        c8/i/object/class.c c8/i/object/header_obj.c c8/i/object/meta_obj.c c8/i/object/obj_fn.c c8/i/object/obj_list.c c8/i/object/obj_map.c c8/i/object/obj_range.c c8/i/object/obj_string.c c8/i/object/obj_thread.c
#        c8/i/parser/parser.c
#        c8/i/vm/core.c c8/i/vm/vm.c
#        )


#add_executable(cli-main c8/j/cli/cli.c
#        c8/j/compiler/compiler.c
#        c8/j/include/unicodeUtf8.c c8/j/include/utils.c
#        c8/j/object/class.c c8/j/object/header_obj.c c8/j/object/meta_obj.c c8/j/object/obj_fn.c c8/j/object/obj_list.c c8/j/object/obj_map.c c8/j/object/obj_range.c c8/j/object/obj_string.c c8/j/object/obj_thread.c
#        c8/j/parser/parser.c
#        c8/j/vm/core.c c8/j/vm/vm.c
#        )

#add_executable(cli-main c8/k/cli/cli.c
#        c8/k/compiler/compiler.c
#        c8/k/include/unicodeUtf8.c c8/k/include/utils.c
#        c8/k/object/class.c c8/k/object/header_obj.c c8/k/object/meta_obj.c c8/k/object/obj_fn.c c8/k/object/obj_list.c c8/k/object/obj_map.c c8/k/object/obj_range.c c8/k/object/obj_string.c c8/k/object/obj_thread.c
#        c8/k/parser/parser.c
#        c8/k/vm/core.c c8/k/vm/vm.c
#        )

#add_executable(cli-main c8/a/cli/cli.c
#        c8/a/compiler/compiler.c
#        c8/a/include/unicodeUtf8.c c8/a/include/utils.c
#        c8/a/object/class.c c8/a/object/header_obj.c c8/a/object/meta_obj.c c8/a/object/obj_fn.c c8/a/object/obj_list.c c8/a/object/obj_map.c c8/a/object/obj_range.c c8/a/object/obj_string.c c8/a/object/obj_thread.c
#        c8/a/parser/parser.c
#        c8/a/vm/core.c c8/a/vm/vm.c
#        )



