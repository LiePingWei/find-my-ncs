zephyr_include_directories(.)
zephyr_library()

zephyr_library_sources(
    CoreUARPAccessory.c
    CoreUARPPlatformAccessory.c
    CoreUARPPlatformZephyr.c
    CoreUARPUtils.c
    )
