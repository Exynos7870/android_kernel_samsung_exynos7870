/*
 * Task Integrity Verifier
 *
 * Copyright (C) 2016 Samsung Electronics, Inc.
 * Egor Uleyskiy, <e.uleyskiy@samsung.com>
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef _LINUX_TASK_INTEGRITY_H
#define _LINUX_TASK_INTEGRITY_H

#include <linux/atomic.h>
#include <linux/spinlock.h>
#include <linux/sched.h>
#include <linux/list.h>

struct linux_binprm;
struct task_integrity;

struct integrity_label {
	uint16_t len;
	uint8_t data[0];
} __packed;

enum five_event {
	FIVE_RESET_INTEGRITY,
	FIVE_VERIFY_BUNCH_FILES,
};

/*
 * This is list of numbers which Hamming distance is 16
 *
 * 0x00000000 <-- used
 * 0x0f0f0f0f,
 * 0xf0f0f0f0,
 * 0x0000ffff,
 * 0xffff0000,
 * 0xff00ff00,
 * 0x00ff00ff,
 * 0x33333333,<-- used
 * 0xcccccccc,<-- used
 * 0x55555555,<-- used
 * 0x5a5a5a5a,<-- used
 * 0xa5a5a5a5,
 * 0xaaaaaaaa,<-- used
 * 0x3c3c3c3c,<-- used
 * 0xc3c3c3c3,
 * 0xffffffff <-- used
 */
enum task_integrity_value {
	INTEGRITY_NONE                = 0x00000000,
	INTEGRITY_PRELOAD             = 0x33333333,
	INTEGRITY_PRELOAD_ALLOW_SIGN  = 0xcccccccc,
	INTEGRITY_MIXED               = 0x55555555,
	INTEGRITY_MIXED_ALLOW_SIGN    = 0x5a5a5a5a,
	INTEGRITY_DMVERITY            = 0xaaaaaaaa,
	INTEGRITY_DMVERITY_ALLOW_SIGN = 0x3c3c3c3c,
	INTEGRITY_PROCESSING          = 0xffffffff
};

struct processing_event_list {
	enum five_event event;
	struct task_struct *task;
	struct file *file;
	struct task_integrity *saved_integrity;
	int function;
	struct list_head list;
};

struct task_integrity {
	enum task_integrity_value user_value;
	atomic_t value;
	atomic_t usage_count;
	spinlock_t lock;
	struct integrity_label *label;
	struct processing_event_list events;
};

#ifdef CONFIG_FIVE

struct task_integrity *task_integrity_alloc(void);
void task_integrity_free(struct task_integrity *intg);
void task_integrity_clear(struct task_integrity *tint);

static inline void task_integrity_get(struct task_integrity *intg)
{
	BUG_ON(!atomic_read(&intg->usage_count));
	atomic_inc(&intg->usage_count);
}

static inline void task_integrity_put(struct task_integrity *intg)
{
	BUG_ON(!atomic_read(&intg->usage_count));
	if (atomic_dec_and_test(&intg->usage_count))
		task_integrity_free(intg);
}

static inline int __atomic_set_unless(atomic_t *v, int a, int u)
{
	int c, old;

	c = atomic_read(v);
	while (c != u && (old = atomic_cmpxchg((v), c, a)) != c)
		c = old;
	return c;
}

static inline void task_integrity_set_unless(struct task_integrity *intg,
					enum task_integrity_value value,
					enum task_integrity_value unless)
{
	__atomic_set_unless(&intg->value, value, unless);
}

static inline void task_integrity_set(struct task_integrity *intg,
					enum task_integrity_value value)
{
	atomic_set(&intg->value, value);
}

static inline void task_integrity_cmpxchg(struct task_integrity *intg,
		enum task_integrity_value old,
		enum task_integrity_value new)
{
	atomic_cmpxchg(&intg->value, old, new);
}

static inline void task_integrity_reset(struct task_integrity *intg)
{
	task_integrity_set(intg, INTEGRITY_NONE);
}

extern void task_integrity_delayed_reset(struct task_struct *task);

static inline enum task_integrity_value task_integrity_read(
						struct task_integrity *intg)
{
	return atomic_read(&intg->value);
}

static inline enum task_integrity_value task_integrity_user_read(
						struct task_integrity *intg)
{
	enum task_integrity_value intg_user;

	spin_lock(&intg->lock);
	intg_user = intg->user_value;
	spin_unlock(&intg->lock);

	return intg_user;
}

