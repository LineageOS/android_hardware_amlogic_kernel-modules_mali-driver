/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

/**
 * @file mali_sync.h
 *
 */

#ifndef _MALI_SYNC_H_
#define _MALI_SYNC_H_

#ifdef CONFIG_SYNC

#include <linux/seq_file.h>
#include <linux/sync.h>

/*
 * Create a stream object.
 * Built on top of timeline object.
 * Exposed as a file descriptor.
 * Life-time controlled via the file descriptor:
 * - dup to add a ref
 * - close to remove a ref
 */
_mali_osk_errcode_t mali_stream_create(const char * name, int * out_fd);

/*
 * Create a fence in a stream object
 */
struct sync_pt *mali_stream_create_point(int tl_fd);
int mali_stream_create_fence(struct sync_pt *pt);

/*
 * Validate a fd to be a valid fence
 * No reference is taken.
 *
 * This function is only usable to catch unintentional user errors early,
 * it does not stop malicious code changing the fd after this function returns.
 */
_mali_osk_errcode_t mali_fence_validate(int fd);


/* Returns true if the specified timeline is allocated by Mali */
int mali_sync_timeline_is_ours(struct sync_timeline *timeline);

/* Allocates a timeline for Mali
 *
 * One timeline should be allocated per API context.
 */
struct sync_timeline *mali_sync_timeline_alloc(const char *name);

/* Allocates a sync point within the timeline.
 *
 * The timeline must be the one allocated by mali_sync_timeline_alloc
 *
 * Sync points must be triggered in *exactly* the same order as they are allocated.
 */
struct sync_pt *mali_sync_pt_alloc(struct sync_timeline *parent);

/* Signals a particular sync point
 *
 * Sync points must be triggered in *exactly* the same order as they are allocated.
 *
 * If they are signalled in the wrong order then a message will be printed in debug
 * builds and otherwise attempts to signal order sync_pts will be ignored.
 */
void mali_sync_signal_pt(struct sync_pt *pt);

#endif /* CONFIG_SYNC */
#endif /* _MALI_SYNC_H_ */
