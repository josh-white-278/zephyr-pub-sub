/* Copyright (c) 2024 Joshua White
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef HELPERS_H_
#define HELPERS_H_

#ifdef __cplusplus
extern "C" {
#endif
#include <pub_sub/pub_sub.h>

struct rx_msg {
	uint16_t msg_id;
	const void *msg;
};

struct callback_subscriber {
	struct k_msgq msgq;
	struct rx_msg msgq_buffer[8];
	atomic_t *subs_bitarray;
	struct pub_sub_subscriber subscriber;
};

struct msgq_subscriber {
	struct k_msgq msgq;
	void *msgq_buffer;
	atomic_t *subs_bitarray;
	struct pub_sub_subscriber subscriber;
};

struct fifo_subscriber {
	atomic_t *subs_bitarray;
	struct pub_sub_subscriber subscriber;
};

struct pub_sub_allocator *malloc_mem_slab_allocator(size_t msg_size, size_t num_msgs);
void reset_mem_slab_allocator(struct pub_sub_allocator *allocator);
void free_mem_slab_allocator(struct pub_sub_allocator *allocator);

struct callback_subscriber *malloc_callback_subscriber(uint16_t max_msg_id);
void free_callback_subscriber(struct callback_subscriber *c_subscriber);

struct msgq_subscriber *malloc_msgq_subscriber(uint16_t max_msg_id, size_t msgq_len);
void free_msgq_subscriber(struct msgq_subscriber *m_subscriber);

struct fifo_subscriber *malloc_fifo_subscriber(uint16_t max_msg_id);
void free_fifo_subscriber(struct fifo_subscriber *f_subscriber);

void teardown_pub_sub_broker(struct pub_sub_broker *broker);
void reset_default_broker(void);

#ifdef __cplusplus
}
#endif

#endif /* HELPERS_H_ */