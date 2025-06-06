/* Copyright 2013 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
#include "config.h"

#define STRINGIFY0(name) #name
#define STRINGIFY(name) STRINGIFY0(name)

#define FW_OFF_(section) CONFIG_##section##_MEM_OFF
#define FW_OFF(section) (CONFIG_PROGRAM_MEMORY_BASE + FW_OFF_(section))

#define FW_SIZE_(section) CONFIG_##section##_SIZE
#define FW_SIZE(section) FW_SIZE_(section)


OUTPUT_FORMAT(BFD_FORMAT, BFD_FORMAT, BFD_FORMAT)
OUTPUT_ARCH(BFD_ARCH)
ENTRY(reset)

MEMORY
{
	FLASH (rx) : ORIGIN = FW_OFF(SECTION), LENGTH = FW_SIZE(SECTION)
	IRAM (rw)  : ORIGIN = CONFIG_RAM_BASE, LENGTH = CONFIG_RAM_SIZE
#if defined(CONFIG_HOSTCMD_X86) || defined(CONFIG_I2C_PERIPHERAL)
	H2RAM (rw) : ORIGIN = CONFIG_H2RAM_BASE, LENGTH = CONFIG_H2RAM_SIZE
#endif
}

SECTIONS
{
	.text : {
		/*
		 * We put "__flash_dma_start" at the beginning of
		 * the text section to avoid gap.
		 */
		__flash_dma_start = .;
		ASSERT((__flash_dma_start == 0),
		       "__flash_dma_start has to be 4k-byte aligned");
		KEEP(STRINGIFY(OUTDIR/core/CORE/init.o) (.text.vecttable))
		. = ALIGN(4);
		__image_data_offset = .;
		KEEP(*(.rodata.ver))
		. = ALIGN(4);
		KEEP(STRINGIFY(OUTDIR/core/CORE/init.o) (.text.vectirq))
		KEEP(STRINGIFY(OUTDIR/core/CORE/init.o) (.text))
		KEEP(*(.flash_direct_map))
		. = ALIGN(16);
		KEEP(*(.ram_code))
		__flash_dma_size = . - __flash_dma_start;
		ASSERT((__flash_dma_size < IT83XX_ILM_BLOCK_SIZE),
		       "__flash_dma_size < IT83XX_ILM_BLOCK_SIZE");

#ifdef CONFIG_IT83XX_EXTENDED_ILM
		. = ALIGN(IT83XX_ILM_BLOCK_SIZE);
		__nds32_flash_ilm_start = .;
#ifdef CONFIG_IT83XX_EXTENDED_ILM_I2C_DRIVER
		OUTDIR/chip/it83xx/i2c.o (.text*)
		OUTDIR/chip/it83xx/i2c.o (.rodata)
#endif
#ifdef CONFIG_IT83XX_EXTENDED_ILM_I2C_CONTROLLER
		OUTDIR/common/i2c_controller.o (.text*)
		OUTDIR/common/i2c_controller.o (.rodata)
#endif
#ifdef CONFIG_IT83XX_EXTENDED_ILM_HWTIMER_DRIVER
		OUTDIR/chip/it83xx/hwtimer.o (.text*)
		OUTDIR/chip/it83xx/hwtimer.o (.rodata)
#endif
#ifdef CONFIG_IT83XX_EXTENDED_ILM_TIMER
		OUTDIR/common/timer.o (.text*)
		OUTDIR/common/timer.o (.rodata)
#endif
#ifdef CONFIG_IT83XX_EXTENDED_ILM_IRQ_DRIVER
		OUTDIR/chip/it83xx/irq.o (.text*)
		OUTDIR/chip/it83xx/irq.o (.rodata)
#endif
#ifdef CONFIG_IT83XX_EXTENDED_ILM_UART_DRIVER
		OUTDIR/chip/it83xx/uart.o (.text*)
		OUTDIR/chip/it83xx/uart.o (.rodata)
#endif
#ifdef CONFIG_IT83XX_EXTENDED_ILM_USBC_PD_TIMER
		OUTDIR/common/usbc/usb_pd_timer.o (.text*)
		OUTDIR/common/usbc/usb_pd_timer.o (.rodata)
