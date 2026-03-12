/* Public domain. */

#ifndef _LINUX_PM_H
#define _LINUX_PM_H

#include <linux/completion.h>

struct dev_pm_ops {
	int (*prepare)(struct device *);
	void (*complete)(struct device *);
	int (*suspend)(struct device *);
	int (*resume)(struct device *);
	int (*freeze)(struct device *);
	int (*thaw)(struct device *);
	int (*poweroff)(struct device *);
	int (*restore)(struct device *);
	int (*runtime_suspend)(struct device *);
	int (*runtime_resume)(struct device *);
	int (*runtime_idle)(struct device *);
};

#define DEFINE_SIMPLE_DEV_PM_OPS(name, suspend_fn, resume_fn)	\
    const struct dev_pm_ops name = { 				\
	    .suspend = suspend_fn, .resume = resume_fn		\
    }

struct dev_pm_domain {
};

typedef struct pm_message {
	int event;
} pm_message_t;

/* PM notifier events are defined as an enum in linux/suspend.h */

#endif
