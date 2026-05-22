#include "x3dctl-common.h"
#include "x3dctl-topology.h"
#include "x3dctl-irq.h"

int irq_is_gpu_line(const char *line)
{
    if (!line)
        return 0;

    return (
        strstr(line, "amdgpu") ||
        strstr(line, "nvidia") ||
        strstr(line, "nouveau")
    );
}

static void steer_irqs(const cpu_set_t *target_mask, int gpu_only)
{
    if (!target_mask)
        return;

    FILE *f = fopen("/proc/interrupts", "r");
    if (!f)
        return;

    char line[512];
    char hexmask[CPU_SETSIZE / 4 + 32];

    topology_cpuset_to_hexmask(target_mask, hexmask, sizeof(hexmask));

    while (fgets(line, sizeof(line), f)) {
        if (gpu_only && !irq_is_gpu_line(line))
            continue;

        char *colon = strchr(line, ':');
        if (!colon)
            continue;

        *colon = '\0';

        int irq = atoi(line);
        if (irq <= 0)
            continue;

        char path[128];
        snprintf(path, sizeof(path),
                 "/proc/irq/%d/smp_affinity", irq);

        FILE *irqf = fopen(path, "w");
        if (!irqf)
            continue;

        fprintf(irqf, "%s", hexmask);
        fclose(irqf);
    }

    fclose(f);
}

void irq_steer_gpu_irqs(const cpu_set_t *target_mask)
{
    steer_irqs(target_mask, 1);
}

void irq_steer_all_irqs(const cpu_set_t *target_mask)
{
    steer_irqs(target_mask, 0);
}

void irq_watcher_stop(void)
{
    FILE *pf = fopen(WATCHER_PIDFILE, "r");
    if (!pf)
        return;

    pid_t watcher;
    if (fscanf(pf, "%d", &watcher) == 1)
        kill(watcher, SIGTERM);
    fclose(pf);
    unlink(WATCHER_PIDFILE);
}

void irq_watcher_start(const cpu_set_t *target_mask)
{
    if (!target_mask)
        return;

    irq_watcher_stop();

    pid_t child = fork();
    if (child < 0)
        return;

    if (child > 0) {
        FILE *pf = fopen(WATCHER_PIDFILE, "w");
        if (pf) {
            fprintf(pf, "%d\n", child);
            fclose(pf);
        }
        return;
    }

    /* Detach from parent's session so we survive sudo's exit */
    setsid();

    /* Pin to target_mask (freq CCD) so we never compete with game threads */
    sched_setaffinity(0, sizeof(cpu_set_t), target_mask);

    /*
     * Hold /dev/cpu_dma_latency open at 0 for the lifetime of this process.
     * This prevents the CPU from entering deep C-states (C2+) while gaming
     * mode is active, eliminating the wake-up latency penalty on the next
     * interrupt or scheduler tick. Restored automatically when this process
     * exits (i.e. when irq_watcher_stop() kills us).
     */
    int latency_fd = open("/dev/cpu_dma_latency", O_WRONLY);
    if (latency_fd >= 0) {
        uint32_t target_latency = 0;
        (void)write(latency_fd, &target_latency, sizeof(target_latency));
        /* intentionally not closed — held for process lifetime */
    }

    fclose(stdin);
    fclose(stdout);
    fclose(stderr);

    while (1) {
        sleep(2);
        steer_irqs(target_mask, 0);
    }
}

int irq_read_mask(int irq, char *buf, size_t size)
{
    if (irq <= 0 || !buf || size == 0)
        return 1;

    char path[128];
    snprintf(path, sizeof(path),
             "/proc/irq/%d/smp_affinity", irq);

    FILE *f = fopen(path, "r");
    if (!f)
        return 1;

    if (!fgets(buf, size, f)) {
        fclose(f);
        return 1;
    }

    buf[strcspn(buf, "\n")] = '\0';
    fclose(f);
    return 0;
}
