/* Copyright (c) 2024 Joshua White
 * SPDX-License-Identifier: Apache-2.0
 */
#include <pub_sub/msg_alloc.h>
#include <pub_sub/static_msg.h>

void pub_sub_alloc_init_allocators(struct pub_sub_allocators *allocators)
{
	__ASSERT(allocators != NULL, "");
	allocators->num_allocators = 0;
	for (size_t i = 0; i < CONFIG_PUB_SUB_ALLOC_MAX_NUM; i++) {
		allocators->allocators[i] = NULL;
	}
}

int pub_sub_alloc_add(struct pub_sub_allocators *allocators, struct pub_sub_allocator *allocator)
{
	__ASSERT(allocators != NULL, "");
	__ASSERT(allocator != NULL, "");
	int ret = -ENOMEM;
	if (allocators->num_allocators < CONFIG_PUB_SUB_ALLOC_MAX_NUM) {
		// Insert into allocators array so that it is sorted by max msg size
		struct pub_sub_allocator *tmp;
		for (size_t i = 0; i < allocators->num_allocators; i++) {
			tmp = allocators->allocators[i];
			if (allocator->max_msg_size < tmp->max_msg_size) {
				allocators->allocators[i] = allocator;
				allocator = tmp;
			}
		}
		allocators->allocators[allocators->num_allocators] = allocator;
		allocators->num_allocators++;
		ret = 0;
	}
	return ret;
}

void *pub_sub_alloc_new(struct pub_sub_allocators *allocators, uint16_t msg_id,
			size_t msg_size_bytes, k_timeout_t timeout)
{
	__ASSERT(allocators != NULL, "");
	void *msg = NULL;
	for (size_t i = 0; i < allocators->num_allocators; i++) {
		struct pub_sub_allocator *allocator = allocators->allocators[i];
		if (allocator->max_msg_size >= msg_size_bytes) {
			msg = allocator->allocate(allocator->impl, msg_size_bytes, timeout);
			if (msg != NULL) {
				pub_sub_msg_init(msg, msg_id, i);
			}
			break;
		}
	}
	return msg;
}

void pub_sub_alloc_release(struct pub_sub_allocators *allocators, const void *msg)
{
	__ASSERT(allocators != NULL, "");
	__ASSERT(msg != NULL, "");
	uint8_t prev_ref_cnt = pub_sub_msg_dec_ref_cnt(msg);
	if (prev_ref_cnt == 1) {
		uint8_t allocator_id = pub_sub_msg_get_alloc_id(msg);
		if (allocator_id < allocators->num_allocators) {
			struct pub_sub_allocator *allocator = allocators->allocators[allocator_id];
			allocator->free(allocator->impl, msg);
		} else if (allocator_id == PUB_SUB_ALLOC_ID_CALLBACK_MSG) {
			pub_sub_free_callback_msg(msg);
		}
	}
}