#endif
#ifdef CONFIG_IT83XX_EXTENDED_ILM_RAA489000_DRIVER
		OUTDIR/driver/tcpm/raa489000.o (.text*)
		OUTDIR/driver/tcpm/raa489000.o (.rodata)
#endif
#ifdef CONFIG_IT83XX_EXTENDED_ILM_TCPCI_DRIVER
		OUTDIR/driver/tcpm/tcpci.o (.text*)
		OUTDIR/driver/tcpm/tcpci.o (.rodata)
#endif
		__nds32_flash_ilm_end = .;
		__nds32_flash_ilm_size = . - __nds32_flash_ilm_start;
#endif

		. = ALIGN(IT83XX_ILM_BLOCK_SIZE);
		__flash_text_start = .;
		*(.text*)
	} > FLASH
	. = ALIGN(4);
	.rodata :  {
		/* Symbols defined here are declared in link_defs.h */
		__irqprio = .;
		KEEP(*(.rodata.irqprio))
		__irqprio_end = .;

		. = ALIGN(4);
		__irqhandler = .;
		KEEP(STRINGIFY(OUTDIR/core/CORE/init.o) (.rodata.vecthandlers))

		. = ALIGN(4);
		__cmds = .;
		KEEP(*(SORT(.rodata.cmds*)))
		__cmds_end = .;

		. = ALIGN(4);
		__hcmds = .;
		KEEP(*(SORT(.rodata.hcmds*)))
		__hcmds_end = .;

		. = ALIGN(4);
		__mkbp_evt_srcs = .;
		KEEP(*(.rodata.evtsrcs))
		__mkbp_evt_srcs_end = .;

		. = ALIGN(4);
		__hooks_init = .;
		KEEP(*(.rodata.HOOK_INIT))
		__hooks_init_end = .;

		__hooks_init_early = .;
		KEEP(*(.rodata.HOOK_INIT_EARLY))
		__hooks_init_early_end = .;

		__hooks_pre_freq_change = .;
		KEEP(*(.rodata.HOOK_PRE_FREQ_CHANGE))
		__hooks_pre_freq_change_end = .;

		__hooks_freq_change = .;
		KEEP(*(.rodata.HOOK_FREQ_CHANGE))
		__hooks_freq_change_end = .;

		__hooks_sysjump = .;
		KEEP(*(.rodata.HOOK_SYSJUMP))
		__hooks_sysjump_end = .;

		__hooks_chipset_pre_init = .;
		KEEP(*(.rodata.HOOK_CHIPSET_PRE_INIT))
		__hooks_chipset_pre_init_end = .;

		__hooks_chipset_startup = .;
		KEEP(*(.rodata.HOOK_CHIPSET_STARTUP))
		__hooks_chipset_startup_end = .;

		__hooks_chipset_resume = .;
		KEEP(*(.rodata.HOOK_CHIPSET_RESUME))
		__hooks_chipset_resume_end = .;

		__hooks_chipset_suspend = .;
		KEEP(*(.rodata.HOOK_CHIPSET_SUSPEND))
		__hooks_chipset_suspend_end = .;

#ifdef CONFIG_CHIPSET_RESUME_INIT_HOOK
		__hooks_chipset_resume_init = .;
		KEEP(*(.rodata.HOOK_CHIPSET_RESUME_INIT))
		__hooks_chipset_resume_init_end = .;

		__hooks_chipset_suspend_complete = .;
		KEEP(*(.rodata.HOOK_CHIPSET_SUSPEND_COMPLETE))
		__hooks_chipset_suspend_complete_end = .;
#endif

		__hooks_chipset_shutdown = .;
		KEEP(*(.rodata.HOOK_CHIPSET_SHUTDOWN))
		__hooks_chipset_shutdown_end = .;

		__hooks_chipset_shutdown_complete = .;
		KEEP(*(.rodata.HOOK_CHIPSET_SHUTDOWN_COMPLETE))
		__hooks_chipset_shutdown_complete_end = .;

		__hooks_chipset_hard_off = .;
		KEEP(*(.rodata.HOOK_CHIPSET_HARD_OFF))
		__hooks_chipset_hard_off_end = .;

		__hooks_chipset_reset = .;
		KEEP(*(.rodata.HOOK_CHIPSET_RESET))
		__hooks_chipset_reset_end = .;

		__hooks_ac_change = .;
		KEEP(*(.rodata.HOOK_AC_CHANGE))
		__hooks_ac_change_end = .;

		__hooks_lid_change = .;
		KEEP(*(.rodata.HOOK_LID_CHANGE))
		__hooks_lid_change_end = .;

		__hooks_tablet_mode_change = .;
		KEEP(*(.rodata.HOOK_TABLET_MODE_CHANGE))
		__hooks_tablet_mode_change_end = .;

#ifdef CONFIG_BODY_DETECTION
		__hooks_body_detect_change = .;
		KEEP(*(.rodata.HOOK_BODY_DETECT_CHANGE))
		__hooks_body_detect_change_end = .;
#endif

		__hooks_base_attached_change = .;
		KEEP(*(.rodata.HOOK_BASE_ATTACHED_CHANGE))
		__hooks_base_attached_change_end = .;

		__hooks_pwrbtn_change = .;
		KEEP(*(.rodata.HOOK_POWER_BUTTON_CHANGE))
		__hooks_pwrbtn_change_end = .;

		__hooks_battery_soc_change = .;
		KEEP(*(.rodata.HOOK_BATTERY_SOC_CHANGE))
		__hooks_battery_soc_change_end = .;

#ifdef CONFIG_USB_SUSPEND
		__hooks_usb_change = .;
		KEEP(*(.rodata.HOOK_USB_PM_CHANGE))
		__hooks_usb_change_end = .;
#endif

		__hooks_tick = .;
		KEEP(*(.rodata.HOOK_TICK))
		__hooks_tick_end = .;

		__hooks_second = .;
		KEEP(*(.rodata.HOOK_SECOND))
		__hooks_second_end = .;

		__hooks_usb_pd_disconnect = .;
		KEEP(*(.rodata.HOOK_USB_PD_DISCONNECT))
		__hooks_usb_pd_disconnect_end = .;

		__hooks_usb_pd_connect = .;
		KEEP(*(.rodata.HOOK_USB_PD_CONNECT))
		__hooks_usb_pd_connect_end = .;

		__hooks_power_supply_change = .;
		KEEP(*(.rodata.HOOK_POWER_SUPPLY_CHANGE))
		__hooks_power_supply_change_end = .;

		__deferred_funcs = .;
		KEEP(*(.rodata.deferred))
		__deferred_funcs_end = .;

		. = ALIGN(4);
		*(.rodata*)

#ifdef CONFIG_CHIP_INIT_ROM_REGION
	ASSERT(0, "CONFIG_CHIP_INIT_ROM_REGION not supported by linker script")
#endif /* CONFIG_CHIP_INIT_ROM_REGION */
		/*
		 * This linker file does not yet support a separate ROM resident
		 * section. Ensure the corresponding data objects are linked
		 * into the .rodata section.
		 */
		. = ALIGN(4);
		__init_rom_start = .;
		*(.init.rom)
		__init_rom_end = .;

#if defined(SECTION_IS_RO) && defined(CONFIG_FLASH_CROS)
		. = ALIGN(64);
		KEEP(*(.google))
#endif
		. = ALIGN(4);
	} >FLASH

	__data_lma_start = . ;

	.data : {
		. = ALIGN(4);
		__data_start = .;
		*(.data.tasks)
		*(.data)
#ifdef CONFIG_MPU
		/* It has to be aligned by 32 bytes to be a valid MPU region. */
		. = ALIGN(32);
		__iram_text_start = .;
#else
		. = ALIGN(4);
#endif
		*(.iram.text)
#ifdef CONFIG_MPU
		. = ALIGN(32);
		__iram_text_end = .;
#else
		. = ALIGN(4);
#endif
		__data_end = .;

	} > IRAM AT>FLASH

	.bss : {
		/* Stacks must be 64-bit aligned */
		. = ALIGN(8);
		__bss_start = .;
		*(.bss.tasks)
		. = ALIGN(8);
		*(.bss.system_stack)
		. = ALIGN(8);
		*(.bss.task_scratchpad)
		/* Rest of .bss takes care of its own alignment */
		*(.bss)

		/*
		 * Reserve space for deferred function firing times.
		 * Each time is a uint64_t, each func is a 32-bit pointer,
		 * thus the scaling factor of two.
		 */
		. = ALIGN(8);
		__deferred_until = .;
		. += (__deferred_funcs_end - __deferred_funcs) * (8 / 4);
		__deferred_until_end = .;

		. = ALIGN(4);
		__bss_end = .;

#ifdef CONFIG_IT83XX_EXTENDED_ILM
		. = ALIGN(IT83XX_ILM_BLOCK_SIZE);
		__nds32_flash_dlm_start = .;
		. += __nds32_flash_ilm_end - __nds32_flash_ilm_start;
		. = ALIGN(IT83XX_ILM_BLOCK_SIZE);
#endif

	} > IRAM

	#include "noinit_end_of_ram.inc"

	/*
	 * Shared memory buffer must be at the end of preallocated
	 * RAM, so it can expand to use all the remaining RAM.
	 */
	__shared_mem_buf = .;

	/* NO RAM ALLOCATIONS MAY GO AFTER THIS! */

	ASSERT((__shared_mem_buf + CONFIG_SHAREDMEM_MINIMUM_SIZE) <=
	       (CONFIG_RAM_BASE + CONFIG_RAM_SIZE),
	       "Not enough space for shared memory.")
	__ram_free = (CONFIG_RAM_BASE + CONFIG_RAM_SIZE) -
		     (__shared_mem_buf + CONFIG_SHAREDMEM_MINIMUM_SIZE);

	/*
	 * __flash_used and __flash_size is used in flash free calculations by
	 * the makefile. Note the difference between __flash_size and
	 * __config_flash_size. __flash_size is the flash size for the given
	 * image (RW, RO, etc.), while __config_flash_size is the *total*
	 * flash size.
	 * __image_size is stored in the struct image_data header and used
	 * in hash calculations.
	 */
	__flash_used = LOADADDR(.data) + SIZEOF(.data) - FW_OFF(SECTION);
	__flash_size = LENGTH(FLASH);
	__image_size = __flash_used;

