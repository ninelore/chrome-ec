/* Copyright 2014 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Watchdog common code */

#include "common.h"
#include "cpu.h"
#include "panic.h"
#include "task.h"
#include "timer.h"
#include "watchdog.h"

/*
 * As defined by Armv7-M Reference Manual B1.5.6 "Exception Entry Behavior",
 * the structure of the saved context on the stack is:
 * r0, r1, r2, r3, r12, lr, pc, psr, ...
 */
#define STACK_IDX_REG_LR 5
#define STACK_IDX_REG_PC 6

void watchdog_trace(uint32_t excep_lr, uint32_t excep_sp)
{
	uint32_t psp;
	uint32_t *stack;

	asm("mrs %0, psp" : "=r"(psp));
	if (is_frame_in_handler_stack(excep_lr)) {
		/* we were already in exception context */
		stack = (uint32_t *)excep_sp;
	} else {
		/* we were in task context */
		stack = (uint32_t *)psp;
	}

	/* Log PC from stack and task id.
	 *
	 * Why we set set PANIC_SW_WATCHDOG_WARN reason:
	 *
	 * The PANIC_SW_WATCHDOG_WARN reason will be changed to a regular
	 * PANIC_SW_WATCHDOG in system_common_pre_init if a watchdog reset
	 * actually occurs. If no watchdog reset occurs, this watchdog warning
	 * panic may still be collected by the kernel and handled as a
	 * non-fatal EC panic.
	 */
	panic_set_reason(PANIC_SW_WATCHDOG_WARN, stack[STACK_IDX_REG_PC],
			 task_get_current());

	panic_printf("### WATCHDOG PC=%08x / LR=%08x / pSP=%08x ",
		     stack[STACK_IDX_REG_PC], stack[STACK_IDX_REG_LR], psp);
	if (is_exception_from_handler_mode(excep_lr))
		panic_puts("(exc) ###\n");
	else
		panic_printf("(task %d) ###\n", task_get_current());

	/* If we are blocked in a high priority IT handler, the following debug
	 * messages might not appear but they are useless in that situation. */
	timer_print_info();
	task_print_list();

	if (IS_ENABLED(CONFIG_PANIC_ON_WATCHDOG_WARNING))
		software_panic(PANIC_SW_WATCHDOG, task_get_current());
}
