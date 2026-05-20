idf_component_register(
    SRC_DIRS "."
    INCLUDE_DIRS "."
    PRIV_REQUIRES unity gen_bmgr_codes esp_psram
    WHOLE_ARCHIVE
)

littlefs_create_partition_image(littlefs_data ../littlefs FLASH_IN_PROJECT)
