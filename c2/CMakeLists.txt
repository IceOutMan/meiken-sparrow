cmake_minimum_required(VERSION 3.16)
project(stepByStep C)
include_directories("${PROJECT_SOURCE_DIR}")
set(CMAKE_C_STANDARD 99)



#
#add_executable(cli-main c2/a/cli/cli.c
#        c2/a/compiler/compiler.c
#        c2/a/include/unicodeUtf8.c c2/a/include/utils.c
#        c2/a/object/class.c c2/a/object/header_obj.c c2/a/object/meta_obj.c c2/a/object/obj_fn.c c2/a/object/obj_list.c c2/a/object/obj_map.c c2/a/object/obj_range.c c2/a/object/obj_string.c c2/a/object/obj_thread.c
#        c2/a/parser/parser.c
#        c2/a/vm/core.c c2/a/vm/vm.c
#        )


#add_executable(cli-main c2/a/cli/cli.c
#        c2/a/compiler/compiler.c
#        c2/a/include/unicodeUtf8.c c2/a/include/utils.c
#        c2/a/object/class.c c2/a/object/header_obj.c c2/a/object/meta_obj.c c2/a/object/obj_fn.c c2/a/object/obj_list.c c2/a/object/obj_map.c c2/a/object/obj_range.c c2/a/object/obj_string.c c2/a/object/obj_thread.c
#        c2/a/parser/parser.c
#        c2/a/vm/core.c c2/a/vm/vm.c
#        )
#
#add_executable(cli-main c2/c/cli/cli.c
#        c2/c/compiler/compiler.c
#        c2/c/include/unicodeUtf8.c c2/c/include/utils.c
#        c2/c/object/class.c c2/c/object/header_obj.c c2/c/object/meta_obj.c c2/c/object/obj_fn.c c2/c/object/obj_list.c c2/c/object/obj_map.c c2/c/object/obj_range.c c2/c/object/obj_string.c c2/c/object/obj_thread.c
#        c2/c/parser/parser.c
#        c2/c/vm/core.c c2/c/vm/vm.c
#        )

#add_executable(cli-main c2/d/cli/cli.c
#        c2/d/compiler/compiler.c
#        c2/d/include/unicodeUtf8.c c2/d/include/utils.c
#        c2/d/object/class.c c2/d/object/header_obj.c c2/d/object/meta_obj.c c2/d/object/obj_fn.c c2/d/object/obj_list.c c2/d/object/obj_map.c c2/d/object/obj_range.c c2/d/object/obj_string.c c2/d/object/obj_thread.c
#        c2/d/parser/parser.c
#        c2/d/vm/core.c c2/d/vm/vm.c
#        )

#add_executable(cli-main c2/e/cli/cli.c
#        c2/e/compiler/compiler.c
#        c2/e/include/unicodeUtf8.c c2/e/include/utils.c
#        c2/e/object/class.c c2/e/object/header_obj.c c2/e/object/meta_obj.c c2/e/object/obj_fn.c c2/e/object/obj_list.c c2/e/object/obj_map.c c2/e/object/obj_range.c c2/e/object/obj_string.c c2/e/object/obj_thread.c
#        c2/e/parser/parser.c
#        c2/e/vm/core.c c2/e/vm/vm.c
#        )


#
#add_executable(cli-main c2/f/cli/cli.c
#        c2/f/compiler/compiler.c
#        c2/f/include/unicodeUtf8.c c2/f/include/utils.c
#        c2/f/object/class.c c2/f/object/header_obj.c c2/f/object/meta_obj.c c2/f/object/obj_fn.c c2/f/object/obj_list.c c2/f/object/obj_map.c c2/f/object/obj_range.c c2/f/object/obj_string.c c2/f/object/obj_thread.c
#        c2/f/parser/parser.c
#        c2/f/vm/core.c c2/f/vm/vm.c
#        )


#add_executable(cli-main c2/g/cli/cli.c
#        c2/g/compiler/compiler.c
#        c2/g/include/unicodeUtf8.c c2/g/include/utils.c
#        c2/g/object/class.c c2/g/object/header_obj.c c2/g/object/meta_obj.c c2/g/object/obj_fn.c c2/g/object/obj_list.c c2/g/object/obj_map.c c2/g/object/obj_range.c c2/g/object/obj_string.c c2/g/object/obj_thread.c
#        c2/g/parser/parser.c
#        c2/g/vm/core.c c2/g/vm/vm.c
#        )

#add_executable(cli-main c2/h/cli/cli.c
#        c2/h/compiler/compiler.c
#        c2/h/include/unicodeUtf8.c c2/h/include/utils.c
#        c2/h/object/class.c c2/h/object/header_obj.c c2/h/object/meta_obj.c c2/h/object/obj_fn.c c2/h/object/obj_list.c c2/h/object/obj_map.c c2/h/object/obj_range.c c2/h/object/obj_string.c c2/h/object/obj_thread.c
#        c2/h/parser/parser.c
#        c2/h/vm/core.c c2/h/vm/vm.c
#        )

#add_executable(cli-main c2/i/cli/cli.c
#        c2/i/compiler/compiler.c
#        c2/i/include/unicodeUtf8.c c2/i/include/utils.c
#        c2/i/object/class.c c2/i/object/header_obj.c c2/i/object/meta_obj.c c2/i/object/obj_fn.c c2/i/object/obj_list.c c2/i/object/obj_map.c c2/i/object/obj_range.c c2/i/object/obj_string.c c2/i/object/obj_thread.c
#        c2/i/parser/parser.c
#        c2/i/vm/core.c c2/i/vm/vm.c
#        )


#add_executable(cli-main c2/j/cli/cli.c
#        c2/j/compiler/compiler.c
#        c2/j/include/unicodeUtf8.c c2/j/include/utils.c
#        c2/j/object/class.c c2/j/object/header_obj.c c2/j/object/meta_obj.c c2/j/object/obj_fn.c c2/j/object/obj_list.c c2/j/object/obj_map.c c2/j/object/obj_range.c c2/j/object/obj_string.c c2/j/object/obj_thread.c
#        c2/j/parser/parser.c
#        c2/j/vm/core.c c2/j/vm/vm.c
#        )

#add_executable(cli-main c2/k/cli/cli.c
#        c2/k/compiler/compiler.c
#        c2/k/include/unicodeUtf8.c c2/k/include/utils.c
#        c2/k/object/class.c c2/k/object/header_obj.c c2/k/object/meta_obj.c c2/k/object/obj_fn.c c2/k/object/obj_list.c c2/k/object/obj_map.c c2/k/object/obj_range.c c2/k/object/obj_string.c c2/k/object/obj_thread.c
#        c2/k/parser/parser.c
#        c2/k/vm/core.c c2/k/vm/vm.c
#        )



