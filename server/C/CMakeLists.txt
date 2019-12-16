cmake_minimum_required(VERSION 3.0)

project (cem-web VERSION 0.1)

########### CEM BMI library #############
set (BUILD_SHARED_LIBS ON)
include_directories(${CMAKE_CURRENT_SOURCE_DIR})

set(cem_sources py_interface/cem_interface.c cem/cem.c cem/BeachGrid.c cem/BeachNode.c cem/sedtrans.c cem/WaveClimate.c cem/utils.c cem/config.h)
add_library(py_cem ${cem_sources})
SET_TARGET_PROPERTIES(py_cem PROPERTIES PREFIX "")

###### link libm if not using MSVC #######
if(NOT MSVC)
	target_link_libraries(py_cem m)
endif()

######## generate exports for MSVC#######
include(GenerateExportHeader)
GENERATE_EXPORT_HEADER (py_cem
	BASE_NAME py_cem
	EXPORT_MACRO_NAME cem_EXPORT
	EXPORT_FILE_NAME ${CMAKE_CURRENT_SOURCE_DIR}/py_interface/cem_EXPORTS.h
	STATIC_DEFINE bmi_BUILT_AS_STATIC)

########### Install files ###############
#install(TARGETS bmi_cem DESTINATION lib)
 