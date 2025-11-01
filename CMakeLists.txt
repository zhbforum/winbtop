cmake_minimum_required(VERSION 3.20)
project(winbtop LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/src")

file(GLOB_RECURSE WINBTOP_SRC CONFIGURE_DEPENDS
    "${SRC_DIR}/*.cpp"
)

add_executable(winbtop ${WINBTOP_SRC})

source_group(TREE ${SRC_DIR} PREFIX "Source" FILES ${WINBTOP_SRC})

target_include_directories(winbtop PRIVATE
    ${SRC_DIR}
    ${SRC_DIR}/app
    ${SRC_DIR}/core
    ${SRC_DIR}/metrics
    ${SRC_DIR}/ui
)

add_custom_command(TARGET winbtop POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:winbtop>/themes"
  COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/themes" "$<TARGET_FILE_DIR:winbtop>/themes"
  COMMENT "Copying themes folder next to the binary"
)

set_target_properties(winbtop PROPERTIES WIN32_EXECUTABLE OFF)

target_compile_definitions(winbtop PRIVATE
  NOMINMAX
  WIN32_LEAN_AND_MEAN
  UNICODE
  _UNICODE
)

if (MSVC)
  target_compile_options(winbtop PRIVATE
    /W4 /permissive- /utf-8 /Zc:__cplusplus /EHsc /MP
  )
  target_compile_definitions(winbtop PRIVATE _CRT_SECURE_NO_WARNINGS)
else()
  target_compile_options(winbtop PRIVATE -Wall -Wextra -Wpedantic)
endif()

target_link_libraries(winbtop PRIVATE
  user32
  advapi32
  psapi
  pdh
  Shlwapi
  ntdll
)
