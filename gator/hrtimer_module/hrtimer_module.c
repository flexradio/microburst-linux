#include <linux/cpu.h>
#include <linux/hrtimer.h>
#include <linux/module.h>

DEFINE_PER_CPU(struct hrtimer, percpu_hrtimer);
DEFINE_PER_CPU(int, hrtimer_is_active);
DEFINE_PER_CPU(ktime_t, hrtimer_expire);
DEFINE_PER_CPU(int, hrtimer_count);
DEFINE_PER_CPU(s64, percpu_timestamp);
static int hrtimer_running;
static ktime_t interval;

volatile int p;
int qwork2(void)
{
	int q = 0;
	p = 0;
	while (p < 10000000) {
		q++;
		p++;
	}
	return q;
}

int qwork(void)
{
	int q = 0;
	p = 0;
	while (p < 10000000) {
		q++;
		p++;
	}
	return q;
}

int do_some_work(int x)
{
	int q;
	if (x & 1)
		q = qwork();
	else
		q = qwork2();
	return q;
}

static enum hrtimer_restart hrtimer_notify(struct hrtimer *hrtimer)
{
	int cpu, count;
	cpu = smp_processor_id();
	hrtimer_forward(hrtimer, per_cpu(hrtimer_expire, cpu), interval);
	per_cpu(hrtimer_expire, cpu) = ktime_add(per_cpu(hrtimer_expire, cpu), interval);
	count = per_cpu(hrtimer_count, cpu)++;
	// do_some_work(count);
	if (count % 1000 == 0) {
		struct timespec ts;
		s64 timestamp, last_timestamp;
		getnstimeofday(&ts);
		timestamp = timespec_to_ns(&ts);
		last_timestamp = per_cpu(percpu_timestamp, cpu);
		if (last_timestamp > 0) {
			const char * jitter_status;
			s64 jitter = timestamp - last_timestamp - NSEC_PER_SEC;
			if (jitter < 0) {
				jitter = -jitter;
			}
			if (jitter < NSEC_PER_SEC/10) {
				jitter_status = "pass";
			} else {
				jitter_status = "fail";
			}
			printk(KERN_ERR "core: %d hrtimer: %s (%lld)\n", cpu, jitter_status, jitter);
		}
		per_cpu(percpu_timestamp, cpu) = timestamp;
	}
	return HRTIMER_RESTART;
}

static void __timer_offline(void *unused)
{
	int cpu = smp_processor_id();
	if (per_cpu(hrtimer_is_active, cpu)) {
		struct hrtimer *hrtimer = &per_cpu(percpu_hrtimer, cpu);
		hrtimer_cancel(hrtimer);
		per_cpu(hrtimer_is_active, cpu) = 0;
	}
}

static void timer_offline(void)
{
	if (hrtimer_running) {
		hrtimer_running = 0;

		on_each_cpu(__timer_offline, NULL, 1);
	}
}

static void __timer_online(void *unused)
{
	int cpu = smp_processor_id();
	if (!per_cpu(hrtimer_is_active, cpu)) {
		struct hrtimer *hrtimer = &per_cpu(percpu_hrtimer, cpu);
		hrtimer_init(hrtimer, CLOCK_MONOTONIC, HRTIMER_MODE_ABS);
		hrtimer->function = hrtimer_notify;
		per_cpu(hrtimer_expire, cpu) = ktime_add(hrtimer->base->get_time(), interval);
		hrtimer_start(hrtimer, per_cpu(hrtimer_expire, cpu), HRTIMER_MODE_ABS_PINNED);
		per_cpu(hrtimer_is_active, cpu) = 1;
	}
}

static int timer_online(unsigned long setup)
{
	if (!setup) {
		pr_err("hrtimer_module: cannot start due to a hrtimer value of zero");
		return -1;
	} else if (hrtimer_running) {
		pr_notice("hrtimer_module: high res timer already running");
		return 0;
	}

	hrtimer_running = 1;
	interval = ns_to_ktime(1000000000UL / setup);
	on_each_cpu(__timer_online, NULL, 1);

	return 0;
}

static int __init hrtimer_module_init(void)
{
	printk(KERN_ERR "hrtimer module init\n");
	timer_online(1000); // number of interrupts per second per core
	return 0;
}

static void __exit hrtimer_module_exit(void)
{
	printk(KERN_ERR "hrtimer module exit\n");
	timer_offline();
}

module_init(hrtimer_module_init);
module_exit(hrtimer_module_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("ARM Ltd");
MODULE_DESCRIPTION("hrtimer module");
