/* Copyright (c) 2024 Joshua White
 * SPDX-License-Identifier: Apache-2.0
 */
#include <pub_sub/msg_alloc.h>
#include <pub_sub/msg_alloc_pool.h>
#include <pub_sub/msg_alloc_mem_slab.h>
#include <zephyr/ztest.h>
#include <stdlib.h>
#include <helpers.h>

#define ALLOC_0_MSG_SIZE 2
#define ALLOC_1_MSG_SIZE 4
#define ALLOC_2_MSG_SIZE 8
#define ALLOC_3_MSG_SIZE 16
#define ALLOC_4_MSG_SIZE 32

#define ALLOC_0_MSG_COUNT 64
#define ALLOC_1_MSG_COUNT 32
#define ALLOC_2_MSG_COUNT 16
#define ALLOC_3_MSG_COUNT 8
#define ALLOC_4_MSG_COUNT 4

PUB_SUB_MEM_SLAB_ALLOCATOR_DEFINE_STATIC(static_mem_slab_allocator_0, ALLOC_0_MSG_SIZE,
					 ALLOC_0_MSG_COUNT);
PUB_SUB_MEM_SLAB_ALLOCATOR_DEFINE_STATIC(static_mem_slab_allocator_1, ALLOC_1_MSG_SIZE,
					 ALLOC_1_MSG_COUNT);
PUB_SUB_MEM_SLAB_ALLOCATOR_DEFINE_STATIC(static_mem_slab_allocator_2, ALLOC_2_MSG_SIZE,
					 ALLOC_2_MSG_COUNT);
PUB_SUB_MEM_SLAB_ALLOCATOR_DEFINE_STATIC(static_mem_slab_allocator_3, ALLOC_3_MSG_SIZE,
					 ALLOC_3_MSG_COUNT);
PUB_SUB_MEM_SLAB_ALLOCATOR_DEFINE_STATIC(static_mem_slab_allocator_4, ALLOC_4_MSG_SIZE,
					 ALLOC_4_MSG_COUNT);

PUB_SUB_ALLOC_POOL_STATIC_DEFINE(static_alloc_pool, 5);

static void *alloc_pool_suite_setup(void)
{
	pub_sub_alloc_pool_add_alloc(&static_alloc_pool, &static_mem_slab_allocator_4,
				     ALLOC_4_MSG_SIZE);
	pub_sub_alloc_pool_add_alloc(&static_alloc_pool, &static_mem_slab_allocator_3,
				     ALLOC_3_MSG_SIZE);
	pub_sub_alloc_pool_add_alloc(&static_alloc_pool, &static_mem_slab_allocator_2,
				     ALLOC_2_MSG_SIZE);
	pub_sub_alloc_pool_add_alloc(&static_alloc_pool, &static_mem_slab_allocator_1,
				     ALLOC_1_MSG_SIZE);
	pub_sub_alloc_pool_add_alloc(&static_alloc_pool, &static_mem_slab_allocator_0,
				     ALLOC_0_MSG_SIZE);
	return NULL;
}

static void alloc_pool_after_test(void *fixture)
{
	for (size_t i = 0; i < static_alloc_pool.len; i++) {
		reset_mem_slab_allocator(static_alloc_pool.allocators[i]);
	}
}

ZTEST(alloc_pool, test_alloc_pool_add)
{
	zassert_equal(static_alloc_pool.len, 5);
	zassert_equal(static_alloc_pool.max_len, 5);
	zassert_equal_ptr(static_alloc_pool.allocators[0], &static_mem_slab_allocator_0);
	zassert_equal_ptr(static_alloc_pool.allocators[1], &static_mem_slab_allocator_1);
	zassert_equal_ptr(static_alloc_pool.allocators[2], &static_mem_slab_allocator_2);
	zassert_equal_ptr(static_alloc_pool.allocators[3], &static_mem_slab_allocator_3);
	zassert_equal_ptr(static_alloc_pool.allocators[4], &static_mem_slab_allocator_4);

	zassert_not_ok(pub_sub_alloc_pool_add_alloc(
		&static_alloc_pool, &static_mem_slab_allocator_0, ALLOC_0_MSG_SIZE));
}

ZTEST(alloc_pool, test_alloc_pool_max_msgs)
{
	size_t msg_sizes[] = {
		ALLOC_0_MSG_SIZE, ALLOC_1_MSG_SIZE, ALLOC_2_MSG_SIZE,
		ALLOC_3_MSG_SIZE, ALLOC_4_MSG_SIZE,
	};
	size_t msg_counts[] = {
		ALLOC_0_MSG_COUNT, ALLOC_1_MSG_COUNT, ALLOC_2_MSG_COUNT,
		ALLOC_3_MSG_COUNT, ALLOC_4_MSG_COUNT,
	};
	ARRAY_FOR_EACH(msg_sizes, i) {
		for (size_t j = 0; j < msg_counts[i]; j++) {
			void *msg = pub_sub_alloc_pool_new(&static_alloc_pool, 123, msg_sizes[i],
							   K_NO_WAIT);
			zassert_not_null(msg);
		}
		void *msg =
			pub_sub_alloc_pool_new(&static_alloc_pool, 123, msg_sizes[i], K_NO_WAIT);
		zassert_is_null(msg);
	}
}

ZTEST(alloc_pool, test_alloc_pool_new_min_msgs)
{
	size_t msg_sizes[] = {
		ALLOC_3_MSG_SIZE + 1,
		ALLOC_2_MSG_SIZE + 1,
		ALLOC_1_MSG_SIZE + 1,
		ALLOC_0_MSG_SIZE + 1,
		0,
	};
	size_t msg_counts[] = {
		ALLOC_4_MSG_COUNT, ALLOC_3_MSG_COUNT, ALLOC_2_MSG_COUNT,
		ALLOC_1_MSG_COUNT, ALLOC_0_MSG_COUNT,
	};
	ARRAY_FOR_EACH(msg_sizes, i) {
		for (size_t j = 0; j < msg_counts[i]; j++) {
			void *msg = pub_sub_alloc_pool_new(&static_alloc_pool, 123, msg_sizes[i],
							   K_NO_WAIT);
			zassert_not_null(msg);
		}
		void *msg =
			pub_sub_alloc_pool_new(&static_alloc_pool, 123, msg_sizes[i], K_NO_WAIT);
		zassert_is_null(msg);
	}
}

ZTEST_SUITE(alloc_pool, NULL, alloc_pool_suite_setup, NULL, alloc_pool_after_test, NULL);