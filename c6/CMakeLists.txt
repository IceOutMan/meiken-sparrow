cmake_minimum_required(VERSION 3.16)
project(stepByStep C)
include_directories("${PROJECT_SOURCE_DIR}")
set(CMAKE_C_STANDARD 99)

#add_executable(cli-main c6/e/cli/cli.c
#        c6/e/compiler/compiler.c
#        c6/e/include/unicodeUtf8.c c6/e/include/utils.c
#        c6/e/object/class.c c6/e/object/header_obj.c c6/e/object/meta_obj.c c6/e/object/obj_fn.c c6/e/object/obj_list.c c6/e/object/obj_map.c c6/e/object/obj_range.c c6/e/object/obj_string.c c6/e/object/obj_thread.c
#        c6/e/parser/parser.c
#        c6/e/vm/core.c c6/e/vm/vm.c
#        )
#
#add_executable(cli-main c6/f/cli/cli.c
#        c6/f/compiler/compiler.c
#        c6/f/include/unicodeUtf8.c c6/f/include/utils.c
#        c6/f/object/class.c c6/f/object/header_obj.c c6/f/object/meta_obj.c c6/f/object/obj_fn.c c6/f/object/obj_list.c c6/f/object/obj_map.c c6/f/object/obj_range.c c6/f/object/obj_string.c c6/f/object/obj_thread.c
#        c6/f/parser/parser.c
#        c6/f/vm/core.c c6/f/vm/vm.c
#        )

#add_executable(cli-main c6/g/cli/cli.c
#        c6/g/compiler/compiler.c
#        c6/g/include/unicodeUtf8.c c6/g/include/utils.c
#        c6/g/object/class.c c6/g/object/header_obj.c c6/g/object/meta_obj.c c6/g/object/obj_fn.c c6/g/object/obj_list.c c6/g/object/obj_map.c c6/g/object/obj_range.c c6/g/object/obj_string.c c6/g/object/obj_thread.c
#        c6/g/parser/parser.c
#        c6/g/vm/core.c c6/g/vm/vm.c
#        )
#
#add_executable(cli-main c6/h/cli/cli.c
#        c6/h/compiler/compiler.c
#        c6/h/include/unicodeUtf8.c c6/h/include/utils.c
#        c6/h/object/class.c c6/h/object/header_obj.c c6/h/object/meta_obj.c c6/h/object/obj_fn.c c6/h/object/obj_list.c c6/h/object/obj_map.c c6/h/object/obj_range.c c6/h/object/obj_string.c c6/h/object/obj_thread.c
#        c6/h/parser/parser.c
#        c6/h/vm/core.c c6/h/vm/vm.c
#        )

#
#add_executable(cli-main c6/i/cli/cli.c
#        c6/i/compiler/compiler.c
#        c6/i/include/unicodeUtf8.c c6/i/include/utils.c
#        c6/i/object/class.c c6/i/object/header_obj.c c6/i/object/meta_obj.c c6/i/object/obj_fn.c c6/i/object/obj_list.c c6/i/object/obj_map.c c6/i/object/obj_range.c c6/i/object/obj_string.c c6/i/object/obj_thread.c
#        c6/i/parser/parser.c
#        c6/i/vm/core.c c6/i/vm/vm.c
#        )

#add_executable(cli-main c6/j/cli/cli.c
#        c6/j/compiler/compiler.c
#        c6/j/include/unicodeUtf8.c c6/j/include/utils.c
#        c6/j/object/class.c c6/j/object/header_obj.c c6/j/object/meta_obj.c c6/j/object/obj_fn.c c6/j/object/obj_list.c c6/j/object/obj_map.c c6/j/object/obj_range.c c6/j/object/obj_string.c c6/j/object/obj_thread.c
#        c6/j/parser/parser.c
#        c6/j/vm/core.c c6/j/vm/vm.c
#        )

#add_executable(cli-main c6/k/cli/cli.c
#        c6/k/compiler/compiler.c
#        c6/k/include/unicodeUtf8.c c6/k/include/utils.c
#        c6/k/object/class.c c6/k/object/header_obj.c c6/k/object/meta_obj.c c6/k/object/obj_fn.c c6/k/object/obj_list.c c6/k/object/obj_map.c c6/k/object/obj_range.c c6/k/object/obj_string.c c6/k/object/obj_thread.c
#        c6/k/parser/parser.c
#        c6/k/vm/core.c c6/k/vm/vm.c
#        )

#add_executable(cli-main c6/a/cli/cli.c
#        c6/a/compiler/compiler.c
#        c6/a/include/unicodeUtf8.c c6/a/include/utils.c
#        c6/a/object/class.c c6/a/object/header_obj.c c6/a/object/meta_obj.c c6/a/object/obj_fn.c c6/a/object/obj_list.c c6/a/object/obj_map.c c6/a/object/obj_range.c c6/a/object/obj_string.c c6/a/object/obj_thread.c
#        c6/a/parser/parser.c
#        c6/a/vm/core.c c6/a/vm/vm.c
#        )

