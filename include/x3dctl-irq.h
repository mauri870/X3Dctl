#ifndef X3DCTL_IRQ_H
#define X3DCTL_IRQ_H

#include "x3dctl-common.h"

/*
 * Return non-zero if a /proc/interrupts line appears to belong to a GPU driver.
 *   - amdgpu
 *   - nvidia
 *   - nouveau
 */
int irq_is_gpu_line(const char *line);

/*
 * Steer all detected GPU IRQs to the provided CPU mask.
 * Silently skips IRQs that cannot be opened.
 */
void irq_steer_gpu_irqs(const cpu_set_t *target_mask);

/*
 * Steer all IRQs in /proc/interrupts to the provided CPU mask.
 * Silently skips IRQs that cannot be opened or are kernel-managed.
 */
void irq_steer_all_irqs(const cpu_set_t *target_mask);

/*
 * Read the current smp_affinity mask for a specific IRQ into buf.
 * Newline is stripped if present.
 *
 * Returns 0 on success, non-zero on failure.
 */
int irq_read_mask(int irq, char *buf, size_t size);

/*
 * Fork a background watcher pinned to target_mask that re-applies
 * irq_steer_all_irqs every 2 seconds. PID is written to WATCHER_PIDFILE.
 * Calls irq_watcher_stop() first to replace any existing watcher.
 */
void irq_watcher_start(const cpu_set_t *target_mask);

/*
 * Kill the running watcher (if any) and remove WATCHER_PIDFILE.
 */
void irq_watcher_stop(void);

#endif 
