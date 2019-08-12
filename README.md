The branch contain changes to make the codebase working twrp 3.3.0
(boot.img from elephone P9000)

changes included:
(1)PMIC sm5414 sm5414.c/sm5414.h/charger_hw_sm5414.c
(2)various Makefile and adding missing include path
(3)DT overlay support ported from newer branch

The idea to add kernel dt-overlay support is due to it's hacky HW's fw nature, 
as the combination of (1)unlocked boot-loader, together with 
(2)Chinese version of lk.

With adb boot feature enabled, there come a very development
unfriendly feature, aka dt overlay, happened before booting into the kernel.
I have to mention that the m681-intl version of update.zip DOES NOT have a 
adb boot enabled lk(.bin) provided.

The base tree is cloned from github.com/mtkkkk/mt6755_wt6755_66_n_kernel
so i dont have any idea what happened in the code. So no garentee the
security/feature completeness.

Finally i would conclude the current kernel tree branch with working feature.
(0)unlock bootloader
(1)LCD/LCM 
(2)PMIC charger partly supported (sm5414, bq24196) so remove usb cable wont reboot
(3)touch (gt9xx, ft5x0x)
(4)RTC/TEMPRETURE/WDT
(5)MMC/SD (internal/external)
(6)Vibrator

The sm5414 code is port from the original m681 tree from meizuosc, thus not working
well with newer mtk battery driver. BQ24196 driver added but not tested. Other 
feature are not tested. it might work, or not at all.

Happy hacking.
Uleak

================

~~  YOUR OWN RISK ~~    ~~  YOUR OWN RISK ~~    ~~  YOUR OWN RISK ~~

Here are detail for developer:
(0) unlock bootloader
generally the lk have a magic frp partition for security. Either for google suite
use or lk unlock bootloader. In short the last dword set to 1 will unlock the
bootloader. So you dont have to had a very unfriendly (and possibly not working)
tool installed like something similar below.
==> https://forum.xda-developers.com/m3-note/how-to/tutorial-unlock-bootloader-meizu-m3-note-txxxxxx

if you wanted to know more about the lk and unlock magic, here are the 
URL, so the myth above can clear.
==> https://github.com/mbskykill/m3note_android_bootable.git

Here are steps to unlock the bootloader. ~~YOUR OWN RISK~~
  (a) make sure lower version (flyme5 ?) installed
  (b) install kingroot(or flyme root) to get root
  (c) install partitions backup (or other tool)
  (d) backup frp partition (need root)
  (e) edit with hex tool (hex editor for e.g.) 
  (f) locate last dword, write 1, save file, write back to frp 
  (g) reboot and install Chinese version of the rom
  (h) optional, step g might fail due to chn/intl (G->A)rom different serial number
  http://forum.flymeos.com/thread-38493-1-1.html
  
then the M3 note is unlocked. please note step h might have draw back such 
as loss of CDMA MEID (Mobile Equipment Identifier) so do as of 
          ~~  YOUR OWN RISK ~~

(1)the system boot CMDLINE specified (by the lk) the boot LCM module name
====> r63350_fhd_dsi_vdo_tcl_sharp_nt50358_drv <=====
thus it might work with other LCM in other M3 Note variant, not sure.
other varient might need to modify accordingly with same name. 
The M3note (A) verision rom have up-side-down LCD with P9000 twrp image,
so the provided kernel source had HW_ROTATE enabled to overcome this issue.

(2)had tried to modify with best affort for multi ext-PMIC (sm5414 / bq24196) 
support driver probe. But due to the odt overlay problem, probing with sm5141 
will use dt node "mediatek,SWITHING_CHARGER" <== note the spelling, its address
reg = <0x6b> thus not suit for sm5414 (0x49) thus driver had a code piece to
override and force probe success if component_detect() found a responding address.

to make the system not reboot on battery not found, battery_common_fg_20.c
is modified too.

(3)kernel dt overlay is for further use. hopefully usb3/usb3_phy might make use
of it. Up till writing, the USB gadget / XHCI is still not working. below are
example for make use of it:
/dts-v1/;

/ {
        fragment@1 {
                target-path = "/soc/i2c@11008000";
                __overlay__ {
                        #address-cells = <0x1>;
                        #size-cells = <0x1>;
                        sm5414@49 {
                                compatible = "sm5414";
                                reg = <0x49>;
                        };
                };
        };
};

(4) the boot cmdline for ref:
			console=tty0 console=ttyMT0,921600n1 root=/dev/ram vmalloc=496M 
slub_max_order=0 slub_debug=O androidboot.hardware=mt6755 bootopt=64S3,32N2,64N2 enforcing=0 androidboot.selinux=permi
ssive  lcm=1-r63350_fhd_dsi_vdo_tcl_sharp_nt50358_drv fps=5419 vram=29229056 printk.disable_uart=1 bootprof.pl_t=4232
bootprof.lk_t=6251 boot_reason=3 androidboot.serialno=91HECNS2RTGZ androidboot.bootreason=wdt androidboot.psn=XXXXXXXX
 psn=XXXXXXXXXX color_type=M1621K hw_version=0x10000000 sw_version=68100000 gpt=1 usb2jtag_mode=0

(5)below might be the reqired driver (yet to port)
isl_sensor_probe
aw8738+aw8738
GF516M.
icm20608
ltr579
pa122

(6) I get it compiled in win 10 with WSL 1.0 and put all of them into D:\
also Make file is modified to include gcc path.

to obtain gcc for linux (that can run inwin10 WSL)
https://releases.linaro.org/components/toolchain/gcc-linaro/

since the DrvGen is 32bit bin and not working in WSL, to skip, remove the file:
/mnt/d/android_kernel_wt6755_66_n/drivers/misc/mediatek/dws/mt6755/wt6755_66_n.dws

-END-
