/* Copyright (c) 2024 Joshua White
 * SPDX-License-Identifier: Apache-2.0
 */
#include <pub_sub/msg_alloc.h>
#include <pub_sub/msg_alloc_mem_slab.h>
#include <zephyr/ztest.h>
#include <stdlib.h>
#include <helpers.h>

struct mem_slab_fixture {
	struct pub_sub_allocators allocators;
	size_t *allocator_msg_sizes;
	size_t *allocator_num_msgs;
	size_t num_allocators;
};

static void *mem_slab_suite_setup(void)
{
	struct mem_slab_fixture *test_fixture = malloc(sizeof(struct mem_slab_fixture));
	pub_sub_alloc_init_allocators(&test_fixture->allocators);
	// Create three mem_slab allocators of different msg sizes and number of msgs
	test_fixture->num_allocators = 3;
	test_fixture->allocator_msg_sizes =
		malloc(sizeof(*test_fixture->allocator_msg_sizes) * test_fixture->num_allocators);
	test_fixture->allocator_num_msgs =
		malloc(sizeof(*test_fixture->allocator_num_msgs) * test_fixture->num_allocators);
	for (size_t i = 0; i < test_fixture->num_allocators; i++) {
		test_fixture->allocator_msg_sizes[i] = 8 << i;
		test_fixture->allocator_num_msgs[i] = 16 >> i;
	}
	return test_fixture;
}

static void mem_slab_before_test(void *fixture)
{
	struct mem_slab_fixture *test_fixture = fixture;
	struct pub_sub_allocators *allocators = &test_fixture->allocators;
	pub_sub_alloc_init_allocators(allocators);
	// Create the mem slab allocators for the test
	for (size_t i = 0; i < test_fixture->num_allocators; i++) {
		struct pub_sub_allocator *allocator = malloc_mem_slab_allocator(
			test_fixture->allocator_msg_sizes[i], test_fixture->allocator_num_msgs[i]);
		zassert_ok(pub_sub_alloc_add(allocators, allocator));
	}
}

static void mem_slab_after_test(void *fixture)
{
	struct mem_slab_fixture *test_fixture = fixture;
	struct pub_sub_allocators *allocators = &test_fixture->allocators;
	// A pub_sub allocator isn't really made to be torn down during normal operation
	// so we have to look inside and do it ourselves for test teardowns

	// Free all of the mem slab allocators
	for (size_t i = 0; i < allocators->num_allocators; i++) {
		free_mem_slab_allocator(allocators->allocators[i]);
		allocators->allocators[i] = NULL;
	}
	allocators->num_allocators = 0;
}

static void mem_slab_suite_teardown(void *fixture)
{
	struct mem_slab_fixture *test_fixture = fixture;
	free(test_fixture->allocator_msg_sizes);
	free(test_fixture->allocator_num_msgs);
	free(test_fixture);
}

ZTEST_F(mem_slab, test_allocator_num_msgs)
{
	struct pub_sub_allocators *allocators = &fixture->allocators;
	void *msg;
	// Test allocating the maximum number of msgs from each allocator
	for (size_t i = 0; i < fixture->num_allocators; i++) {
		for (size_t j = 0; j < fixture->allocator_num_msgs[i]; j++) {
			msg = pub_sub_alloc_new(allocators, 0, fixture->allocator_msg_sizes[i],
						K_NO_WAIT);
			zassert_not_null(msg,
					 "Allocator index: %u, allocator num msgs: %u, allocation "
					 "attempt: %u",
					 i, fixture->allocator_num_msgs[i], j);
			zassert_equal(pub_sub_msg_get_alloc_id(msg), i);
		}
		msg = pub_sub_alloc_new(allocators, 0, fixture->allocator_msg_sizes[i], K_NO_WAIT);
		zassert_is_null(
			msg, "Allocator index: %u, allocator num msgs: %u, allocation attempt: %u",
			i, fixture->allocator_num_msgs[i], fixture->allocator_num_msgs[i] + 1);
	}
}