#if defined(SECTION_IS_RW) && (CONFIG_FLASH_WRITE_SIZE > 0)
	/* Alignment is needed by flash_command_write() for RW update. */
	ASSERT(__image_size % CONFIG_FLASH_WRITE_SIZE == 0,
	       "Error: RW image is not aligned to CONFIG_FLASH_WRITE_SIZE")
#endif

#ifdef CONFIG_FLASH_CROS
        /*
         * These linker labels are just for analysis and not used in the code.
         */
	__config_flash_size = CONFIG_FLASH_SIZE_BYTES;
	__config_ro_size = CONFIG_RO_SIZE;
	__config_ec_protected_storage_size = CONFIG_EC_PROTECTED_STORAGE_SIZE;
	__config_rw_size = CONFIG_RW_SIZE;
	__config_ec_writable_storage_size = CONFIG_EC_WRITABLE_STORAGE_SIZE;
#endif

#if defined(CONFIG_HOSTCMD_X86) || defined(CONFIG_I2C_PERIPHERAL)
	.h2ram (NOLOAD) : {
		. += CONFIG_H2RAM_HOST_LPC_IO_BASE;
		*(.h2ram.pool.hostcmd)
		. = ALIGN(256);
		*(.h2ram.pool.acpiec)
#ifdef CONFIG_I2C_PERIPHERAL
		. = ALIGN(256);
		*(.h2ram.pool.i2cslv)
#endif
#ifdef CONFIG_IT83XX_I2C_CMD_QUEUE
		. = ALIGN(256);
		*(.h2ram.pool.i2c_cq)
#endif
		__h2ram_end = .;
	} > H2RAM

	ASSERT((__h2ram_end) <= (CONFIG_H2RAM_BASE + CONFIG_H2RAM_SIZE),
	       "Not enough space for h2ram section.")
#endif

#if !(defined(SECTION_IS_RO) && defined(CONFIG_FLASH_CROS))
	/DISCARD/ : { *(.google) }
#endif

	/DISCARD/ : { *(.ARM.*) }
}
