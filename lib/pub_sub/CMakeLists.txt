if (CONFIG_PUB_SUB)
    zephyr_sources(
        pub_sub.c
        delayable_msg.c
        msg_alloc.c
        msg_alloc_mem_slab.c
        msg_alloc_pool.c
    )
    zephyr_sources_ifdef(CONFIG_PUB_SUB_RUNTIME_SUBSCRIBERS runtime_subscriber.c)

    zephyr_linker_sources(SECTIONS pub_sub_rom.ld)
    zephyr_linker_sources(DATA_SECTIONS pub_sub_ram.ld)
endif()