static inline void task_integrity_user_set(struct task_integrity *intg,
					   enum task_integrity_value value)
{
	spin_lock(&intg->lock);
	intg->user_value = value;
	spin_unlock(&intg->lock);
}

static inline void task_integrity_reset_both(struct task_integrity *intg)
{
	spin_lock(&intg->lock);
	task_integrity_reset(intg);
	intg->user_value = INTEGRITY_NONE;
	spin_unlock(&intg->lock);
}

extern int task_integrity_copy(struct task_integrity *from,
				struct task_integrity *to);
extern int five_bprm_check(struct linux_binprm *bprm);
extern void five_file_free(struct file *file);
extern int five_file_mmap(struct file *file, unsigned long prot);
extern void five_task_free(struct task_struct *task);

extern void five_inode_post_setattr(struct task_struct *tsk,
					struct dentry *dentry);
extern int five_inode_setxattr(struct dentry *dentry, const char *xattr_name,
			const void *xattr_value, size_t xattr_value_len);
extern int five_inode_removexattr(struct dentry *dentry,
					const char *xattr_name);
extern int five_fcntl_sign(struct file *file,
				struct integrity_label __user *label);
extern int five_fcntl_verify_async(struct file *file);
extern int five_fcntl_verify_sync(struct file *file);
extern int five_fork(struct task_struct *task, struct task_struct *child_task);
extern int five_ptrace(struct task_struct *task, long request);
extern int five_process_vm_rw(struct task_struct *task, int write);

#ifdef CONFIG_FIVE_PA_FEATURE
extern int fivepa_fcntl_setxattr(struct file *file, void __user *lv_xattr);
#endif /* CONFIG_FIVE_PA_FEATURE */

#else
static inline struct task_integrity *task_integrity_alloc(void)
{
	return NULL;
}

static inline void task_integrity_free(struct task_integrity *intg)
{
}

static inline void task_integrity_clear(struct task_integrity *tint)
{
}

static inline void task_integrity_set(struct task_integrity *intg,
						enum task_integrity_value value)
{
}

static inline void task_integrity_set_unless(struct task_integrity *intg,
					enum task_integrity_value value,
					enum task_integrity_value unless)
{
}

static inline void task_integrity_cmpxchg(struct task_integrity *intg,
		enum task_integrity_value old,
		enum task_integrity_value new)
{
}

static inline void task_integrity_reset(struct task_integrity *intg)
{
}

static inline enum task_integrity_value task_integrity_read(
						struct task_integrity *intg)
{
	return INTEGRITY_NONE;
}

static inline void task_integrity_user_set(struct task_integrity *intg,
						enum task_integrity_value value)
{
}

static inline enum task_integrity_value task_integrity_user_read(
						struct task_integrity *intg)
{
	return INTEGRITY_NONE;
}

static inline void task_integrity_delayed_reset(struct task_struct *task)
{
}

static inline void task_integrity_reset_both(struct task_integrity *intg)
{
}

static inline void task_integrity_add_file(struct task_integrity *intg)
{
}

static inline void task_integrity_report_file(struct task_integrity *intg)
{
}

static inline int five_bprm_check(struct linux_binprm *bprm)
{
	return 0;
}

static inline void five_file_free(struct file *file)
{
}

static inline int five_file_mmap(struct file *file, unsigned long prot)
{
	return 0;
}

static inline void five_task_free(struct task_struct *task)
{
}

static inline void five_inode_post_setattr(struct task_struct *tsk,
							struct dentry *dentry)
{
}

static inline int five_inode_setxattr(struct dentry *dentry,
					const char *xattr_name,
					const void *xattr_value,
					size_t xattr_value_len)
{
	return 0;
}

static inline int five_inode_removexattr(struct dentry *dentry,
					const char *xattr_name)
{
	return 0;
}

static inline int five_fcntl_sign(struct file *file,
					struct integrity_label __user *label)
{
	return 0;
}

static inline int five_fcntl_verify_async(struct file *file)
{
	return 0;
}

static inline int five_fcntl_verify_sync(struct file *file)
{
	return 0;
}

static inline int five_fork(struct task_struct *task,
				struct task_struct *child_task)
{
	return 0;
}

static inline int five_ptrace(struct task_struct *task, long request)
{
	return 0;
}

static inline int five_process_vm_rw(struct task_struct *task, int write)
{
	return 0;
}

static inline int task_integrity_copy(struct task_integrity *from,
				struct task_integrity *to)
{
	return 0;
}
#endif

#endif /* _LINUX_TASK_INTEGRITY_H */
