idf_component_register(
    SRCS
        "main.c"
        "vfo_state.c"
        "si5351.c"
        "display.c"
        "graph.c"
        "dial.c"
        "ui_overlay.c"
        "encoder.c"
        "buttons.c"
        "nvs_storage.c"
    INCLUDE_DIRS
        "../include"
    REQUIRES
        nvs_flash driver esp_timer esp_event esp_system spi_flash)