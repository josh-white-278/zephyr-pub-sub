/* Copyright (c) 2024 Joshua White
 * SPDX-License-Identifier: Apache-2.0
 */
#include "helpers.h"
#include <stdlib.h>
#include <pub_sub/msg_alloc_mem_slab.h>
#include <zephyr/ztest.h>

static void callback_msg_handler(uint16_t msg_id, const void *msg, void *user_data);

struct pub_sub_allocator *malloc_mem_slab_allocator(size_t msg_size, size_t num_msgs)
{
	uint8_t *buffer = malloc(PUB_SUB_MEM_SLAB_ALLOCATOR_BUF_SIZE(msg_size, num_msgs));
	struct k_mem_slab *mem_slab = malloc(sizeof(struct k_mem_slab));
	struct pub_sub_allocator *allocator = malloc(sizeof(struct pub_sub_allocator));
	k_mem_slab_init(mem_slab, buffer, PUB_SUB_MEM_SLAB_ALLOCATOR_BLOCK_SIZE(msg_size),
			num_msgs);
	pub_sub_init_mem_slab_allocator(allocator, mem_slab);
	return allocator;
}

void reset_mem_slab_allocator(struct pub_sub_allocator *allocator)
{
	struct k_mem_slab *mem_slab = allocator->impl;
	k_mem_slab_init(mem_slab, mem_slab->buffer, mem_slab->info.block_size,
			mem_slab->info.num_blocks);
}

void free_mem_slab_allocator(struct pub_sub_allocator *allocator)
{
	struct k_mem_slab *mem_slab = allocator->impl;
	// Check all of mem_slab blocks are free to see if we have any leaks
	__ASSERT(k_mem_slab_num_used_get(mem_slab) == 0, "");
	free(mem_slab->buffer);
	free(mem_slab);
	free(allocator);
}

struct callback_subscriber *malloc_callback_subscriber(uint16_t max_msg_id)
{
	struct callback_subscriber *c_subscriber = malloc(sizeof(struct callback_subscriber));
	c_subscriber->subs_bitarray = malloc(PUB_SUB_SUBS_BITARRAY_BYTE_LEN(max_msg_id));

	k_msgq_init(&c_subscriber->msgq, (char *)c_subscriber->msgq_buffer,
		    sizeof(c_subscriber->msgq_buffer[0]), ARRAY_SIZE(c_subscriber->msgq_buffer));
	pub_sub_init_callback_subscriber(&c_subscriber->subscriber, c_subscriber->subs_bitarray,
					 max_msg_id);
	pub_sub_subscriber_set_handler_data(&c_subscriber->subscriber, callback_msg_handler,
					    (void *)&c_subscriber->msgq);
	return c_subscriber;
}

void free_callback_subscriber(struct callback_subscriber *c_subscriber)
{
	free(c_subscriber->subs_bitarray);
	free(c_subscriber);
}

struct msgq_subscriber *malloc_msgq_subscriber(uint16_t max_msg_id, size_t msgq_len)
{
	struct msgq_subscriber *m_subscriber = malloc(sizeof(struct msgq_subscriber));
	m_subscriber->msgq_buffer = malloc(PUB_SUB_RX_MSGQ_BUFFER_LEN(msgq_len));
	m_subscriber->subs_bitarray = malloc(PUB_SUB_SUBS_BITARRAY_BYTE_LEN(max_msg_id));

	k_msgq_init(&m_subscriber->msgq, (char *)m_subscriber->msgq_buffer,
		    PUB_SUB_RX_MSGQ_MSG_SIZE, msgq_len);
	pub_sub_init_msgq_subscriber(&m_subscriber->subscriber, m_subscriber->subs_bitarray,
				     max_msg_id, &m_subscriber->msgq);
	return m_subscriber;
}

void free_msgq_subscriber(struct msgq_subscriber *m_subscriber)
{
	free(m_subscriber->subs_bitarray);
	free(m_subscriber->msgq_buffer);
	free(m_subscriber);
}

struct fifo_subscriber *malloc_fifo_subscriber(uint16_t max_msg_id)
{
	struct fifo_subscriber *f_subscriber = malloc(sizeof(struct fifo_subscriber));
	f_subscriber->subs_bitarray = malloc(PUB_SUB_SUBS_BITARRAY_BYTE_LEN(max_msg_id));

	pub_sub_init_fifo_subscriber(&f_subscriber->subscriber, f_subscriber->subs_bitarray,
				     max_msg_id);
	return f_subscriber;
}

void free_fifo_subscriber(struct fifo_subscriber *f_subscriber)
{
	free(f_subscriber->subs_bitarray);
	free(f_subscriber);
}

void teardown_pub_sub_broker(struct pub_sub_broker *broker)
{
	// A pub_sub broker isn't really made to be torn down during normal operation
	// so we have to look inside and do it ourselves for test teardowns

	zassert_ok(k_work_poll_cancel(&broker->publish_work));

	// Free all of the subscribers
	sys_snode_t *node;
	while ((node = sys_slist_get(&broker->subscribers)) != NULL) {
		struct pub_sub_subscriber *subscriber =
			CONTAINER_OF(node, struct pub_sub_subscriber, sub_list_node);
		switch (subscriber->rx_type) {
		case PUB_SUB_RX_TYPE_CALLBACK: {
			struct callback_subscriber *c_subscriber =
				CONTAINER_OF(subscriber, struct callback_subscriber, subscriber);
			free_callback_subscriber(c_subscriber);
			break;
		}
		case PUB_SUB_RX_TYPE_MSGQ: {
			struct msgq_subscriber *m_subscriber =
				CONTAINER_OF(subscriber, struct msgq_subscriber, subscriber);
			free_msgq_subscriber(m_subscriber);
			break;
		}
		case PUB_SUB_RX_TYPE_FIFO: {
			struct fifo_subscriber *f_subscriber =
				CONTAINER_OF(subscriber, struct fifo_subscriber, subscriber);
			free_fifo_subscriber(f_subscriber);
			break;
		}
		}
	}
}

void reset_default_broker(void)
{
	teardown_pub_sub_broker(&g_pub_sub_default_broker);
	pub_sub_init_broker(&g_pub_sub_default_broker);
}
static void callback_msg_handler(uint16_t msg_id, const void *msg, void *user_data)
{
	struct k_msgq *msgq = user_data;
	struct rx_msg rx_msg = {
		.msg_id = msg_id,
		.msg = msg,
	};
	pub_sub_acquire_msg(msg);
	int ret = k_msgq_put(msgq, &rx_msg, K_FOREVER);
	zassert_ok(ret);
}