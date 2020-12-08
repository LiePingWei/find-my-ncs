zephyr_library()
zephyr_library_include_directories(.)
zephyr_library_sources_ifdef(CONFIG_FMN_ADK fmna_gatt_fmns.c)
