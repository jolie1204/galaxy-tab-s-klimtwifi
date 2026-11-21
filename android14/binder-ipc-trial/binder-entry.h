// SPDX-License-Identifier: GPL-2.0-only
/* Bounded SM-T700 Binder smoke test, included from selinuxfs.c. */
#include <linux/magic.h>
#include <linux/reboot.h>
#include <linux/task_work.h>
#include <linux/workqueue.h>

static bool klimt_committed, klimt_linker_ok, klimt_started, klimt_finished;
static char klimt_run[17];
static struct workqueue_struct *klimt_queue;

static void klimt_return(struct work_struct *work)
{
	pr_notice("KLIMT-BINDER RECOVERY reason=%s\n",
		  READ_ONCE(klimt_finished) ? "verdict" : "timeout");
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
	queue_delayed_work(klimt_queue, &klimt_reboot, 65 * HZ);
	return 0;
}
late_initcall(klimt_start_timeout);

static bool klimt_in_domain(const char *expected)
{
	char *context = NULL;
	u32 size = 0;
	bool match = !security_sid_to_context(current_sid(), &context, &size) &&
		     !strcmp(context, expected);
	kfree(context);
	return match;
}

static bool klimt_binder_device(const char *name)
{
	char link[32], device[40];
	struct path resolved, direct;
	bool ok = false;
	int err;

	snprintf(link, sizeof(link), "/dev/%s", name);
	snprintf(device, sizeof(device), "/dev/binderfs/%s", name);
	err = kern_path(link, LOOKUP_FOLLOW, &resolved);
	if (err)
		return false;
	err = kern_path(device, LOOKUP_NO_SYMLINKS, &direct);
	if (!err) {
		struct inode *inode = d_inode(direct.dentry);

		ok = path_equal(&direct, &resolved) && S_ISCHR(inode->i_mode) &&
		     (inode->i_mode & 0777) == 0666 &&
		     inode->i_sb->s_magic == BINDERFS_SUPER_MAGIC;
		pr_notice("KLIMT-BINDER node=%s dev=%u:%u valid=%u\n",
			  name, MAJOR(inode->i_rdev), MINOR(inode->i_rdev), ok);
		path_put(&direct);
	}
	path_put(&resolved);
	return ok;
}

static void klimt_after(struct callback_head *work)
{
	bool binder = klimt_binder_device("binder");
	bool hw = klimt_binder_device("hwbinder");
	bool vendor = klimt_binder_device("vndbinder");
	bool pass = binder && hw && vendor && klimt_started && klimt_linker_ok &&
		    READ_ONCE(klimt_committed) && enforcing_enabled() &&
		    klimt_in_domain("u:r:init:s0");

	pr_notice("KLIMT-BINDER %s run=%s devices=%u init_sequence=%u enforcing=%u\n",
		  pass ? "SETUP_OK" : "FAIL", klimt_run, binder && hw && vendor,
		  klimt_started && klimt_linker_ok, enforcing_enabled());
	WRITE_ONCE(klimt_finished, true);
	mod_delayed_work(klimt_queue, &klimt_reboot, 6 * HZ);
	for (;;)
		schedule_timeout_uninterruptible(MAX_SCHEDULE_TIMEOUT);
}
static struct callback_head klimt_post_work = { .func = klimt_after };

void klimt_trial_log(const char *line)
{
	char expected[100];
	size_t prefix;

	if (current->pid != 1)
		return;
	prefix = str_has_prefix(line, "init: KLIMT-FIRST ");
	if (!klimt_run[0] && prefix && strspn(line + prefix, "0123456789abcdef") == 16 &&
	    sysfs_streq(line + prefix + 16, " NATIVE_PID1_STARTED pid=1")) {
		memcpy(klimt_run, line + prefix, 16);
		return;
	}
	if (!klimt_run[0])
		return;
	if (sysfs_streq(line, "init: linkerconfig generated /linkerconfig with mounted APEX modules info"))
		klimt_linker_ok = true;
	snprintf(expected, sizeof(expected), "KLIMT-BINDER BEGIN %s cold=true ready=unset", klimt_run);
	if (sysfs_streq(line, expected))
		klimt_started = true;
	snprintf(expected, sizeof(expected), "KLIMT-BINDER END %s ready=true client=verified", klimt_run);
	if (sysfs_streq(line, expected))
		task_work_add(current, &klimt_post_work, TWA_RESUME);
}
