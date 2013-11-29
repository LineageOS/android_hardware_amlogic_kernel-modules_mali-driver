/*
 * mali_platform.h
 *
 *  Created on: Nov 8, 2013
 *      Author: amlogic
 */

#include <linux/kernel.h>
#ifndef MALI_PLATFORM_H_
#define MALI_PLATFORM_H_

#if MESON_CPU_TYPE < MESON_CPU_TYPE_MESON8
#else
#include "meson8/platform_api.h"
#endif


#endif /* MALI_PLATFORM_H_ */
