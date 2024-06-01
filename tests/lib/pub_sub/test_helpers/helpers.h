/* Copyright (c) 2024 Joshua White
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef HELPERS_H_
#define HELPERS_H_

#ifdef __cplusplus
extern "C" {
#endif
#include <pub_sub/pub_sub.h>

struct pub_sub_allocator *malloc_mem_slab_allocator(size_t msg_size, size_t num_msgs);
void reset_mem_slab_allocator(struct pub_sub_allocator *allocator);
void free_mem_slab_allocator(struct pub_sub_allocator *allocator);

#ifdef __cplusplus
}
#endif

#endif /* HELPERS_H_ */