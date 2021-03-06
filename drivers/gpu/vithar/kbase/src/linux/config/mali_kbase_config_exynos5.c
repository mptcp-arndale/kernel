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
#include <linux/clk.h>
#include <kbase/src/common/mali_kbase.h>
#include <kbase/src/common/mali_kbase_defs.h>
#include <kbase/src/linux/mali_kbase_config_linux.h>
#include <kbase/src/platform/mali_kbase_runtime_pm.h>
#include <ump/ump_common.h>
#include <mach/map.h>
#include <plat/devs.h>
#include <plat/pd.h>
#include <linux/pm_runtime.h>
#include <kbase/src/platform/mali_kbase_platform.h>
#include <kbase/src/platform/mali_kbase_runtime_pm.h>
#include <linux/mm.h>

#define HZ_IN_MHZ                           (1000000)

static kbase_io_resources io_resources =
{
	.job_irq_number   = JOB_IRQ_NUMBER,
	.mmu_irq_number   = MMU_IRQ_NUMBER,
	.gpu_irq_number   = GPU_IRQ_NUMBER,
	.io_memory_region =
	{
		.start = EXYNOS5_PA_G3D,
		.end   = EXYNOS5_PA_G3D+ (4096 * 5) - 1
	}
};

int get_cpu_clock_speed(u32* cpu_clock)
{
	struct clk * cpu_clk;
	u32 freq=0;
	cpu_clk = clk_get(NULL, "armclk");
	if (IS_ERR(cpu_clk))
		return 1;
	freq = clk_get_rate(cpu_clk);
	*cpu_clock = (freq/HZ_IN_MHZ);
	return 0;
}

/**
 *  * Exynos5 hardware specific initialization
 *   */
mali_bool kbase_platform_exynos5_init(kbase_device *kbdev)
{
	if(MALI_ERROR_NONE == kbase_platform_init(kbdev))
	{
#ifdef CONFIG_VITHAR_DEBUG_SYS
		if(kbase_platform_create_sysfs_file(kbdev->osdev.dev))
		{
			return MALI_TRUE;
		}
#endif /* CONFIG_VITHAR_DEBUG_SYS */
		return MALI_TRUE;
	}

	return MALI_FALSE;
}

/**
 *  * Exynos5 hardware specific termination
 *   */
void kbase_platform_exynos5_term(kbase_device *kbdev)
{
#ifdef CONFIG_VITHAR_DEBUG_SYS
	kbase_platform_remove_sysfs_file(kbdev->osdev.dev);
#endif /* CONFIG_VITHAR_DEBUG_SYS */
	kbase_platform_term(kbdev);
}

kbase_platform_funcs_conf platform_funcs =
{
	.platform_init_func = &kbase_platform_exynos5_init,
	.platform_term_func = &kbase_platform_exynos5_term,
};

#ifdef CONFIG_VITHAR_RT_PM
static int pm_callback_power_on(kbase_device *kbdev)
{
	/* Nothing is needed on VExpress, but we may have destroyed GPU state (if the below HARD_RESET code is active) */
	int ret_val;
	struct kbase_os_device *osdev = &kbdev->osdev;


	if (osdev->dev->power.runtime_status == RPM_SUSPENDED)
		ret_val = 1;
	else
		ret_val = 0;

	kbase_device_runtime_get_sync(osdev->dev);
	return ret_val;
}

static void pm_callback_power_off(kbase_device *kbdev)
{
	struct kbase_os_device *osdev = &kbdev->osdev;
	kbase_device_runtime_put_sync(osdev->dev);
}


static kbase_pm_callback_conf pm_callbacks =
{
	.power_on_callback = pm_callback_power_on,
	.power_off_callback = pm_callback_power_off,
#ifdef CONFIG_PM_RUNTIME
	.power_runtime_init_callback = kbase_device_runtime_init,
	.power_runtime_term_callback = kbase_device_runtime_disable,
	.power_runtime_on_callback = NULL,
	.power_runtime_off_callback = NULL,
#else /* CONFIG_PM_RUNTIME */
	.power_runtime_init_callback = NULL,
	.power_runtime_term_callback = NULL,
	.power_runtime_on_callback = NULL,
	.power_runtime_off_callback = NULL,
#endif /* CONFIG_PM_RUNTIME */
};
#endif

static kbase_attribute config_attributes[] = {
	{
		KBASE_CONFIG_ATTR_UMP_DEVICE,
		UMP_DEVICE_Z_SHIFT
	},

	{
		KBASE_CONFIG_ATTR_MEMORY_OS_SHARED_MAX,
		2048 * 1024 * 1024UL /* 2048MB */
	},

	{
		KBASE_CONFIG_ATTR_MEMORY_OS_SHARED_PERF_GPU,
		KBASE_MEM_PERF_FAST
	},
#ifdef CONFIG_VITHAR_RT_PM
	{
		KBASE_CONFIG_ATTR_POWER_MANAGEMENT_CALLBACKS,
		(uintptr_t)&pm_callbacks
	},
#endif
	{
		KBASE_CONFIG_ATTR_PLATFORM_FUNCS,
		(uintptr_t)&platform_funcs
	},
	{
		KBASE_CONFIG_ATTR_GPU_FREQ_KHZ_MAX,
		533000
	},

	{
		KBASE_CONFIG_ATTR_GPU_FREQ_KHZ_MIN,
		100000
	},
	{
		KBASE_CONFIG_ATTR_JS_RESET_TIMEOUT_MS,
		500 /* 500ms before cancelling stuck jobs */
	},
	{
		KBASE_CONFIG_ATTR_CPU_SPEED_FUNC,
		(uintptr_t)&get_cpu_clock_speed
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
		.midgard_type              = KBASE_MALI_T604
};

void kbase_update_platform_config(void)
{
	int i = 0;

	while (config_attributes[i].id != KBASE_CONFIG_ATTR_END) {
		if (config_attributes[i].id == KBASE_CONFIG_ATTR_MEMORY_OS_SHARED_MAX) {
			config_attributes[i].data = num_physpages << PAGE_SHIFT;
			break;
		}
		i++;
	}
	return;
}

