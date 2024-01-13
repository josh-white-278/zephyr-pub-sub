if (CONFIG_PUB_SUB)
    zephyr_sources(
        broker.c
        msg_alloc.c
        msg_alloc_mem_slab.c
        subscriber.c
    )

    zephyr_linker_sources(SECTIONS pub_sub.ld)
    zephyr_iterable_section(NAME pub_sub_allocator KVMA RAM_REGION GROUP RODATA_REGION SUBALIGN 4)
endif()