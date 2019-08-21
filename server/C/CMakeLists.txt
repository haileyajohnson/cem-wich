cmake_minimum_required(VERSION 3.0)

project (cem-web VERSION 0.1)

########### CEM BMI library #############
set (BUILD_SHARED_LIBS ON)
include_directories(${CMAKE_CURRENT_SOURCE_DIR})

set(bmicem_sources bmi/bmi_cem.c cem/cem.c cem/BeachGrid.c cem/BeachNode.c cem/sedtrans.c cem/WaveClimate.c cem/utils.c)
add_library(bmi_cem ${bmicem_sources})

###### link libm if not using MSVC #######
if(NOT MSVC)
	target_link_libraries(bmi_cem m)
endif()

######## generate exports for MSVC#######
include(GenerateExportHeader)
GENERATE_EXPORT_HEADER (bmi_cem
	BASE_NAME bmi_cem
	EXPORT_MACRO_NAME bmi_EXPORT
	EXPORT_FILE_NAME ${CMAKE_CURRENT_SOURCE_DIR}/bmi/bmi_EXPORTS.h
	STATIC_DEFINE bmi_BUILT_AS_STATIC)

########### Install files ###############
#install(TARGETS bmi_cem DESTINATION lib)
 