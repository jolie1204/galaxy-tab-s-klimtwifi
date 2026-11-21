// SPDX-License-Identifier: GPL-2.0-only
/* Bounded SM-T700 bootstrap diagnostic, included from selinuxfs.c. */
#include <linux/magic.h>
#include <linux/mount.h>
#include <linux/reboot.h>
#include <linux/task_work.h>
#include <linux/workqueue.h>

static bool klimt_committed, klimt_before_ok, klimt_linker_ok, klimt_finished;
static int klimt_apex_pid;
static char klimt_run[17];
static struct workqueue_struct *klimt_queue;

static void klimt_return(struct work_struct *work)
{
	pr_notice("KLIMT-APEX RECOVERY reason=%s\n",
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
	queue_delayed_work(klimt_queue, &klimt_reboot, 70 * HZ);
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

/* Packaged manifest sizes come from the installed SYSTEM APEX payloads. */
static const struct klimt_apex_file {
	const char *path;
	loff_t size;
} klimt_manifests[] = {
	{ "/apex/com.android.runtime/apex_manifest.pb", 49 },
	{ "/apex/com.android.i18n/apex_manifest.pb", 151 },
	{ "/apex/com.android.tzdata/apex_manifest.pb", 26 },
};

static bool klimt_apex_paths(bool after)
{
	bool ok = true;
	unsigned int i;
	struct path p;
	int err;

	for (i = 0; i < ARRAY_SIZE(klimt_manifests); ++i) {
		const struct klimt_apex_file *file = &klimt_manifests[i];
		err = kern_path(file->path, LOOKUP_NO_SYMLINKS, &p);
		if (err) {
			ok &= !after && err == -ENOENT;
			pr_notice("KLIMT-APEX manifest=%s lookup=%d\n", file->path, err);
			continue;
		}
		struct inode *ino = d_inode(p.dentry);
		bool ro = (p.mnt->mnt_flags & MNT_READONLY) || sb_rdonly(ino->i_sb);
		loff_t size = i_size_read(ino);

		ok &= after && S_ISREG(ino->i_mode) && size == file->size &&
		      ro && ino->i_sb->s_magic == EXT4_SUPER_MAGIC && ino->i_sb->s_bdev;
		pr_notice("KLIMT-APEX manifest=%s bytes=%lld dev=%u:%u ro=%u\n",
			  file->path, size, MAJOR(ino->i_sb->s_dev), MINOR(ino->i_sb->s_dev), ro);
		path_put(&p);
	}

	err = kern_path("/linkerconfig/ld.config.txt", LOOKUP_NO_SYMLINKS, &p);
	if (err)
		return false;
	{
		struct inode *ino = d_inode(p.dentry);
		loff_t size = i_size_read(ino);

		pr_notice("KLIMT-APEX linker_config_bytes=%lld\n", size);
		ok &= S_ISREG(ino->i_mode) && ino->i_sb->s_magic == TMPFS_MAGIC &&
		      (after ? size > 1 : size == 1);
	}
	path_put(&p);
	return ok;
}

static void klimt_before(struct callback_head *work)
{
	klimt_before_ok = klimt_apex_paths(false);
}

static void klimt_after(struct callback_head *work)
{
	bool paths = klimt_apex_paths(true);
	int pid = READ_ONCE(klimt_apex_pid);
	bool pass = klimt_before_ok && paths && pid && klimt_linker_ok &&
		    READ_ONCE(klimt_committed) && enforcing_enabled() &&
		    klimt_in_domain("u:r:init:s0");

	pr_notice("KLIMT-APEX %s run=%s before=%u mounts_config=%u apexd_pid=%d linker=%u enforcing=%u\n",
		  pass ? "PASS" : "FAIL", klimt_run, klimt_before_ok, paths, pid,
		  klimt_linker_ok, enforcing_enabled());
	WRITE_ONCE(klimt_finished, true);
	mod_delayed_work(klimt_queue, &klimt_reboot, 6 * HZ);
	for (;;)
		schedule_timeout_uninterruptible(MAX_SCHEDULE_TIMEOUT);
}
static struct callback_head klimt_pre_work = { .func = klimt_before };
static struct callback_head klimt_post_work = { .func = klimt_after };

void klimt_trial_log(const char *line)
{
	char expected[80];
	size_t prefix;

	if (current->pid != 1) {
		if (str_has_prefix(line, "apexd-bootstrap: OnBootstrap done, duration=") &&
		    enforcing_enabled() && klimt_in_domain("u:r:apexd:s0")) {
			WRITE_ONCE(klimt_apex_pid, current->tgid);
			pr_notice("KLIMT-APEX bootstrap pid=%d context=u:r:apexd:s0 enforcing=1\n",
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
	if (!klimt_run[0])
		return;
	if (sysfs_streq(line, "init: linkerconfig generated /linkerconfig with mounted APEX modules info"))
		klimt_linker_ok = true;
	snprintf(expected, sizeof(expected), "KLIMT-APEX BEFORE %s", klimt_run);
	if (sysfs_streq(line, expected))
		task_work_add(current, &klimt_pre_work, TWA_RESUME);
	snprintf(expected, sizeof(expected), "KLIMT-APEX AFTER %s cold=true", klimt_run);
	if (sysfs_streq(line, expected))
		task_work_add(current, &klimt_post_work, TWA_RESUME);
}
