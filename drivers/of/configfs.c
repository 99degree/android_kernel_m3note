/*
 * Configfs entries for device-tree
 *
 * Copyright (C) 2013 - Pantelis Antoniou <panto@antoniou-consulting.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version
 * 2 of the License, or (at your option) any later version.
 */
#include <linux/ctype.h>
#include <linux/cpu.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_fdt.h>
#include <linux/spinlock.h>
#include <linux/slab.h>
#include <linux/proc_fs.h>
#include <linux/configfs.h>
#include <linux/types.h>
#include <linux/stat.h>
#include <linux/limits.h>
#include <linux/file.h>
#include <linux/vmalloc.h>
#include <linux/firmware.h>
#include <linux/version.h>

#include "of_private.h"

struct cfs_overlay_item {
	struct config_item	item;

	char			path[PATH_MAX];

	const struct firmware	*fw;
	struct device_node	*overlay;
	int			ov_id;

	void			*dtbo;
	int			dtbo_size;
};

ssize_t cfs_overlay_item_dtbo_show(struct config_item *item, \
           void *buf);
ssize_t cfs_overlay_item_dtbo_store(struct config_item *item, \
           void *buf, size_t count);

static int create_overlay(struct cfs_overlay_item *overlay, void *blob)
{
	int err;

	/* unflatten the tree */
	of_fdt_unflatten_tree(blob, &overlay->overlay);
	if (overlay->overlay == NULL) {
		pr_err("%s: failed to unflatten tree\n", __func__);
		err = -EINVAL;
		goto out_err;
	}
	pr_debug("%s: unflattened OK\n", __func__);

	/* mark it as detached */
	of_node_set_flag(overlay->overlay, OF_DETACHED);

	/* perform resolution */
	err = of_resolve_phandles(overlay->overlay);
	if (err != 0) {
		pr_err("%s: Failed to resolve tree\n", __func__);
		goto out_err;
	}
	pr_debug("%s: resolved OK\n", __func__);

	err = of_overlay_create(overlay->overlay);
	if (err < 0) {
		pr_err("%s: Failed to create overlay (err=%d)\n",
				__func__, err);
		goto out_err;
	}
	overlay->ov_id = err;

out_err:
	return err;
}

static inline struct cfs_overlay_item *to_cfs_overlay_item(
		struct config_item *item)
{
	return item ? container_of(item, struct cfs_overlay_item, item) : NULL;
}

static ssize_t cfs_overlay_item_path_show(struct config_item *item,
		void *page)
{
	struct cfs_overlay_item *overlay = to_cfs_overlay_item(item);
	return sprintf((char *)page, "%s\n", overlay->path);
}

static ssize_t cfs_overlay_item_path_store(struct config_item *item,
		void *page, size_t count)
{
	struct cfs_overlay_item *overlay = to_cfs_overlay_item(item);
	const char *p = (char *)page;
	char *s;
	int err;

	/* if it's set do not allow changes */
	if (overlay->path[0] != '\0' || overlay->dtbo_size > 0)
		return -EPERM;

	/* copy to path buffer (and make sure it's always zero terminated */
	count = snprintf(overlay->path, sizeof(overlay->path) - 1, "%s", p);
	overlay->path[sizeof(overlay->path) - 1] = '\0';

	/* strip trailing newlines */
	s = overlay->path + strlen(overlay->path);
	while (s > overlay->path && *--s == '\n')
		*s = '\0';

	pr_debug("%s: path is '%s'\n", __func__, overlay->path);

	err = request_firmware(&overlay->fw, overlay->path, NULL);
	if (err != 0)
		goto out_err;

	err = create_overlay(overlay, (void *)overlay->fw->data);
	if (err < 0)
		goto out_err;

	return count;

out_err:

	release_firmware(overlay->fw);
	overlay->fw = NULL;

	overlay->path[0] = '\0';
	return err;
}

static ssize_t cfs_overlay_item_status_show(struct config_item *item,
		void *page)
{
	struct cfs_overlay_item *overlay = to_cfs_overlay_item(item);

	return sprintf(page, "%s\n",
			overlay->ov_id >= 0 ? "applied" : "unapplied");
}

#if 0
CONFIGFS_ATTR(cfs_overlay_item_, path);
CONFIGFS_ATTR_RO(cfs_overlay_item_, status);
CONFIGFS_ATTR(cfs_overlay_item_, dtbo);
#endif

//CONFIGFS_ATTR_STRUCT(cfs_overlay_item);
struct cfs_overlay_item_attribute {						\
	struct configfs_attribute attr;					\
	ssize_t (*show)(struct config_item *, void *);			\
	ssize_t (*store)(struct config_item *, void *, size_t);		\
};

#define CFS_OVERLAY_ITEM_ATTR(_name, _mode, _show, _store)      \
struct cfs_overlay_item_attribute cfs_overlay_item_attr_##_name = \
          __CONFIGFS_ATTR(_name, _mode, _show, _store) \

#define CFS_OVERLAY_ITEM_ATTR_RO(_name, _show)  \
struct cfs_overlay_item_attribute cfs_overlay_item_attr_##_name = \
              __CONFIGFS_ATTR_RO(_name, _show)

