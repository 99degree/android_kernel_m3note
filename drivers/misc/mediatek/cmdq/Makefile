#
# Copyright (C) 2015 MediaTek Inc.
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License version 2 as
# published by the Free Software Foundation.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
# GNU General Public License for more details.
#

# common driver code for CMDQ
# 72/82/92 does not use this common code

ifeq ($(CONFIG_MTK_CMDQ_TAB), y)
ifneq (,$(filter $(CONFIG_MTK_PLATFORM), "mt8163"))
  # use common driver
  obj-y += v2/
else
  # hv1
  # obj-y += $(subst ",,$(CONFIG_MTK_PLATFORM))/
  obj-y += hv1/
endif
else ifeq ($(CONFIG_MTK_CMDQ_MT2701), y)
  obj-$(CONFIG_MTK_CMDQ_MT2701) += mt2701/
else
  # use common driver
  obj-y += v2/
endif
# EOF
