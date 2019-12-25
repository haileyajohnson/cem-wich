cmake_minimum_required(VERSION 3.0)

project (cem VERSION 0.1)

########### CEM BMI library ###############
set (BUILD_SHARED_LIBS ON)
include_directories(${CMAKE_CURRENT_SOURCE_DIR})

set(cem_sources cem/cem.c cem/utils.c)
add_library(test_cem ${cem_sources})
SET_TARGET_PROPERTIES(test_cem PROPERTIES PREFIX "")

###### link libm if not using MSVC #######
if(NOT MSVC)
	target_link_libraries(test_cem m)
endif()

######## generate exports for MSVC#######
include(GenerateExportHeader)
GENERATE_EXPORT_HEADER (test_cem
	BASE_NAME test_cem
	EXPORT_MACRO_NAME cem_EXPORT
	EXPORT_FILE_NAME ${CMAKE_CURRENT_SOURCE_DIR}/cem/cem_EXPORTS.h)
