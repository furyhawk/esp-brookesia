set(COMPONENT_DIR ${CMAKE_CURRENT_SOURCE_DIR})
set(COMPONENT_SRC_DIR ${COMPONENT_DIR}/src)
set(COMPONENT_INCLUDE_DIRS ${COMPONENT_DIR}/include)
set(COMPONENT_PRIVATE_INCLUDE_DIRS ${COMPONENT_SRC_DIR})
set(COMPONENT_REQUIRES brookesia_hal_interface)
set(COMPONENT_PRIV_REQUIRES "")
set(COMPONENT_SRCS_C "")
set(COMPONENT_SRCS_CPP "")
set(COMPONENT_SRCS_C_COMPILE_FLAGS "")
set(COMPONENT_SRCS_CPP_COMPILE_FLAGS "")
set(COMPONENT_SRCS_COMPILE_OPTIONS "")

#
# Service
#
file(GLOB SERVICE_SRCS_C ${COMPONENT_SRC_DIR}/*.c)
file(GLOB SERVICE_SRCS_CPP ${COMPONENT_SRC_DIR}/*.cpp)
list(APPEND COMPONENT_SRCS_C ${SERVICE_SRCS_C})
list(APPEND COMPONENT_SRCS_CPP ${SERVICE_SRCS_CPP})

if(ESP_PLATFORM)
    #
    # ESP Platform
    #
    idf_component_register(
        SRCS ${COMPONENT_SRCS_C} ${COMPONENT_SRCS_CPP}
        INCLUDE_DIRS ${COMPONENT_INCLUDE_DIRS}
        PRIV_INCLUDE_DIRS ${COMPONENT_PRIVATE_INCLUDE_DIRS}
        REQUIRES ${COMPONENT_REQUIRES}
        PRIV_REQUIRES ${COMPONENT_PRIV_REQUIRES}
    )
    if(CONFIG_BROOKESIA_SERVICE_WIFI_ENABLE_AUTO_REGISTER)
        set(BROOKESIA_SERVICE_WIFI_PLUGIN_SYMBOL service_wifi_symbol)
        target_compile_definitions(${COMPONENT_LIB} PRIVATE
            BROOKESIA_SERVICE_WIFI_PLUGIN_SYMBOL=${BROOKESIA_SERVICE_WIFI_PLUGIN_SYMBOL}
        )
        target_link_libraries(${COMPONENT_LIB} PRIVATE "-u ${BROOKESIA_SERVICE_WIFI_PLUGIN_SYMBOL}")
    endif()

    include(package_manager)
    cu_pkg_define_version(${CMAKE_CURRENT_LIST_DIR})
else()
    #
    # PC Platform
    #
    if(NOT TARGET brookesia_lib_utils)
        add_subdirectory(
            ${COMPONENT_DIR}/../../utils/brookesia_lib_utils
            ${CMAKE_BINARY_DIR}/brookesia_lib_utils
        )
    endif()
    if((NOT TARGET brookesia_lib_utils) AND TARGET brookesia_lib_utils_impl)
        add_library(brookesia_lib_utils ALIAS brookesia_lib_utils_impl)
    endif()
    if(NOT TARGET brookesia_service_manager)
        add_subdirectory(
            ${COMPONENT_DIR}/../brookesia_service_manager
            ${CMAKE_BINARY_DIR}/brookesia_service_manager
        )
    endif()
    if(NOT TARGET brookesia::service_manager)
        add_library(brookesia::service_manager ALIAS brookesia_service_manager)
    endif()
    if(NOT TARGET brookesia::hal_interface)
        add_subdirectory(
            ${COMPONENT_DIR}/../../hal/brookesia_hal_interface
            ${CMAKE_BINARY_DIR}/brookesia_hal_interface
        )
    endif()
    if(NOT TARGET brookesia::service_helper)
        add_subdirectory(
            ${COMPONENT_DIR}/../brookesia_service_helper
            ${CMAKE_BINARY_DIR}/brookesia_service_helper
        )
    endif()

    find_package(Boost REQUIRED COMPONENTS thread system chrono json)

    set(COMPONENT_LIB brookesia_service_wifi)
    add_library(${COMPONENT_LIB} STATIC
        ${COMPONENT_SRCS_C}
        ${COMPONENT_SRCS_CPP}
    )
    target_compile_features(${COMPONENT_LIB} PUBLIC cxx_std_23)
    target_include_directories(${COMPONENT_LIB}
        PUBLIC
            ${COMPONENT_INCLUDE_DIRS}
            ${Boost_INCLUDE_DIRS}
        PRIVATE
            ${COMPONENT_PRIVATE_INCLUDE_DIRS}
    )
    target_link_libraries(${COMPONENT_LIB}
        PUBLIC
            brookesia_service_manager
            brookesia::service_helper
            brookesia::hal_interface
            brookesia::lib_utils
            Boost::thread
            Boost::system
            Boost::chrono
            Boost::json
    )
    add_library(brookesia::service_wifi ALIAS ${COMPONENT_LIB})
endif()

#
# Compile Options
#
target_compile_options(${COMPONENT_LIB}
    PUBLIC
        ${COMPONENT_SRCS_COMPILE_OPTIONS}
)
set_source_files_properties(${COMPONENT_SRCS_C}
    PROPERTIES
        COMPILE_FLAGS "${COMPONENT_SRCS_C_COMPILE_FLAGS}"
)
set_source_files_properties(${COMPONENT_SRCS_CPP}
    PROPERTIES
        COMPILE_FLAGS "${COMPONENT_SRCS_CPP_COMPILE_FLAGS}"
)
