# 设置最低CMake版本要求
cmake_minimum_required(VERSION 3.31)

# 项目名称和版本
project(motor_control VERSION 1.0.0 LANGUAGES CXX)

# 设置C++标准
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# MinGW 编译选项
add_compile_options(-Wall -Wextra -Wpedantic)

# 设置为控制台应用程序
set(CMAKE_WIN32_EXECUTABLE FALSE)

# 设置输出目录
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# 添加源文件
file(GLOB_RECURSE SOURCES "src/*.cpp" "example/*.cpp" "User/src/*.cpp")
file(GLOB_RECURSE HEADERS "include/*.h" "include/*.hpp" "User/include/*.h" "User/include/*.hpp")

# 创建可执行文件
add_executable(${PROJECT_NAME} ${SOURCES} ${HEADERS} User/src/PidController.cpp)

# 添加头文件目录
target_include_directories(${PROJECT_NAME} PRIVATE include User)

# 添加 WinSock2 库
target_link_libraries(${PROJECT_NAME} PRIVATE ws2_32)