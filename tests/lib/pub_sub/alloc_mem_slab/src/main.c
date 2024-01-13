/* Copyright (c) 2024 Joshua White
 * SPDX-License-Identifier: Apache-2.0
 */
#include <pub_sub/msg_alloc.h>
#include <pub_sub/msg_alloc_mem_slab.h>
#include <zephyr/ztest.h>
#include <stdlib.h>
#include <helpers.h>

struct static_allocator_info {
	struct pub_sub_allocator *allocator;
	size_t num_msgs;
	size_t msg_size;
};

PUB_SUB_MEM_SLAB_ALLOCATOR_DEFINE_STATIC(static_mem_slab_allocator_0, 2, 64);
PUB_SUB_MEM_SLAB_ALLOCATOR_DEFINE_STATIC(static_mem_slab_allocator_1, 4, 32);
PUB_SUB_MEM_SLAB_ALLOCATOR_DEFINE_STATIC(static_mem_slab_allocator_2, 8, 16);
PUB_SUB_MEM_SLAB_ALLOCATOR_DEFINE_STATIC(static_mem_slab_allocator_3, 16, 8);
PUB_SUB_MEM_SLAB_ALLOCATOR_DEFINE_STATIC(static_mem_slab_allocator_4, 32, 4);

const struct static_allocator_info static_allocator_info[5] = {
	{
		.allocator = &static_mem_slab_allocator_0,
		.msg_size = 2,
		.num_msgs = 64,
	},
	{
		.allocator = &static_mem_slab_allocator_1,
		.msg_size = 4,
		.num_msgs = 32,
	},
	{
		.allocator = &static_mem_slab_allocator_2,
		.msg_size = 8,
		.num_msgs = 16,
	},
	{
		.allocator = &static_mem_slab_allocator_3,
		.msg_size = 16,
		.num_msgs = 8,
	},
	{
		.allocator = &static_mem_slab_allocator_4,
		.msg_size = 32,
		.num_msgs = 4,
	},
};

struct mem_slab_fixture {
	struct pub_sub_allocator **allocators;
	size_t *allocator_msg_sizes;
	size_t *allocator_num_msgs;
	size_t num_allocators;
};

static void *mem_slab_suite_setup(void)
{
	struct mem_slab_fixture *test_fixture = malloc(sizeof(struct mem_slab_fixture));
	// Create the maximum number of mem_slab allocators of different msg sizes and number of
	// msgs
	test_fixture->num_allocators = CONFIG_PUB_SUB_RUNTIME_ALLOCATORS_MAX_NUM;
	test_fixture->allocators =
		malloc(sizeof(*test_fixture->allocators) * test_fixture->num_allocators);
	test_fixture->allocator_msg_sizes =
		malloc(sizeof(*test_fixture->allocator_msg_sizes) * test_fixture->num_allocators);
	test_fixture->allocator_num_msgs =
		malloc(sizeof(*test_fixture->allocator_num_msgs) * test_fixture->num_allocators);
	for (size_t i = 0; i < test_fixture->num_allocators; i++) {
		test_fixture->allocator_msg_sizes[i] = 2 << i;
		test_fixture->allocator_num_msgs[i] = 64 >> i;
		test_fixture->allocators[i] = malloc_mem_slab_allocator(
			test_fixture->allocator_msg_sizes[i], test_fixture->allocator_num_msgs[i]);
		pub_sub_add_runtime_allocator(test_fixture->allocators[i]);
	}
	return test_fixture;
}

static void mem_slab_after_test(void *fixture)
{
	struct mem_slab_fixture *test_fixture = fixture;
	for (size_t i = 0; i < test_fixture->num_allocators; i++) {
		reset_mem_slab_allocator(test_fixture->allocators[i]);
	}
	ARRAY_FOR_EACH(static_allocator_info, i) {
		reset_mem_slab_allocator(static_allocator_info[i].allocator);
	}
}

static void mem_slab_suite_teardown(void *fixture)
{
	struct mem_slab_fixture *test_fixture = fixture;
	for (size_t i = 0; i < test_fixture->num_allocators; i++) {
		free_mem_slab_allocator(test_fixture->allocators[i]);
	}
	free(test_fixture->allocators);
	free(test_fixture->allocator_msg_sizes);
	free(test_fixture->allocator_num_msgs);
	free(test_fixture);
}

ZTEST_F(mem_slab, test_runtime_allocator_num_msgs)
{
	void *msg;
	// Test allocating the maximum number of msgs from each allocator
	for (size_t i = 0; i < fixture->num_allocators; i++) {
		struct pub_sub_allocator *allocator = fixture->allocators[i];
		for (size_t j = 0; j < fixture->allocator_num_msgs[i]; j++) {
			msg = pub_sub_new_msg(allocator, 0, fixture->allocator_msg_sizes[i],
					      K_NO_WAIT);
			zassert_not_null(msg,
					 "Allocator index: %u, allocator num msgs: %u, allocation "
					 "attempt: %u",
					 i, fixture->allocator_num_msgs[i], j);
			uint8_t alloc_id = pub_sub_msg_get_alloc_id(msg);
			zassert_equal(alloc_id, i + PUB_SUB_ALLOC_ID_RUNTIME_OFFSET,
				      "alloc_id: %u, i: %u", alloc_id, i);
		}
		msg = pub_sub_new_msg(allocator, 0, fixture->allocator_msg_sizes[i], K_NO_WAIT);
		zassert_is_null(
			msg, "Allocator index: %u, allocator num msgs: %u, allocation attempt: %u",
			i, fixture->allocator_num_msgs[i], fixture->allocator_num_msgs[i] + 1);
	}
}

