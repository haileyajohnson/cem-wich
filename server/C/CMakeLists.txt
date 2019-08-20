cmake_minimum_required(VERSION 3.0)

project (cem-web VERSION 0.1)

include(GenerateExportHeader)

########### CEM BMI library ###############

set (BUILD_SHARED_LIBS ON)

include_directories(${CMAKE_CURRENT_SOURCE_DIR})

set(bmicem_sources bmi/bmi_cem.c cem/cem.c cem/BeachGrid.c cem/BeachNode.c cem/sedtrans.c cem/WaveClimate.c cem/utils.c)

add_library(bmi_cem ${bmicem_sources})
add_library(bmi_cem-static STATIC ${bmicem_sources})
set_target_properties(bmi_cem-static PROPERTIES OUTPUT_NAME bmi_cem)

if(NOT MSVC)
	target_link_libraries(bmi_cem m)
endif()

########### CEM main ###############
#set (cem_sources main.c)
#add_executable (cem ${cem_sources})
#target_link_libraries(cem bmi_cem-static m)

########### Install files ###############

#install(PROGRAMS ${CMAKE_CURRENT_BINARY_DIR}/cem DESTINATION bin)
install(FILES bmi/bmi.h bmi/bmi_cem.h DESTINATION include/cem)
install(TARGETS bmi_cem DESTINATION lib)
install(TARGETS bmi_cem-static DESTINATION lib)
 