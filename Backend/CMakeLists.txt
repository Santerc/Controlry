# 设置最低CMake版本要求
cmake_minimum_required(VERSION 3.11)

# 项目名称和版本
project(motor_control VERSION 1.0.0 LANGUAGES CXX)

# 设置C++标准
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# 编译器特定选项
if(MSVC)
    # MSVC 编译选项
    add_compile_options(/W4 /MP /utf-8)
    # 禁用特定警告
    add_compile_options(/wd4819 /wd4251)
else()
    # MinGW/其他编译器选项
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# 设置为控制台应用程序
if(MSVC)
    set(CMAKE_WIN32_EXECUTABLE FALSE)
    add_compile_definitions(_CONSOLE)
else()
    set(CMAKE_WIN32_EXECUTABLE FALSE)
endif()

# 字符集设置
if(MSVC)
    add_compile_definitions(UNICODE _UNICODE)
endif()

# 调试信息格式
if(MSVC)
    set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} /Zi")
else()
    set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -g")
endif()

# ImGui 源文件
set(IMGUI_DIR ${CMAKE_SOURCE_DIR}/third/imgui)
set(IMGUI_SOURCES
        ${IMGUI_DIR}/imgui.cpp
        ${IMGUI_DIR}/imgui_demo.cpp
        ${IMGUI_DIR}/imgui_draw.cpp
        ${IMGUI_DIR}/imgui_tables.cpp
        ${IMGUI_DIR}/imgui_widgets.cpp
        ${IMGUI_DIR}/backends/imgui_impl_win32.cpp
        ${IMGUI_DIR}/backends/imgui_impl_dx11.cpp
)

# 设置输出目录
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# 添加源文件
file(GLOB_RECURSE SOURCES "src/*.cpp" "example/*.cpp" "User/src/*.cpp" "Tools/src/*.cpp")
file(GLOB_RECURSE HEADERS "include/*.h" "include/*.hpp" "User/include/*.h" "User/include/*.hpp" "Tools/include/*.h")

# 创建可执行文件
add_executable(${PROJECT_NAME}
        ${SOURCES}
        ${HEADERS}
        User/src/PidController.cpp
        ${IMGUI_SOURCES}
)

# 添加头文件目录
target_include_directories(${PROJECT_NAME} PRIVATE
        include
        User
        Tools
        ${IMGUI_DIR}
        ${IMGUI_DIR}/backends)

# 添加 WinSock2 库
target_link_libraries(${PROJECT_NAME} PRIVATE ws2_32)

if(WIN32)
    target_link_libraries(${PROJECT_NAME} PRIVATE
            d3d11
            dxgi
            comctl32
            ws2_32
            dwmapi
            d3dcompiler
    )
endif()