ZTEST_F(mem_slab, test_allocator_msg_sizes)
{
	struct pub_sub_allocators *allocators = &fixture->allocators;
	void *msg;
	size_t previous_msg_max_size = 0 - 1; // -1 to start from 0
	// Test allocations come from the correct allocator based on msg size
	for (size_t i = 0; i < fixture->num_allocators; i++) {
		for (size_t msg_size = previous_msg_max_size + 1;
		     msg_size < fixture->allocator_msg_sizes[i]; msg_size++) {
			msg = pub_sub_alloc_new(allocators, 0, msg_size, K_NO_WAIT);
			zassert_not_null(msg);
			zassert_equal(pub_sub_msg_get_alloc_id(msg), i,
				      "msg allocator id: %u, expected: %u, msg size: %u",
				      pub_sub_msg_get_alloc_id(msg), i, msg_size);
			pub_sub_alloc_release(allocators, msg);
		}
		previous_msg_max_size = fixture->allocator_msg_sizes[i];
	}

	// Test allocating a message that is too large
	size_t max_msg_size = fixture->allocator_msg_sizes[fixture->num_allocators - 1];
	msg = pub_sub_alloc_new(allocators, 0, max_msg_size + 1, K_NO_WAIT);
	zassert_is_null(msg);
}

ZTEST_F(mem_slab, test_allocator_ref_counts)
{
	struct pub_sub_allocators *allocators = &fixture->allocators;
	void *msg = NULL;
	const size_t msg_size = fixture->allocator_msg_sizes[0];
	const size_t num_msgs = fixture->allocator_num_msgs[0];

	// Allocate all of the msgs
	for (size_t i = 0; i < num_msgs; i++) {
		msg = pub_sub_alloc_new(allocators, 0, msg_size, K_NO_WAIT);
		zassert_not_null(msg);
	}
	void *new_msg = pub_sub_alloc_new(allocators, 0, msg_size, K_NO_WAIT);
	zassert_is_null(new_msg);

	// Decrement the ref count on the last msg returning it to the allocator
	pub_sub_alloc_release(allocators, msg);
	// A msg can now be allocated
	msg = pub_sub_alloc_new(allocators, 0, msg_size, K_NO_WAIT);
	zassert_not_null(msg);

	// Increment and decrement the ref count on the msg
	for (size_t i = 0; i < 10; i++) {
		pub_sub_msg_inc_ref_cnt(msg);
		pub_sub_alloc_release(allocators, msg);
	}

	// The msg is still allocated and a new one can not be allocated
	new_msg = pub_sub_alloc_new(allocators, 0, msg_size, K_NO_WAIT);
	zassert_is_null(new_msg);

	// Decrement the ref count on the last msg returning it to the allocator
	pub_sub_alloc_release(allocators, msg);
	// A msg can now be allocated
	msg = pub_sub_alloc_new(allocators, 0, msg_size, K_NO_WAIT);
	zassert_not_null(msg);
}

ZTEST_F(mem_slab, test_allocator_add)
{
	struct pub_sub_allocators *allocators = &fixture->allocators;
	struct pub_sub_allocator *allocator = malloc_mem_slab_allocator(1, 1);

	// Test adding an allocator with a small max msg size after other allocators.
	// It should get allocations based on its max msg size
	int ret = pub_sub_alloc_add(allocators, allocator);
	zassert_ok(ret);

	void *msg = pub_sub_alloc_new(allocators, 0, 1, K_NO_WAIT);
	zassert_not_null(msg);

	// The allocator only has a single msg so the next allocation should fail
	void *new_msg = pub_sub_alloc_new(allocators, 0, 1, K_NO_WAIT);
	zassert_is_null(new_msg);

	// Test adding too many allocators, 5 are supported by default so need to add 2
	// for it to fail
	allocator = malloc_mem_slab_allocator(1, 1);
	ret = pub_sub_alloc_add(allocators, allocator);
	zassert_ok(ret);

	allocator = malloc_mem_slab_allocator(1, 1);
	ret = pub_sub_alloc_add(allocators, allocator);
	// Free the allocator because it will not get freed as part of the test teardown
	if (ret != 0) {
		free_mem_slab_allocator(allocator);
	}
	zassert_equal(ret, -ENOMEM);
}

ZTEST_SUITE(mem_slab, NULL, mem_slab_suite_setup, mem_slab_before_test, mem_slab_after_test,
	    mem_slab_suite_teardown);