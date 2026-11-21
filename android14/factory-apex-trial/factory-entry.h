// SPDX-License-Identifier: GPL-2.0-only
/* Bounded SM-T700 factory APEX diagnostic, included from selinuxfs.c. */
#include <linux/magic.h>
#include <linux/major.h>
#include <linux/mount.h>
#include <linux/nsproxy.h>
#include <linux/reboot.h>
#include <linux/task_work.h>
#include <linux/workqueue.h>

static bool klimt_committed, klimt_default_ok, klimt_linker_ok, klimt_finished;
static struct mnt_namespace *klimt_bootstrap_ns, *klimt_default_ns;
static int klimt_actor;
static char klimt_run[17];
static struct workqueue_struct *klimt_queue;

static void klimt_return(struct work_struct *work)
{
	pr_notice("KLIMT-FACTORY RECOVERY reason=%s\n",
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
	queue_delayed_work(klimt_queue, &klimt_reboot, 82 * HZ);
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

/* Module names from the installed system and vendor package inventory. */
static const char * const klimt_modules[] = {
	"adbd",
	"adservices",
	"apex.cts.shim",
	"appsearch",
	"art",
	"btservices",
	"configinfrastructure",
	"conscrypt",
	"devicelock",
	"extservices",
	"hardware.cas",
	"healthfitness",
	"i18n",
	"ipsec",
	"media",
	"media.swcodec",
	"mediaprovider",
	"neuralnetworks",
	"ondevicepersonalization",
	"os.statsd",
	"permission",
	"resolv",
	"rkpd",
	"runtime",
	"scheduling",
	"sdkext",
	"tethering",
	"tzdata",
	"uwb",
	"virt",
	"wifi",
};

static bool klimt_ram_data(void)
{
	struct path root, session;
	bool ok = false;

	if (kern_path("/data", LOOKUP_NO_SYMLINKS, &root))
		return false;
	if (!kern_path("/data/apex/sessions", LOOKUP_NO_SYMLINKS, &session)) {
		struct super_block *sb = root.dentry->d_sb;

		ok = root.mnt == session.mnt && sb->s_magic == TMPFS_MAGIC &&
		     !sb->s_bdev && !sb_rdonly(sb) && !(root.mnt->mnt_flags & MNT_READONLY);
		path_put(&session);
	}
	path_put(&root);
	return ok;
}

static unsigned int klimt_check_modules(bool activated)
{
	dev_t devices[ARRAY_SIZE(klimt_modules)] = {};
	unsigned int i, count = 0;

	for (i = 0; i < ARRAY_SIZE(klimt_modules); i++) {
		char name[110];
		struct path p;
		int err;

		snprintf(name, sizeof(name), "/apex/com.android.%s/apex_manifest.pb",
			 klimt_modules[i]);
		err = kern_path(name, LOOKUP_NO_SYMLINKS, &p);
		if (err) {
			if (!activated && err == -ENOENT)
				count++;
			else
				pr_notice("KLIMT-FACTORY module=%s lookup=%d\n", klimt_modules[i], err);
			continue;
		}

		if (activated) {
			struct inode *ino = d_inode(p.dentry);
			struct super_block *sb = ino->i_sb;
			dev_t dev = sb->s_dev;
			unsigned int j;
			bool ok;

			ok = S_ISREG(ino->i_mode) && i_size_read(ino) > 0 &&
			     sb->s_magic == EXT4_SUPER_MAGIC && sb->s_bdev &&
			     MAJOR(dev) == LOOP_MAJOR &&
			     (sb_rdonly(sb) || (p.mnt->mnt_flags & MNT_READONLY));
			for (j = 0; ok && j < count; j++)
				ok = devices[j] != dev;
			if (ok)
				devices[count++] = dev;
			pr_notice("KLIMT-FACTORY module=%s dev=%u:%u valid=%u\n",
				  klimt_modules[i], MAJOR(dev), MINOR(dev), ok);
		}
		path_put(&p);
	}
	return count;
}

static bool klimt_config_exists(const char *name, unsigned int min_size)
{
	struct path p;
	bool ok;

	if (kern_path(name, LOOKUP_NO_SYMLINKS, &p))
		return false;
	ok = p.dentry->d_sb->s_magic == TMPFS_MAGIC &&
	     S_ISREG(d_inode(p.dentry)->i_mode) &&
	     i_size_read(d_inode(p.dentry)) >= min_size;
	path_put(&p);
	return ok;
}

static void klimt_finish(void)
{
	WRITE_ONCE(klimt_finished, true);
	mod_delayed_work(klimt_queue, &klimt_reboot, 6 * HZ);
	for (;;)
		schedule_timeout_uninterruptible(MAX_SCHEDULE_TIMEOUT);
}

static void klimt_before(struct callback_head *work)
{
	unsigned int absent = klimt_check_modules(false);
	bool ram = klimt_ram_data();
	bool changed = klimt_bootstrap_ns && klimt_default_ns != klimt_bootstrap_ns;

	klimt_default_ok = ram && changed && absent == ARRAY_SIZE(klimt_modules);
	pr_notice("KLIMT-FACTORY clean_default=%u tmpfs=%u absent=%u\n", changed, ram, absent);
	if (!klimt_default_ok)
		klimt_finish();
}

static void klimt_skip(struct callback_head *work)
{
	unsigned int absent = klimt_check_modules(false);

	pr_notice("KLIMT-FACTORY SKIPPED run=%s absent=%u actor=%d\n",
		  klimt_run, absent, READ_ONCE(klimt_actor));
	klimt_finish();
}

static void klimt_after(struct callback_head *work)
{
	unsigned int count = klimt_check_modules(true);
	int actor = READ_ONCE(klimt_actor);
	bool ram = klimt_ram_data();
	bool config = klimt_linker_ok && klimt_config_exists("/linkerconfig/ld.config.txt", 2) &&
		      klimt_config_exists("/apex/apex-info-list.xml", 128);
	bool pass = count == ARRAY_SIZE(klimt_modules) && ram && config && actor &&
		    klimt_default_ok && READ_ONCE(klimt_committed) && enforcing_enabled() &&
		    klimt_in_domain("u:r:init:s0");

	pr_notice("KLIMT-FACTORY %s run=%s mounts=%u tmpfs=%u config=%u actor=%d enforcing=%u\n",
		  pass ? "PASS" : "FAIL", klimt_run, count, ram, config, actor,
		  enforcing_enabled());
	klimt_finish();
}
static struct callback_head klimt_pre_work = { .func = klimt_before };
static struct callback_head klimt_skip_work = { .func = klimt_skip };
static struct callback_head klimt_post_work = { .func = klimt_after };

void klimt_trial_log(const char *line)
{
	char expected[112];
	size_t prefix;

	if (current->pid != 1) {
		if (sysfs_streq(line, "apexd: Marking APEXd as activated") &&
		    current->nsproxy->mnt_ns == READ_ONCE(klimt_default_ns) &&
		    enforcing_enabled() && klimt_in_domain("u:r:apexd:s0")) {
			WRITE_ONCE(klimt_actor, current->tgid);
			pr_notice("KLIMT-FACTORY apexd pid=%d default_ns=1 enforcing=1\n",
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
	snprintf(expected, sizeof(expected), "KLIMT-FACTORY BOOTSTRAP %s status=unset", klimt_run);
	if (sysfs_streq(line, expected))
		klimt_bootstrap_ns = current->nsproxy->mnt_ns;
	snprintf(expected, sizeof(expected), "KLIMT-FACTORY DEFAULT %s status=unset", klimt_run);
	if (sysfs_streq(line, expected)) {
		WRITE_ONCE(klimt_default_ns, current->nsproxy->mnt_ns);
		task_work_add(current, &klimt_pre_work, TWA_RESUME);
	}
	snprintf(expected, sizeof(expected), "KLIMT-FACTORY SKIP %s status=unset", klimt_run);
	if (sysfs_streq(line, expected))
		task_work_add(current, &klimt_skip_work, TWA_RESUME);
	if (klimt_default_ns && sysfs_streq(line,
				"init: linkerconfig generated /linkerconfig with mounted APEX modules info"))
		klimt_linker_ok = true;
	snprintf(expected, sizeof(expected), "KLIMT-FACTORY END %s status=activated service=running",
		 klimt_run);
	if (sysfs_streq(line, expected))
		task_work_add(current, &klimt_post_work, TWA_RESUME);
}
