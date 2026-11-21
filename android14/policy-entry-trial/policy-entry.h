// SPDX-License-Identifier: GPL-2.0-only
/* SM-T700 lab kernel only. Included by security/selinux/selinuxfs.c. */
#include <linux/reboot.h>
#include <linux/task_work.h>
#include <linux/workqueue.h>

static struct workqueue_struct *klimt_queue;
static bool klimt_committed;
static bool klimt_at_entry;

static void klimt_return(struct work_struct *work)
{
	pr_notice("KLIMT-POLICY RECOVERY reason=%s\n",
		  READ_ONCE(klimt_at_entry) ? "entry" : "timeout");
	kernel_restart("recovery");
}
static DECLARE_DELAYED_WORK(klimt_reboot, klimt_return);

static int __init klimt_start_timeout(void)
{
	klimt_queue = alloc_ordered_workqueue("klimt-recovery", WQ_MEM_RECLAIM);
	if (!klimt_queue) {
		kernel_restart("recovery");
		return -ENOMEM;
	}
	queue_delayed_work(klimt_queue, &klimt_reboot, 55 * HZ);
	return 0;
}
late_initcall(klimt_start_timeout);

static void klimt_check_entry(struct callback_head *work)
{
	char *context = NULL;
	u32 length = 0;
	int ret = security_sid_to_context(current_sid(), &context, &length);
	bool pass = !ret && !strcmp(context, "u:r:init:s0") &&
		    READ_ONCE(klimt_committed) && enforcing_enabled();

	pr_notice("KLIMT-POLICY %s SECOND_STAGE pid=%d enforcing=%u context=%s\n",
		  pass ? "PASS" : "FAIL", current->pid, enforcing_enabled(),
		  context ? context : "unknown");
	kfree(context);
	WRITE_ONCE(klimt_at_entry, true);
	mod_delayed_work(klimt_queue, &klimt_reboot, 6 * HZ);
	/* This runs on return to userspace, after the log write released its locks. */
	for (;;)
		schedule_timeout_uninterruptible(MAX_SCHEDULE_TIMEOUT);
}

static struct callback_head klimt_stop = { .func = klimt_check_entry };

void klimt_trial_log(const char *line)
{
	if (current->pid == 1 && sysfs_streq(line, "init: init second stage started!"))
		task_work_add(current, &klimt_stop, TWA_RESUME);
}
