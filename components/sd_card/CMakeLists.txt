idf_component_register(
    SRCS "sd_card.c"
    INCLUDE_DIRS "include"
    REQUIRES driver vfs fatfs sdmmc
    PRIV_REQUIRES main
)
