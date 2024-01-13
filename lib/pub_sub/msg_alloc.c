/* Copyright (c) 2024 Joshua White
 * SPDX-License-Identifier: Apache-2.0
 */
#include <pub_sub/msg_alloc.h>
#include <pub_sub/static_msg.h>

struct pub_sub_runtime_allocators {
	struct pub_sub_allocator *allocators[CONFIG_PUB_SUB_ALLOC_MAX_NUM];
	size_t num_allocators;
	struct k_mutex mutex;
};

static struct pub_sub_runtime_allocators g_runtime_allocators;

int pub_sub_add_runtime_allocator(struct pub_sub_allocator *allocator)
{
	__ASSERT(allocator != NULL, "");
	int ret = -ENOMEM;
	k_mutex_lock(&g_runtime_allocators.mutex, K_FOREVER);
	if (g_runtime_allocators.num_allocators < CONFIG_PUB_SUB_ALLOC_MAX_NUM) {
		uint8_t allocator_id = g_runtime_allocators.num_allocators;
		allocator->allocator_id = allocator_id;
		g_runtime_allocators.allocators[allocator_id] = allocator;
		g_runtime_allocators.num_allocators++;
		ret = 0;
	}
	k_mutex_unlock(&g_runtime_allocators.mutex);
	return ret;
}

void pub_sub_release_msg(const void *msg)
{
	__ASSERT(msg != NULL, "");
	uint8_t prev_ref_cnt = pub_sub_msg_dec_ref_cnt(msg);
	if (prev_ref_cnt == 1) {
		uint8_t allocator_id = pub_sub_msg_get_alloc_id(msg);
		if (allocator_id < g_runtime_allocators.num_allocators) {
			// Run time allocators can only be added and never removed so
			// we don't need to lock the mutex to find a run time allocator
			// from an allocator id as it can never change once assigned.
			struct pub_sub_allocator *allocator =
				g_runtime_allocators.allocators[allocator_id];
			allocator->free(allocator->impl, msg);
		} else if (allocator_id == PUB_SUB_ALLOC_ID_CALLBACK_MSG) {
			pub_sub_free_callback_msg(msg);
		}
	}
}

static int pub_sub_init_runtime_allocators(void)
{
	g_runtime_allocators.num_allocators = 0;
	for (size_t i = 0; i < CONFIG_PUB_SUB_ALLOC_MAX_NUM; i++) {
		g_runtime_allocators.allocators[i] = NULL;
	}
	k_mutex_init(&g_runtime_allocators.mutex);
	return 0;
}

SYS_INIT(pub_sub_init_runtime_allocators, POST_KERNEL, CONFIG_APPLICATION_INIT_PRIORITY);
