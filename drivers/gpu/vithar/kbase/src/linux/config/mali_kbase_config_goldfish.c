/*
 *
 * (C) COPYRIGHT 2011 ARM Limited. All rights reserved.
 *
 * This program is free software and is provided to you under the terms of the GNU General Public License version 2
 * as published by the Free Software Foundation, and any use by you of this program is subject to the terms of such GNU licence.
 * 
 * A copy of the licence is included with the program, and can also be obtained from Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 * 
 */



#include <linux/ioport.h>
#include <kbase/src/common/mali_kbase.h>
#include <kbase/src/common/mali_kbase_defs.h>
#include <kbase/src/linux/mali_kbase_config_linux.h>
#include <ump/ump_common.h>

static kbase_io_resources io_resources =
{
	.job_irq_number   = 20,
	.mmu_irq_number   = 20,
	.gpu_irq_number   = 20,
	.io_memory_region =
	{
		.start = 0xFC010000,
		.end   = 0xFC010000 + (4096 * 5) - 1
	}
};

static kbase_attribute config_attributes[] = {
	{
		KBASE_CONFIG_ATTR_MEMORY_PER_PROCESS_LIMIT,
		512 * 1024 * 1024UL /* 512MB */
	},
	{
		KBASE_CONFIG_ATTR_UMP_DEVICE,
		UMP_DEVICE_Z_SHIFT
	},

	{
		KBASE_CONFIG_ATTR_MEMORY_OS_SHARED_MAX,
		768 * 1024 * 1024UL /* 768MB */
	},

	{
		KBASE_CONFIG_ATTR_MEMORY_OS_SHARED_PERF_GPU,
		KBASE_MEM_PERF_SLOW
	},

	{
		KBASE_CONFIG_ATTR_GPU_FREQ_KHZ_MAX,
		130
	},

	{
		KBASE_CONFIG_ATTR_GPU_FREQ_KHZ_MIN,
		130
	},

	{
		KBASE_CONFIG_ATTR_END,
		0
	}
};

kbase_platform_config platform_config =
{
		.attributes                = config_attributes,
		.io_resources              = &io_resources,
		.midgard_type              = KBASE_MALI_T6XM
};

