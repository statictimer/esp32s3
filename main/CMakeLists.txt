idf_component_register(
    SRC_DIRS 
        "."
        "app"
    INCLUDE_DIRS 
        "."
        "app")

spiffs_create_partition_image(html ../html FLASH_IN_PROJECT)