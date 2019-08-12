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

#
# Makefile for misc devices that really don't fit anywhere else.
#

obj-$(CONFIG_MTK_LCM) += mt65xx_lcm_list.o        \
                         lcm_common.o             \
                         lcm_gpio.o               \
                         lcm_i2c.o                \
                         lcm_pmic.o               \
                         lcm_util.o

obj-$(CONFIG_MTK_LCM) += tps65132.o

LCM_LISTS := $(subst ",,$(CONFIG_CUSTOM_KERNEL_LCM))
#obj-$(CONFIG_MTK_LCM) += $(foreach LCM,$(LCM_LISTS),$(LCM)/)
obj-y += r63350_fhd_dsi_vdo_tcl_sharp_nt50358/
obj-y += r63350_fhd_dsi_vdo_tcl_nt50358/
obj-y += nt35695_fhd_dsi_cmd_truly_nt50358/

subdir-ccflags-$(CONFIG_MTK_LCM) += -I$(srctree)/drivers/misc/mediatek/lcm/inc
subdir-ccflags-$(CONFIG_MTK_FB) += -I$(srctree)/drivers/misc/mediatek/video/$(MTK_PLATFORM)/dispsys \
                                       -I$(srctree)/drivers/misc/mediatek/video/$(MTK_PLATFORM)/dispsys/mt6735 \
                                       -I$(srctree)/drivers/misc/mediatek/video/$(MTK_PLATFORM)/videox \
                                       -I$(srctree)/drivers/misc/mediatek/video/$(MTK_PLATFORM)/ \
                                       -I$(srctree)/drivers/misc/mediatek/video/$(MTK_PLATFORM)/mt6735/ \
                                       -I$(srctree)/drivers/misc/mediatek/video/include

LCM_DEFINES := $(shell echo $(CONFIG_CUSTOM_KERNEL_LCM) | tr a-z A-Z)
DEFINES += $(foreach LCM,$(LCM_DEFINES),$(LCM))
DEFINES += R63350_FHD_DSI_VDO_TCL_SHARP_NT50358
DEFINES += R63350_FHD_DSI_VDO_TCL_NT50358
DEFINES += NT35695_FHD_DSI_CMD_TRULY_NT50358
#DEFINES += MTK_LCM_PHYSICAL_ROTATION=\"180\"
DEFINES += MTK_LCM_PHYSICAL_ROTATION=\"$(MTK_LCM_PHYSICAL_ROTATION)\"
ccflags-$(CONFIG_MTK_LCM) += $(addprefix -D, $(DEFINES))
ccflags-$(CONFIG_MTK_LCM_DEVICE_TREE_SUPPORT) += -DMTK_LCM_DEVICE_TREE_SUPPORT

$(info $$DEFINES is [${DEFINES}])
