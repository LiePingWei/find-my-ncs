zephyr_library()

zephyr_library_include_directories(.)
# Expose subsys directory from Zephyr to access internal headers
zephyr_library_include_directories(${ZEPHYR_BASE}/subsys/.)

zephyr_library_sources(fmna_adv.c)
zephyr_library_sources(fmna_conn.c)
zephyr_library_sources(fmna_gatt_ais.c)
zephyr_library_sources(fmna_gatt_fmns.c)
zephyr_library_sources(fmna_gatt_pkt_manager.c)
zephyr_library_sources(fmna_keys.c)
zephyr_library_sources(fmna_pair.c)
zephyr_library_sources(fmna_product_plan.c)
zephyr_library_sources(fmna_serial_number.c)
zephyr_library_sources(fmna_sound.c)
zephyr_library_sources(fmna_state.c)
zephyr_library_sources(fmna_storage.c)
zephyr_library_sources(fmna.c)

add_subdirectory(crypto)
add_subdirectory(events)
