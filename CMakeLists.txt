cmake_minimum_required(VERSION 3.10)
project(sst-voicemanager VERSION 0.5 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)

add_library(${PROJECT_NAME} INTERFACE)
target_include_directories(${PROJECT_NAME} INTERFACE include)
target_compile_definitions(${PROJECT_NAME} INTERFACE _USE_MATH_DEFINES=1)

set(bldtest FALSE)
if (DEFINED SST_VOICEMANAGER_BUILD_TESTS)
  set(bldtest ${SST_VOICEMANAGER_BUILD_TESTS})
endif()
if (PROJECT_IS_TOP_LEVEL)
  set(bldtest TRUE)
endif()

if (${bldtest})
    add_executable(${PROJECT_NAME}-test
            # tests/main.cpp
            tests/basic.cpp

            tests/basic_poly.cpp
            tests/stealing_maxvoice.cpp
            tests/mono_playback.cpp
            tests/mpe_tests.cpp
            tests/routing_params.cpp
            tests/stealing_groups.cpp
            tests/legato_playback.cpp
            tests/piano_mode.cpp

            libs/catch2/catch_amalgamated.cpp
        )

    if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
        target_compile_options(${PROJECT_NAME}-test PRIVATE -Wall -Wextra -Wno-unused-parameter -Wpedantic -Werror)
    endif()
    target_link_libraries(${PROJECT_NAME}-test PRIVATE ${PROJECT_NAME})
    target_include_directories(${PROJECT_NAME}-test PRIVATE libs/catch2)
endif ()
