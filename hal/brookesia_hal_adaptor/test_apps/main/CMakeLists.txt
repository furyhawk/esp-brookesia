idf_component_register(
    SRC_DIRS "."
    INCLUDE_DIRS "."
    PRIV_REQUIRES unity gen_bmgr_codes esp_psram
    WHOLE_ARCHIVE
)

if(DEFINED ENV{CI_TEST_WIFI_2_4G_AP1_SSID})
    target_compile_definitions(${COMPONENT_LIB} PRIVATE
        HAL_ADAPTOR_TEST_WIFI_SSID="$ENV{CI_TEST_WIFI_2_4G_AP1_SSID}"
    )
endif()

if(DEFINED ENV{CI_TEST_WIFI_2_4G_AP1_PSW})
    target_compile_definitions(${COMPONENT_LIB} PRIVATE
        HAL_ADAPTOR_TEST_WIFI_PSW="$ENV{CI_TEST_WIFI_2_4G_AP1_PSW}"
    )
endif()

littlefs_create_partition_image(littlefs_data ../littlefs FLASH_IN_PROJECT)