ZTEST_F(mem_slab, test_static_allocator_num_msgs)
{
	void *msg;
	// Test allocating the maximum number of msgs from each allocator
	ARRAY_FOR_EACH(static_allocator_info, i) {
		struct pub_sub_allocator *allocator = static_allocator_info[i].allocator;
		for (size_t j = 0; j < static_allocator_info[i].num_msgs; j++) {
			msg = pub_sub_new_msg(allocator, 0, static_allocator_info[i].msg_size,
					      K_NO_WAIT);
			zassert_not_null(msg,
					 "Allocator index: %u, allocator num msgs: %u, allocation "
					 "attempt: %u",
					 i, static_allocator_info[i].num_msgs, j);
			uint8_t alloc_id = pub_sub_msg_get_alloc_id(msg);
			zassert_equal(alloc_id, i, "alloc_id: %u, i: %u", alloc_id, i);
		}
		msg = pub_sub_new_msg(allocator, 0, static_allocator_info[i].msg_size, K_NO_WAIT);
		zassert_is_null(msg,
				"Allocator index: %u, allocator num msgs: %u, allocation "
				"attempt: %u",
				i, static_allocator_info[i].num_msgs,
				static_allocator_info[i].num_msgs + 1);
	}
}

ZTEST_F(mem_slab, test_runtime_allocator_ref_counts)
{
	for (size_t i = 0; i < fixture->num_allocators; i++) {
		void *msg = NULL;
		struct pub_sub_allocator *allocator = fixture->allocators[i];
		const size_t msg_size = fixture->allocator_msg_sizes[i];
		const size_t num_msgs = fixture->allocator_num_msgs[i];

		// Allocate all of the msgs
		for (size_t i = 0; i < num_msgs; i++) {
			msg = pub_sub_new_msg(allocator, 0, msg_size, K_NO_WAIT);
			zassert_not_null(msg);
		}
		void *new_msg = pub_sub_new_msg(allocator, 0, msg_size, K_NO_WAIT);
		zassert_is_null(new_msg);

		// Decrement the ref count on the last msg returning it to the allocator
		pub_sub_release_msg(msg);
		// A msg can now be allocated
		msg = pub_sub_new_msg(allocator, 0, msg_size, K_NO_WAIT);
		zassert_not_null(msg);

		// Increment and decrement the ref count on the msg
		for (size_t i = 0; i < 10; i++) {
			pub_sub_msg_inc_ref_cnt(msg);
			pub_sub_release_msg(msg);
		}

		// The msg is still allocated and a new one can not be allocated
		new_msg = pub_sub_new_msg(allocator, 0, msg_size, K_NO_WAIT);
		zassert_is_null(new_msg);

		// Decrement the ref count on the last msg returning it to the allocator
		pub_sub_release_msg(msg);
		// A msg can now be allocated
		msg = pub_sub_new_msg(allocator, 0, msg_size, K_NO_WAIT);
		zassert_not_null(msg);
	}
}

ZTEST_F(mem_slab, test_static_allocator_ref_counts)
{
	ARRAY_FOR_EACH(static_allocator_info, i) {
		void *msg = NULL;
		struct pub_sub_allocator *allocator = static_allocator_info[i].allocator;
		const size_t msg_size = static_allocator_info[i].msg_size;
		const size_t num_msgs = static_allocator_info[i].num_msgs;

		// Allocate all of the msgs
		for (size_t i = 0; i < num_msgs; i++) {
			msg = pub_sub_new_msg(allocator, 0, msg_size, K_NO_WAIT);
			zassert_not_null(msg);
		}
		void *new_msg = pub_sub_new_msg(allocator, 0, msg_size, K_NO_WAIT);
		zassert_is_null(new_msg);

		// Decrement the ref count on the last msg returning it to the allocator
		pub_sub_release_msg(msg);
		// A msg can now be allocated
		msg = pub_sub_new_msg(allocator, 0, msg_size, K_NO_WAIT);
		zassert_not_null(msg);

		// Increment and decrement the ref count on the msg
		for (size_t i = 0; i < 10; i++) {
			pub_sub_msg_inc_ref_cnt(msg);
			pub_sub_release_msg(msg);
		}

		// The msg is still allocated and a new one can not be allocated
		new_msg = pub_sub_new_msg(allocator, 0, msg_size, K_NO_WAIT);
		zassert_is_null(new_msg);

		// Decrement the ref count on the last msg returning it to the allocator
		pub_sub_release_msg(msg);
		// A msg can now be allocated
		msg = pub_sub_new_msg(allocator, 0, msg_size, K_NO_WAIT);
		zassert_not_null(msg);
	}
}

ZTEST_F(mem_slab, test_allocator_add)
{
	// Test adding too many allocators, the maximum number has already been added so adding any
	// extra should fail
	struct pub_sub_allocator *allocator = malloc_mem_slab_allocator(1, 1);
	int ret = pub_sub_add_runtime_allocator(allocator);
	// Free the allocator because it will not get freed as part of the test teardown
	if (ret != 0) {
		free_mem_slab_allocator(allocator);
	}
	zassert_equal(ret, -ENOMEM);
}

ZTEST_SUITE(mem_slab, NULL, mem_slab_suite_setup, NULL, mem_slab_after_test,
	    mem_slab_suite_teardown);