cmake_minimum_required(VERSION 3.20)
project(stepByStep C)

set(CMAKE_C_STANDARD 99)

# 列出所有源文件
file(GLOB_RECURSE c_files "stepByStep/c2/f/*.c")
add_executable(cli_main_c2_f ${c_files})

file(GLOB_RECURSE c_files "stepByStep/c3/a/*.c")
add_executable(cli_main_c3_a ${c_files})

file(GLOB_RECURSE c_files "stepByStep/c3/b/*.c")
add_executable(cli_main_c3_b ${c_files})

file(GLOB_RECURSE c_files "stepByStep/c3/c/*.c")
add_executable(cli_main_c3_c ${c_files})

file(GLOB_RECURSE c_files "stepByStep/c4/a/*.c")
add_executable(cli_main_c4_a ${c_files})

file(GLOB_RECURSE c_files "stepByStep/c4/b/*.c")
add_executable(cli_main_c4_b ${c_files})


file(GLOB_RECURSE c_files "stepByStep/c4/c/*.c")
add_executable(cli_main_c4_c ${c_files})


file(GLOB_RECURSE c_files "stepByStep/c6/a/*.c")
add_executable(cli_main_c6_a ${c_files})

file(GLOB_RECURSE c_files "stepByStep/c6/b/*.c")
add_executable(cli_main_c6_b ${c_files})

file(GLOB_RECURSE c_files "stepByStep/c6/c/*.c")
add_executable(cli_main_c6_c ${c_files})




# 查看内容
message("Found the following C files:")
foreach(c_file ${c_files})
   message("  ${c_file}")
endforeach()