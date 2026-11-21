// SPDX-License-Identifier: GPL-2.0-only
/* Isolated SM-T700 diagnostic, included from selinuxfs.c in place of policy-entry.h. */
#include <linux/magic.h>
#include <linux/reboot.h>
#include <linux/task_work.h>
#include <linux/workqueue.h>

static struct workqueue_struct *klimt_queue;
static bool klimt_committed;
static bool klimt_received;
static bool klimt_echo_ok;
static char klimt_run[17];

static void klimt_return(struct work_struct *work)
{
	pr_notice("KLIMT-PROP RECOVERY reason=%s\n",
		  READ_ONCE(klimt_received) ? "reply" : "timeout");
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

static bool klimt_verify_cgroups(void)
{
	static const struct {
		const char *path;
		unsigned long magic;
	} checks[] = {
		{ "/dev/blkio/blkio.throttle.read_bps_device", CGROUP_SUPER_MAGIC },
		{ "/dev/cpuctl/cpu.shares", CGROUP_SUPER_MAGIC },
		{ "/dev/cpuctl/klimt-check/cpu.uclamp.min", CGROUP_SUPER_MAGIC },
		{ "/dev/cpuset/cpus", CGROUP_SUPER_MAGIC },
		{ "/dev/memcg/memory.limit_in_bytes", CGROUP_SUPER_MAGIC },
		{ "/sys/fs/cgroup/klimt-check/cgroup.freeze", CGROUP2_SUPER_MAGIC },
	};
	bool pass = true;
	unsigned int i;

	for (i = 0; i < ARRAY_SIZE(checks); i++) {
		struct path path;
		int err = kern_path(checks[i].path, LOOKUP_FOLLOW, &path);
		unsigned long magic = 0;

		if (!err) {
			magic = path.dentry->d_sb->s_magic;
			path_put(&path);
		}
		pr_notice("KLIMT-PROP cgroup path=%s fs=%lx lookup=%d\n",
			  checks[i].path, magic, err);
		if (err || magic != checks[i].magic)
			pass = false;
	}
	return pass;
}

static void klimt_check_reply(struct callback_head *work)
{
	char *context = NULL;
	u32 length = 0;
	int ret = security_sid_to_context(current_sid(), &context, &length);
	bool cgroups = klimt_verify_cgroups();
	bool pass = cgroups && klimt_echo_ok && !ret && !strcmp(context, "u:r:init:s0") &&
		    READ_ONCE(klimt_committed) && enforcing_enabled();

	pr_notice("KLIMT-PROP %s run=%s echo=%u cgroups=%u enforcing=%u context=%s\n",
		  pass ? "PASS" : "FAIL", klimt_run, klimt_echo_ok, cgroups,
		  enforcing_enabled(), context ? context : "unknown");
	kfree(context);
	WRITE_ONCE(klimt_received, true);
	mod_delayed_work(klimt_queue, &klimt_reboot, 6 * HZ);
	/* Return-to-userspace callback; the VFS log write has released its locks. */
	for (;;)
		schedule_timeout_uninterruptible(MAX_SCHEDULE_TIMEOUT);
}
static struct callback_head klimt_stop = { .func = klimt_check_reply };

void klimt_trial_log(const char *line)
{
	const char *id;
	char expected[80];

	if (current->pid != 1)
		return;
	id = str_has_prefix(line, "init: KLIMT-FIRST ") ? line + strlen("init: KLIMT-FIRST ") : NULL;
	if (!klimt_run[0] && id && strspn(id, "0123456789abcdef") == 16 &&
	    sysfs_streq(id + 16, " NATIVE_PID1_STARTED pid=1")) {
		memcpy(klimt_run, id, 16);
		return;
	}
	if (!str_has_prefix(line, "KLIMT-PROP ECHO "))
		return;
	snprintf(expected, sizeof(expected), "KLIMT-PROP ECHO %s api=34 protocol=2", klimt_run);
	klimt_echo_ok = klimt_run[0] && sysfs_streq(line, expected);
	task_work_add(current, &klimt_stop, TWA_RESUME);
}
