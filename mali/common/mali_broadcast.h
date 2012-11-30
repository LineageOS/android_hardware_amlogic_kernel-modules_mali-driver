/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2012 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

#include "mali_hw_core.h"
#include "mali_group.h"

struct mali_bcast_unit;

struct mali_bcast_unit *mali_bcast_unit_create(const _mali_osk_resource_t * resource);
void mali_bcast_unit_delete(struct mali_bcast_unit *bcast_unit);

void mali_bcast_add_group(struct mali_bcast_unit *bcast_unit, struct mali_group *group);
void mali_bcast_remove_group(struct mali_bcast_unit *bcast_unit, struct mali_group *group);

void mali_bcast_reset(struct mali_bcast_unit *bcast_unit);

void mali_bcast_print(struct mali_bcast_unit *bcast_unit);
