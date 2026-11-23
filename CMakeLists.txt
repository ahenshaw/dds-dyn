cmake_minimum_required(VERSION 3.20)
project(dds_dyn LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Python 3.9 REQUIRED COMPONENTS Interpreter Development.Module ${SKBUILD_SABI_COMPONENT})
find_package(nanobind CONFIG REQUIRED)

# Static Fast DDS from build_deps.py; point CMAKE_PREFIX_PATH at its prefix.
find_package(fastcdr 2 REQUIRED)
find_package(fastdds 3 REQUIRED)

# Stable ABI (abi3) when building for Python >= 3.12; one wheel covers 3.12+.
nanobind_add_module(_core STABLE_ABI NB_STATIC src/core.cpp)
target_link_libraries(_core PRIVATE fastdds fastcdr)
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    # Keep the statically linked Fast DDS symbols private to the module.
    target_link_options(_core PRIVATE "-Wl,--exclude-libs,ALL")
endif()
install(TARGETS _core LIBRARY DESTINATION dds_dyn RUNTIME DESTINATION dds_dyn)

# The wheel redistributes all of the above in binary form, so it carries their licenses.
install(FILES LICENSE DESTINATION dds_dyn/licenses RENAME dds-dyn.txt)
install(DIRECTORY licenses/ DESTINATION dds_dyn/licenses FILES_MATCHING PATTERN "*.txt" PATTERN "*.md")
