cmake_minimum_required(VERSION 3.20)
project(stepByStep C)

set(CMAKE_C_STANDARD 99)

# 列出所有源文件
file(GLOB_RECURSE c_files "stepByStep/c2/f/*.c")
add_executable(cli_main_c2_f ${c_files})
# 查看内容
#message("Found the following C files:")
#foreach(c_file ${c_files})
#   message("  ${c_file}")
#endforeach()