#
#add_executable(cli-main c6/a/cli/cli.c
#        c6/a/compiler/compiler.c
#        c6/a/include/unicodeUtf8.c c6/a/include/utils.c
#        c6/a/object/class.c c6/a/object/header_obj.c c6/a/object/meta_obj.c c6/a/object/obj_fn.c c6/a/object/obj_list.c c6/a/object/obj_map.c c6/a/object/obj_range.c c6/a/object/obj_string.c c6/a/object/obj_thread.c
#        c6/a/parser/parser.c
#        c6/a/vm/core.c c6/a/vm/vm.c
#        )

#add_executable(cli-main c6/n/cli/cli.c
#        c6/n/compiler/compiler.c
#        c6/n/include/unicodeUtf8.c c6/n/include/utils.c
#        c6/n/object/class.c c6/n/object/header_obj.c c6/n/object/meta_obj.c c6/n/object/obj_fn.c c6/n/object/obj_list.c c6/n/object/obj_map.c c6/n/object/obj_range.c c6/n/object/obj_string.c c6/n/object/obj_thread.c
#        c6/n/parser/parser.c
#        c6/n/vm/core.c c6/n/vm/vm.c
#        )

#add_executable(cli-main c6/o/cli/cli.c
#        c6/o/compiler/compiler.c
#        c6/o/include/unicodeUtf8.c c6/o/include/utils.c
#        c6/o/object/class.c c6/o/object/header_obj.c c6/o/object/meta_obj.c c6/o/object/obj_fn.c c6/o/object/obj_list.c c6/o/object/obj_map.c c6/o/object/obj_range.c c6/o/object/obj_string.c c6/o/object/obj_thread.c
#        c6/o/parser/parser.c
#        c6/o/vm/core.c c6/o/vm/vm.c
#        )

#add_executable(cli-main c6/p/cli/cli.c
#        c6/p/compiler/compiler.c
#        c6/p/include/unicodeUtf8.c c6/p/include/utils.c
#        c6/p/object/class.c c6/p/object/header_obj.c c6/p/object/meta_obj.c c6/p/object/obj_fn.c c6/p/object/obj_list.c c6/p/object/obj_map.c c6/p/object/obj_range.c c6/p/object/obj_string.c c6/p/object/obj_thread.c
#        c6/p/parser/parser.c
#        c6/p/vm/core.c c6/p/vm/vm.c
#        )

#add_executable(cli-main c6/q/cli/cli.c
#        c6/q/compiler/compiler.c
#        c6/q/include/unicodeUtf8.c c6/q/include/utils.c
#        c6/q/object/class.c c6/q/object/header_obj.c c6/q/object/meta_obj.c c6/q/object/obj_fn.c c6/q/object/obj_list.c c6/q/object/obj_map.c c6/q/object/obj_range.c c6/q/object/obj_string.c c6/q/object/obj_thread.c
#        c6/q/parser/parser.c
#        c6/q/vm/core.c c6/q/vm/vm.c
#        )

#add_executable(cli-main c6/r/cli/cli.c
#        c6/r/compiler/compiler.c
#        c6/r/include/unicodeUtf8.c c6/r/include/utils.c
#        c6/r/object/class.c c6/r/object/header_obj.c c6/r/object/meta_obj.c c6/r/object/obj_fn.c c6/r/object/obj_list.c c6/r/object/obj_map.c c6/r/object/obj_range.c c6/r/object/obj_string.c c6/r/object/obj_thread.c
#        c6/r/parser/parser.c
#        c6/r/vm/core.c c6/r/vm/vm.c
#        )

#add_executable(cli-main c6/s/cli/cli.c
#        c6/s/compiler/compiler.c
#        c6/s/include/unicodeUtf8.c c6/s/include/utils.c
#        c6/s/object/class.c c6/s/object/header_obj.c c6/s/object/meta_obj.c c6/s/object/obj_fn.c c6/s/object/obj_list.c c6/s/object/obj_map.c c6/s/object/obj_range.c c6/s/object/obj_string.c c6/s/object/obj_thread.c
#        c6/s/parser/parser.c
#        c6/s/vm/core.c c6/s/vm/vm.c
#        )

add_executable(cli-main c6/t/cli/cli.c
        c6/t/compiler/compiler.c
        c6/t/include/unicodeUtf8.c c6/t/include/utils.c
        c6/t/object/class.c c6/t/object/header_obj.c c6/t/object/meta_obj.c c6/t/object/obj_fn.c c6/t/object/obj_list.c c6/t/object/obj_map.c c6/t/object/obj_range.c c6/t/object/obj_string.c c6/t/object/obj_thread.c
        c6/t/parser/parser.c
        c6/t/vm/core.c c6/t/vm/vm.c
        )




