# The following lines of boilerplate have to be in your project's CMakeLists
# in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.16)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
# "Trim" the build. Include the minimal set of components, main, and anything it depends on.
idf_build_set_property(MINIMAL_BUILD ON)
project(test_brookesia_service_audio)

idf_component_get_property(gmf_video_lib espressif__gmf_video COMPONENT_LIB)
if(gmf_video_lib)
    target_compile_definitions(${gmf_video_lib} PRIVATE
        PPA_SRM_COLOR_MODE_YUV422=PPA_SRM_COLOR_MODE_YUV422_YUYV
    )
endif()