CFS_OVERLAY_ITEM_ATTR(dtbo, S_IRUGO | S_IWUSR, \
              cfs_overlay_item_dtbo_show, cfs_overlay_item_dtbo_store);
CFS_OVERLAY_ITEM_ATTR(path, S_IRUGO | S_IWUSR, \
                cfs_overlay_item_path_show, cfs_overlay_item_path_store);
CFS_OVERLAY_ITEM_ATTR_RO(status, \
                cfs_overlay_item_status_show);


#define CONFIGFS_ATTR_NEW_OPS(_item)					\
static ssize_t _item##_attr_show(struct config_item *item,		\
				 struct configfs_attribute *attr,	\
				 char *page)				\
{									\
	struct _item __maybe_unused *_item = to_##_item(item);				\
	struct _item##_attribute *_item##_attr =			\
		container_of(attr, struct _item##_attribute, attr);	\
	ssize_t ret = 0;						\
									\
	if (_item##_attr->show)						\
		ret = _item##_attr->show(item, (void*)page);			\
	return ret;							\
}									\
static ssize_t _item##_attr_store(struct config_item *item,		\
				  struct configfs_attribute *attr,	\
				  const char *page, size_t count)	\
{									\
	struct _item __maybe_unused *_item = to_##_item(item);				\
	struct _item##_attribute *_item##_attr =			\
		container_of(attr, struct _item##_attribute, attr);	\
	ssize_t ret = -EINVAL;						\
									\
	if (_item##_attr->store)					\
		ret = _item##_attr->store(item, (void *)page, count);		\
	return ret;							\
}

//make wrapper from config_item_operation to cfs_overlay_item internal show/store
CONFIGFS_ATTR_NEW_OPS(cfs_overlay_item);

static struct configfs_item_operations cfs_overlay_group_anonymous_item_ops = {
	.show_attribute = cfs_overlay_item_attr_show,
	.store_attribute = cfs_overlay_item_attr_store,
};

#if 0
static struct configfs_attribute *cfs_overlay_group_anonymous_item_attrs[] = {
	&(cfs_overlay_item_attr_path.attr),
	&(cfs_overlay_item_attr_status.attr),
	&(cfs_overlay_item_attr_dtbo.attr),
	NULL,
};
#endif
ssize_t cfs_overlay_item_dtbo_show(struct config_item *item,
		void *buf)
{
	struct cfs_overlay_item *overlay = to_cfs_overlay_item(item);
	size_t max_count = PAGE_SIZE;

	pr_err("[%s] name=%s, buf=%p max_count=%zu\n", __func__,
			item->ci_name, buf, max_count);

	if (overlay->dtbo == NULL)
		return 0;

	/* copy if buffer provided */
	if (buf != NULL) {
		/* the buffer must be large enough */
		if (overlay->dtbo_size > max_count)
			return -ENOSPC;

		memcpy(buf, overlay->dtbo, overlay->dtbo_size);
	}

	return overlay->dtbo_size;
}

ssize_t cfs_overlay_item_dtbo_store(struct config_item *item,
		void *buf, size_t count)
{
	struct cfs_overlay_item *overlay = to_cfs_overlay_item(item);
	int err;

	pr_err("[%s]: name=[%s]\n", __func__, item->ci_name);
	/* if it's set do not allow changes */
	if (overlay->path[0] != '\0' || overlay->dtbo_size > 0)
		return -EPERM;

	/* copy the contents */
	overlay->dtbo = kmemdup(buf, count, GFP_KERNEL);
	if (overlay->dtbo == NULL)
		return -ENOMEM;

	overlay->dtbo_size = count;

	err = create_overlay(overlay, overlay->dtbo);
	if (err < 0)
		goto out_err;

	return count;

out_err:
	kfree(overlay->dtbo);
	overlay->dtbo = NULL;
	overlay->dtbo_size = 0;

	return err;
}

#if 0
CONFIGFS_BIN_ATTR(cfs_overlay_item_, dtbo, NULL, SZ_1M);
static struct configfs_attribute __maybe_unused *cfs_overlay_bin_attrs[] = {
	&cfs_overlay_item_attr_dtbo,
	NULL,
};
#endif

static struct configfs_attribute *cfs_overlay_group_anonymous_item_attrs[] =
{
	&(cfs_overlay_item_attr_path.attr),
	&(cfs_overlay_item_attr_status.attr),
	&(cfs_overlay_item_attr_dtbo.attr),
	NULL,
};

#if 0
static struct config_item_type *cfs_overlay_group_anonymous_item_type = {
        	.ct_item_ops    = &cfs_overlay_item_attr_ops,
      		.ct_attrs       = &cfs_overlay_group_anonymous_item_attrs,
        	.ct_owner       = THIS_MODULE,
};

static struct config_item *cfs_overlay_group_generic_make_item(
    struct config_group *group, const char *name,
	struct config_item_type *item_type);

static void cfs_overlay_group_generic_drop_item(struct config_group *group,
	struct config_item *item);

//specific handling for firmware load/unload
static void cfs_overlay_group_anonymous_drop_item(struct config_group *group, struct config_item *item)
{
	struct cfs_overlay_item *overlay = to_cfs_overlay_item(item);

	if (overlay->ov_id >= 0)
		of_overlay_destroy(overlay->ov_id);
	if (overlay->fw)
		release_firmware(overlay->fw);
	/* kfree with NULL is safe */
	kfree(overlay->dtbo);
	kfree(overlay);
	cfs_overlay_group_generic_drop_item(group, item);
	pr_err("[%s]: name=[%s]\n", __func__, item->ci_name);
}

//no special handling, so directly call to
static struct config_item *cfs_overlay_group_anonymous_make_item(
                 struct config_group *group, const char *name)
{
	 struct config_item_type *item_type = cfs_overlay_group_anonymous_item_type;
		cfs_overlay_group_generic_make_item(group, name, item_type);
}
#endif
#if 0
device-tree/overlays/x/dtbo
|            |       |   |
|            |       |   |=========item (cfs_overlay_group_anoymus_item)
|            |       |=======group_B (cfs_overlay_group_anoymus)
|            | =======group_A (cfs_overlay_group)
|===============system
#endif
#if 0
static struct configfs_group_operations cfs_overlay_group_anonymous_ops = {
        .drop_item        = cfs_overlay_group_anonymous_drop_item,
                //      .make_item = cfs_overlay_group_make_item,
	.make_item = cfs_overlay_group_anonymous_make_item,
}; 

static struct configfs_item_operations cfs_overlay_group_anonymous_ops = {
  //      .release        = cfs_overlay_release,
         //      .make_item = cfs_overlay_group_make_item,
         .store_attribute = cfs_overlay_item_attr_store,
       .show_attributr = cfs_overlay_item_attr_show,
};
#endif

static struct config_item_type cfs_overlay_group_anonymous_type = {
	.ct_item_ops	= &cfs_overlay_group_anonymous_item_ops,
	.ct_attrs	= cfs_overlay_group_anonymous_item_attrs,
//	.ct_bin_attrs	= cfs_overlay_bin_attrs,
//	.ct_group_ops   = &cfs_overlay_group_anonymous_ops,
	.ct_owner	= THIS_MODULE,
};

static struct config_item *cfs_overlay_group_generic_make_item(
		struct config_group *group, const char *name,
		struct config_item_type *item_type)
{
	struct cfs_overlay_item *overlay;

	overlay = kzalloc(sizeof(*overlay), GFP_KERNEL);
	if (!overlay)
		return ERR_PTR(-ENOMEM);
	overlay->ov_id = -1;

        pr_err("[%s]: name=[%s]\n", __func__, name);

	config_item_init_type_name(&overlay->item, name, item_type);
	return &overlay->item;
}

static struct config_item *cfs_overlay_group_make_item(
     	struct config_group *group, const char *name)
{
	return cfs_overlay_group_generic_make_item(group, name, &cfs_overlay_group_anonymous_type);
}

static void cfs_overlay_group_generic_drop_item(struct config_group *group,
		struct config_item *item)
{
	struct cfs_overlay_item *overlay = to_cfs_overlay_item(item);
	pr_err("[%s]: name=[%s]\n", __func__, item->ci_name);
	config_item_put(&overlay->item);
}

static void cfs_overlay_group_drop_item(struct config_group *group,
         struct config_item *item)
{
	cfs_overlay_group_generic_drop_item(group, item);
}

static struct configfs_group_operations overlays_ops = {
	.make_item	= cfs_overlay_group_make_item,
	.drop_item	= cfs_overlay_group_drop_item,
};

static struct config_item_type overlays_type = {
	.ct_group_ops   = &overlays_ops,
	.ct_owner       = THIS_MODULE,
};

static struct configfs_group_operations of_cfs_ops = {
	/* empty - we don't allow anything to be created */
};

static struct config_item_type of_cfs_type = {
	.ct_group_ops   = &of_cfs_ops,
	.ct_owner       = THIS_MODULE,
};

static struct config_group of_cfs_overlay_group;

static struct config_group *of_cfs_def_groups[] = {
	&of_cfs_overlay_group,
	NULL
};

static struct configfs_subsystem of_cfs_subsys = {
	.su_group = {
		.cg_item = {
			.ci_namebuf = "device-tree",
			.ci_type = &of_cfs_type,
		},
		.default_groups = of_cfs_def_groups,
	},
	.su_mutex = __MUTEX_INITIALIZER(of_cfs_subsys.su_mutex),
};

static int __init of_cfs_init(void)
{
	int ret;

	pr_info("%s\n", __func__);

	config_group_init(&of_cfs_subsys.su_group);
	config_group_init_type_name(&of_cfs_overlay_group, "overlays",
			&overlays_type);

	ret = configfs_register_subsystem(&of_cfs_subsys);
	if (ret != 0) {
		pr_err("%s: failed to register subsys\n", __func__);
		goto out;
	}
	pr_info("%s: OK\n", __func__);
out:
	return ret;
}
late_initcall(of_cfs_init);
