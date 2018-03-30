/*
 * Core MFD support for Cirrus Logic CS35L40 codec
 *
 * Copyright 2017 Cirrus Logic
 *
 * Author:	David Rhodes	<david.rhodes@cirrus.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/kernel.h>
#include <linux/gpio.h>
#include <linux/mfd/core.h>
#include <linux/module.h>
#include <linux/platform_device.h>

#include <sound/cs35l40.h>
#include <linux/mfd/cs35l40/core.h>

static const struct mfd_cell cs35l40_devs[] = {
	{ .name = "cs35l40-codec", },
	{ .name = "cs35l40-cal", },
};

int cs35l40_dev_init(struct cs35l40_data *cs35l40)
{
	int ret;

	dev_set_drvdata(cs35l40->dev, cs35l40);
	dev_info(cs35l40->dev, "Prince MFD core probe\n");

	if (dev_get_platdata(cs35l40->dev))
		memcpy(&cs35l40->pdata, dev_get_platdata(cs35l40->dev),
		       sizeof(cs35l40->pdata));

	ret = mfd_add_devices(cs35l40->dev, PLATFORM_DEVID_NONE, cs35l40_devs,
				ARRAY_SIZE(cs35l40_devs),
				NULL, 0, NULL);
	if (ret) {
		dev_err(cs35l40->dev, "Failed to add subdevices: %d\n", ret);
		return -EINVAL;
	}

	return 0;
}

int cs35l40_dev_exit(struct cs35l40_data *cs35l40)
{
	mfd_remove_devices(cs35l40->dev);
	return 0;
}
