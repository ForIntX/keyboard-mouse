cmake_minimum_required(VERSION 3.16)
project(keyboard_mouse VERSION 1.5.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(keyboard_mouse_core
    src/config.cpp
    src/controller.cpp
    src/linux_input.cpp
)
target_include_directories(keyboard_mouse_core PUBLIC include)
target_compile_options(keyboard_mouse_core PRIVATE -Wall -Wextra -Wpedantic)

add_executable(keyboard-mouse src/main.cpp)
target_link_libraries(keyboard-mouse PRIVATE keyboard_mouse_core)
target_compile_options(keyboard-mouse PRIVATE -Wall -Wextra -Wpedantic)
target_compile_definitions(keyboard-mouse PRIVATE
    KEYBOARD_MOUSE_VERSION="${PROJECT_VERSION}"
)

include(CTest)
if(BUILD_TESTING)
    add_executable(keyboard_mouse_tests tests/controller_test.cpp)
    target_link_libraries(keyboard_mouse_tests PRIVATE keyboard_mouse_core)
    target_compile_options(keyboard_mouse_tests PRIVATE -Wall -Wextra -Wpedantic)
    add_test(NAME keyboard_mouse_tests COMMAND keyboard_mouse_tests)
endif()

install(TARGETS keyboard-mouse RUNTIME DESTINATION bin)
