// SPDX-License-Identifier: GPL-2.0-only
/* SM-T700 diagnostic only, included from selinuxfs.c. */
#include <linux/magic.h>
#include <linux/of.h>
#include <linux/reboot.h>
#include <linux/task_work.h>
#include <linux/workqueue.h>

static struct workqueue_struct *klimt_queue;
static bool klimt_committed, klimt_finished, klimt_before_ok, klimt_after_ok;
static int klimt_actor;
static char klimt_run[17];

static void klimt_return(struct work_struct *work)
{
	pr_notice("KLIMT-COLD RECOVERY reason=%s\n",
		  READ_ONCE(klimt_finished) ? "coldboot" : "timeout");
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

static bool klimt_in_domain(const char *expected)
{
	char *context = NULL;
	u32 size = 0;
	bool match = !security_sid_to_context(current_sid(), &context, &size) &&
		     !strcmp(context, expected);
	kfree(context);
	return match;
}

struct klimt_device {
	const char *node;
	const char *sys_class;
	umode_t mode;
	unsigned int uid, gid;
	bool tablet_only;
};

static const struct klimt_device klimt_devices[] = {
	{ "/dev/full", "/sys/class/mem/full", 0666, 0, 0, false },
	{ "/dev/zero", "/sys/class/mem/zero", 0666, 0, 0, false },
	{ "/dev/input/event0", "/sys/class/input/event0", 0660, 0, 1004, true },
	{ "/dev/rtc0", "/sys/class/rtc/rtc0", 0640, 1000, 1000, true },
};

static bool klimt_sysfs_matches(const char *sys_class, dev_t dev)
{
	char name[48];
	struct path cls, by_number;
	bool same = false;

	snprintf(name, sizeof(name), "/sys/dev/char/%u:%u", MAJOR(dev), MINOR(dev));
	if (kern_path(sys_class, LOOKUP_FOLLOW, &cls))
		return false;
	if (!kern_path(name, LOOKUP_FOLLOW, &by_number)) {
		same = path_equal(&cls, &by_number) &&
		       cls.dentry->d_sb->s_magic == SYSFS_MAGIC;
		path_put(&by_number);
	}
	path_put(&cls);
	return same;
}

static bool klimt_nodes(bool after)
{
	bool tablet = of_machine_is_compatible("samsung,klimt-wifi");
	bool all = true;
	unsigned int i;

	for (i = 0; i < ARRAY_SIZE(klimt_devices); i++) {
		const struct klimt_device *dev = &klimt_devices[i];
		struct path p;
		int err;
		bool valid;

		if (dev->tablet_only && !tablet)
			continue;
		err = kern_path(dev->node, LOOKUP_NO_SYMLINKS, &p);
		valid = !after && err == -ENOENT;
		if (!err) {
			struct inode *ino = d_inode(p.dentry);
			umode_t mode = ino->i_mode;
			bool same = S_ISCHR(mode) && klimt_sysfs_matches(dev->sys_class, ino->i_rdev);

			valid = after && same && p.dentry->d_sb->s_magic == TMPFS_MAGIC &&
				(mode & 07777) == dev->mode && i_uid_read(ino) == dev->uid &&
				i_gid_read(ino) == dev->gid;
			pr_notice("KLIMT-COLD device=%s dev=%u:%u mode=%04o uid=%u gid=%u sysfs=%u\n",
				  dev->node, MAJOR(ino->i_rdev), MINOR(ino->i_rdev), mode & 07777,
				  i_uid_read(ino), i_gid_read(ino), same);
			path_put(&p);
		}
		pr_notice("KLIMT-COLD phase=%s node=%s lookup=%d valid=%u\n",
			  after ? "after" : "before", dev->node, err, valid);
		all &= valid;
	}
	return all;
}

static void klimt_before(struct callback_head *work)
{
	klimt_before_ok = klimt_nodes(false) && klimt_before_ok;
}

static void klimt_after(struct callback_head *work)
{
	bool nodes = klimt_nodes(true);
	bool init = klimt_in_domain("u:r:init:s0");
	bool pass = klimt_before_ok && klimt_after_ok && nodes && init &&
		    READ_ONCE(klimt_actor) && READ_ONCE(klimt_committed) && enforcing_enabled();

	pr_notice("KLIMT-COLD %s run=%s before=%u after=%u nodes=%u actor=%d init=%u enforcing=%u\n",
		  pass ? "PASS" : "FAIL", klimt_run, klimt_before_ok, klimt_after_ok,
		  nodes, READ_ONCE(klimt_actor), init, enforcing_enabled());
	WRITE_ONCE(klimt_finished, true);
	mod_delayed_work(klimt_queue, &klimt_reboot, 6 * HZ);
	for (;;)
		schedule_timeout_uninterruptible(MAX_SCHEDULE_TIMEOUT);
}
static struct callback_head klimt_pre_work = { .func = klimt_before };
static struct callback_head klimt_post_work = { .func = klimt_after };

void klimt_trial_log(const char *line)
{
	size_t prefix;
	char expected[92];

	if (current->pid != 1) {
		if (sysfs_streq(line, "ueventd: ueventd started!") &&
		    klimt_in_domain("u:r:ueventd:s0") && enforcing_enabled()) {
			WRITE_ONCE(klimt_actor, current->tgid);
			pr_notice("KLIMT-COLD daemon pid=%d context=u:r:ueventd:s0 enforcing=1\n",
				  current->tgid);
		}
		return;
	}
	prefix = str_has_prefix(line, "init: KLIMT-FIRST ");
	if (!klimt_run[0] && prefix && strspn(line + prefix, "0123456789abcdef") == 16 &&
	    sysfs_streq(line + prefix + 16, " NATIVE_PID1_STARTED pid=1")) {
		memcpy(klimt_run, line + prefix, 16);
		return;
	}
	if (str_has_prefix(line, "KLIMT-COLD BEFORE ")) {
		snprintf(expected, sizeof(expected), "KLIMT-COLD BEFORE %s cold=unset", klimt_run);
		klimt_before_ok = klimt_run[0] && sysfs_streq(line, expected);
		task_work_add(current, &klimt_pre_work, TWA_RESUME);
	} else if (str_has_prefix(line, "KLIMT-COLD AFTER ")) {
		snprintf(expected, sizeof(expected), "KLIMT-COLD AFTER %s cold=true svc=running", klimt_run);
		klimt_after_ok = klimt_run[0] && sysfs_streq(line, expected);
		task_work_add(current, &klimt_post_work, TWA_RESUME);
	}
}
