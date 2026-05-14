cmake_minimum_required(VERSION 3.20)

project(brookesia_hal_simulator_host_test LANGUAGES CXX)

add_subdirectory(.. brookesia_hal_simulator)

add_executable(brookesia_hal_simulator_host_test
    main.cpp
)

target_link_libraries(brookesia_hal_simulator_host_test
    PRIVATE
        brookesia::hal_simulator
)

set_target_properties(brookesia_hal_simulator_host_test PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
)

enable_testing()
add_test(
    NAME brookesia_hal_simulator_smoke
    COMMAND brookesia_hal_simulator_host_test
)
