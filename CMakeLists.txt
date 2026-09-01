cmake_minimum_required(VERSION 3.22)
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
set(EXTRA_COMPONENT_DIRS
    "components"
)
idf_build_set_property(MINIMAL_BUILD ON)

project(speaker)
