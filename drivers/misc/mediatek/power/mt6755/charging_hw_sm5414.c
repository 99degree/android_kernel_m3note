/*****************************************************************************
 *
 * Filename:
 * ---------
 *    charging_pmic.c
 *
 * Project:
 * --------
 *   ALPS_Software
 *
 * Description:
 * ------------
 *   This file implements the interface between BMT and ADC scheduler.
 *
 * Author:
 * -------
 *  Oscar Liu
 *
 *============================================================================
  * $Revision:   1.0  $
 * $Modtime:   11 Aug 2005 10:28:16  $
 * $Log:   //mtkvs01/vmdata/Maui_sw/archives/mcu/hal/peripheral/inc/bmt_chr_setting.h-arc  $
 *             HISTORY
 * Below this line, this part is controlled by PVCS VM. DO NOT MODIFY!!
 *------------------------------------------------------------------------------
 *------------------------------------------------------------------------------
 * Upper this line, this part is controlled by PVCS VM. DO NOT MODIFY!!
 *============================================================================
 ****************************************************************************/
#include <linux/types.h>
#include <mt-plat/charging.h>
#include <mt-plat/upmu_common.h>
#include <linux/delay.h>
#include <linux/reboot.h>
#include <mt-plat/mt_boot.h>
#include <mt-plat/battery_common.h>
#include <mach/mt_charging.h>
#include <mach/mt_pmic.h>
#include <mach/mt_sleep.h>
#include "mtk_bif_intf.h"
#include "sm5414.h"


// ============================================================ //
// Define
// ============================================================ //
#define STATUS_OK	0
#define STATUS_UNSUPPORTED	-1
#define GETARRAYNUM(array) (sizeof(array)/sizeof(array[0]))


// ============================================================ //
// Global variable
// ============================================================ //
static CHARGER_TYPE __maybe_unused g_charger_type = CHARGER_UNKNOWN;

bool charging_type_det_done = KAL_TRUE;

static const unsigned int VBAT_CV_VTH[]=
{
    4100000,4125000,4150000,4175000,
    4200000,4225000,4250000,4275000,
    4300000,4325000,4350000,4375000,
    4400000,4425000,4450000,4475000 
};

static const unsigned int CS_VTH[]=
{
	10000,15000,20000,25000,
	30000,35000,40000,45000,
	50000,55000,60000,65000,
	70000,75000,80000,85000,
	90000,95000,100000,105000,
	110000,115000,120000,125000,
	130000,135000,140000,145000,
	150000,155000,160000,165000,
	170000,175000,180000,185000,
	190000,195000,200000,205000,
	210000,215000,220000,225000,
	230000,235000,240000,245000,
	250000
}; 

static const unsigned int INPUT_CS_VTH[]=
{
	10000,15000,20000,25000,
	30000,35000,40000,45000,
	50000,55000,60000,65000,
	70000,75000,80000,85000,
	90000,95000,100000,105000,
	110000,115000,120000,125000,
	130000,135000,140000,145000,
	150000,155000,160000,165000,
	170000,175000,180000,185000,
	190000,195000,200000,205000
}; 

static const unsigned int VCDT_HV_VTH[]=
{
	BATTERY_VOLT_04_200000_V, BATTERY_VOLT_04_250000_V,	  BATTERY_VOLT_04_300000_V,   BATTERY_VOLT_04_350000_V,
	BATTERY_VOLT_04_400000_V, BATTERY_VOLT_04_450000_V,	  BATTERY_VOLT_04_500000_V,   BATTERY_VOLT_04_550000_V,
	BATTERY_VOLT_04_600000_V, BATTERY_VOLT_06_000000_V,	  BATTERY_VOLT_06_500000_V,   BATTERY_VOLT_07_000000_V,
	BATTERY_VOLT_07_500000_V, BATTERY_VOLT_08_500000_V,	  BATTERY_VOLT_09_500000_V,   BATTERY_VOLT_10_500000_V		  
};

/* BQ25896 REG0A BOOST_LIM[2:0], mA */
const unsigned int BOOST_CURRENT_LIMIT[] = {
        500, 1300,
};

#ifdef CONFIG_MTK_DUAL_INPUT_CHARGER_SUPPORT
#ifndef CUST_GPIO_VIN_SEL
#define CUST_GPIO_VIN_SEL 18
#endif
#if !defined(MTK_AUXADC_IRQ_SUPPORT)
#define SW_POLLING_PERIOD 100 //100 ms
#define MSEC_TO_NSEC(x)		(x * 1000000UL)

static DEFINE_MUTEX(diso_polling_mutex);
static DECLARE_WAIT_QUEUE_HEAD(diso_polling_thread_wq);
static struct hrtimer diso_kthread_timer;
static bool diso_thread_timeout = KAL_FALSE;
static struct delayed_work diso_polling_work;
static void diso_polling_handler(struct work_struct *work);
static DISO_Polling_Data DISO_Polling;
#else
DISO_IRQ_Data DISO_IRQ;
#endif
int g_diso_state  = 0;
int vin_sel_gpio_number   = (CUST_GPIO_VIN_SEL | 0x80000000); 

static char *DISO_state_s[8] = {
  "IDLE",
  "OTG_ONLY",
  "USB_ONLY",
  "USB_WITH_OTG",
  "DC_ONLY",
  "DC_WITH_OTG",
  "DC_WITH_USB",
  "DC_USB_OTG",
};
#endif

// ============================================================ //
// function prototype
// ============================================================ //
 
// ============================================================ //
//extern variable
// ============================================================ //

// ============================================================ //
//extern function
// ============================================================ //
extern unsigned int upmu_get_reg_value(unsigned int reg);
extern bool mt_usb_is_device(void);
extern void Charger_Detect_Init(void);
extern void Charger_Detect_Release(void);
extern int hw_charging_get_charger_type(void);
extern void mt_power_off(void);
extern unsigned int mt6311_get_chip_id(void);
extern int is_mt6311_exist(void);
extern int is_mt6311_sw_ready(void);

static unsigned int charging_error = false;
static unsigned int __maybe_unused charging_get_error_state(void);
static unsigned int charging_set_error_state(void *data);
// ============================================================ //
static unsigned int charging_value_to_parameter(const unsigned int *parameter, const unsigned int array_size, const unsigned int val)
{
	if (val < array_size)
	{
		return parameter[val];
	}
	else
	{
		pr_notice("Can't find the parameter \r\n");	
		return parameter[0];
	}
}

static unsigned int charging_parameter_to_value(const unsigned int *parameter, const unsigned int array_size, const unsigned int val)
{
	unsigned int i;

	pr_info("array_size = %d \r\n", array_size);

	for(i=0;i<array_size;i++)
	{
		if (val == *(parameter + i))
			return i;
	}

	pr_notice("NO register value match. val=%d\r\n", val);

	return 0;
}

static unsigned int bmt_find_closest_level(const unsigned int *pList,unsigned int number,unsigned int level)
{
	unsigned int i;
	unsigned int max_value_in_last_element;

	if(pList[0] < pList[1])
		max_value_in_last_element = KAL_TRUE;
	else
		max_value_in_last_element = KAL_FALSE;

	if(max_value_in_last_element == KAL_TRUE)
	{
		for(i = (number-1); i != 0; i--)	 //max value in the last element
		{
			if(pList[i] <= level)
				return pList[i];
		}

		pr_notice("Can't find closest level, small value first \r\n");
		return pList[0];
	}
	else
	{
		for(i = 0; i< number; i++)  // max value in the first element
		{
			if(pList[i] <= level)
				return pList[i];
		}

		pr_notice("Can't find closest level, large value first \r\n"); 	 
		return pList[number -1];
	}
}

static unsigned int charging_hw_init(void *data)
{
	unsigned int status = STATUS_OK;

	//#if defined(GPIO_SM5414_CHGEN_PIN)
	//mt_set_gpio_mode(GPIO_SM5414_CHGEN_PIN,GPIO_MODE_GPIO);  
	//mt_set_gpio_dir(GPIO_SM5414_CHGEN_PIN,GPIO_DIR_OUT);
	//mt_set_gpio_out(GPIO_SM5414_CHGEN_PIN,GPIO_OUT_ZERO);    
	//#endif
    #if defined(GPIO_SM5414_SHDN_PIN)
        mt_set_gpio_mode(GPIO_SM5414_SHDN_PIN,GPIO_MODE_GPIO);  
       mt_set_gpio_dir(GPIO_SM5414_SHDN_PIN,GPIO_DIR_OUT);
        mt_set_gpio_out(GPIO_SM5414_SHDN_PIN,GPIO_OUT_ONE);
	#endif
     //sm5414_set_chgen(CHARGE_EN);
    sm5414_set_topoff(TOPOFF_150mA);
#if defined(HIGH_BATTERY_VOLTAGE_SUPPORT)
	sm5414_set_batreg(BATREG_4_4_0_0_V); //VREG 4.352V
#else
	sm5414_set_batreg(BATREG_4_2_0_0_V); //VREG 4.208V
#endif 

	#ifdef CONFIG_MTK_DUAL_INPUT_CHARGER_SUPPORT
	mt_set_gpio_mode(vin_sel_gpio_number,0); // 0:GPIO mode
	mt_set_gpio_dir(vin_sel_gpio_number,0); // 0: input, 1: output
	#endif

#if defined(SM5414_TOPOFF_TIMER_SUPPORT)
    sm5414_set_autostop(AUTOSTOP_EN);
    sm5414_set_topofftimer(TOPOFFTIMER_10MIN);
#else
    sm5414_set_autostop(AUTOSTOP_DIS);
#endif
    sm5414_set_aiclth(AICL_THRESHOLD_4_4_V);
	return status;
}

static unsigned int charging_dump_register(void *data)
{
	pr_notice("charging_dump_register\r\n");

	sm5414_dump_register();

	return STATUS_OK;
}	

static unsigned int charging_enable(void *data)
{
	unsigned int status = STATUS_OK;
	unsigned int enable = *(unsigned int*)(data);
	unsigned int __maybe_unused bootmode = 0;
	printk("zmlin charging_enable:%d\n",enable);
	//sm5414_set_chgen(CHARGE_EN);
	if(KAL_TRUE == enable)
	{
#if defined(GPIO_SM5414_CHGEN_PIN)
        mt_set_gpio_mode(GPIO_SM5414_CHGEN_PIN,GPIO_MODE_GPIO);  
        mt_set_gpio_dir(GPIO_SM5414_CHGEN_PIN,GPIO_DIR_OUT);
        mt_set_gpio_out(GPIO_SM5414_CHGEN_PIN,GPIO_OUT_ZERO);    
#else
        sm5414_set_chgen(CHARGE_EN);
#endif
	}
	else
	{
		#if defined(CONFIG_USB_MTK_HDRC_HCD)
		if(mt_usb_is_device())
		#endif
		{
#if defined(GPIO_SM5414_CHGEN_PIN)

        mt_set_gpio_mode(GPIO_SM5414_CHGEN_PIN,GPIO_MODE_GPIO);  
        mt_set_gpio_dir(GPIO_SM5414_CHGEN_PIN,GPIO_DIR_OUT);
        mt_set_gpio_out(GPIO_SM5414_CHGEN_PIN,GPIO_OUT_ONE);    
#else
        //sm5414_set_chgen(CHARGE_DIS);
#endif
		}
#if 0
		bootmode = get_boot_mode();
		if ((bootmode == META_BOOT) || (bootmode == ADVMETA_BOOT))
			bq24296_set_en_hiz(0x1);

		#if defined(CONFIG_MTK_DUAL_INPUT_CHARGER_SUPPORT)
		bq24296_set_chg_config(0x0);
		bq24296_set_en_hiz(0x1);	// disable power path
		#endif
#endif
	}

	return status;
}

static unsigned int charging_set_cv_voltage(void *data)
{
	unsigned int status = STATUS_OK;
	unsigned int array_size;
	unsigned int set_cv_voltage;
	unsigned short register_value;
	unsigned int cv_value = *(unsigned int *)(data);	
	static short __maybe_unused pre_register_value = -1;

	#if defined(HIGH_BATTERY_VOLTAGE_SUPPORT)
	if(cv_value >= BATTERY_VOLT_04_400000_V)
		cv_value = BATTERY_VOLT_04_400000_V;//4350000;
	#endif

	//use nearest value
	if(BATTERY_VOLT_04_200000_V == cv_value)
		cv_value = 4208000;

	array_size = GETARRAYNUM(VBAT_CV_VTH);
	set_cv_voltage = bmt_find_closest_level(VBAT_CV_VTH, array_size, cv_value);
	register_value = charging_parameter_to_value(VBAT_CV_VTH, array_size, set_cv_voltage);
	sm5414_set_batreg(register_value); 

#if 0
	//for jeita recharging issue
	if (pre_register_value != register_value)
		bq24296_set_chg_config(1);

	pre_register_value = register_value;
#endif
	return status;
}

static unsigned int charging_get_current(void *data)
{
	unsigned int status = STATUS_OK;
	unsigned int array_size;
	unsigned char reg_value;
#if 0
	unsigned char ret_val=0;
	unsigned char ret_force_20pct=0;

	//Get current level
	bq24296_read_interface(bq24296_CON2, &ret_val, CON2_ICHG_MASK, CON2_ICHG_SHIFT);

	//Get Force 20% option
	bq24296_read_interface(bq24296_CON2, &ret_force_20pct, CON2_FORCE_20PCT_MASK, CON2_FORCE_20PCT_SHIFT);

	//Parsing
	ret_val = (ret_val*64) + 512;

	if (ret_force_20pct == 0)
	{
		//Get current level
		//array_size = GETARRAYNUM(CS_VTH);
		// *(unsigned int *)data = charging_value_to_parameter(CS_VTH,array_size,reg_value);
		*(unsigned int *)data = ret_val;
	}   
	else
	{
		//Get current level
		//array_size = GETARRAYNUM(CS_VTH_20PCT);
		// *(unsigned int *)data = charging_value_to_parameter(CS_VTH,array_size,reg_value);
		//return (int)(ret_val<<1)/10;
		*(unsigned int *)data = (int)(ret_val<<1)/10;
	}   
#endif

    //Get current level
    array_size = GETARRAYNUM(CS_VTH);
    sm5414_read_interface(SM5414_CHGCTRL2, &reg_value, SM5414_CHGCTRL2_FASTCHG_MASK, SM5414_CHGCTRL2_FASTCHG_SHIFT);//FASTCHG
    *(unsigned int *)data = charging_value_to_parameter(CS_VTH,array_size,reg_value);

	return status;
}  

static unsigned int charging_set_current(void *data)
{
	unsigned int status = STATUS_OK;
	unsigned int set_chr_current;
	unsigned int array_size;
	unsigned int register_value;
	unsigned int current_value = *(unsigned int *)data;

	array_size = GETARRAYNUM(CS_VTH);
	set_chr_current = bmt_find_closest_level(CS_VTH, array_size, current_value);
	register_value = charging_parameter_to_value(CS_VTH, array_size ,set_chr_current);
    sm5414_set_fastchg(register_value);//FASTCHG

	return status;
} 	

static unsigned int charging_set_input_current(void *data)
{
	unsigned int status = STATUS_OK;
	unsigned int current_value = *(unsigned int *)data;
	unsigned int set_chr_current;
	unsigned int array_size;
	unsigned int register_value;

	array_size = GETARRAYNUM(INPUT_CS_VTH);
	set_chr_current = bmt_find_closest_level(INPUT_CS_VTH, array_size, current_value);
	register_value = charging_parameter_to_value(INPUT_CS_VTH, array_size ,set_chr_current);	
    sm5414_set_vbuslimit(register_value);//VBUSLIMIT

	return status;
} 	

static unsigned int charging_get_charging_status(void *data)
{
	unsigned int status = STATUS_OK;
	unsigned int ret_val;

//	ret_val = sm5414_get_chrg_stat();
	ret_val = sm5414_get_topoff_status();//Topoff : Fullcharging

	if(ret_val == 0x1)//Fullcharged status
		*(unsigned int *)data = KAL_TRUE;
	else
		*(unsigned int *)data = KAL_FALSE;//v<4.1 ok

	return status;
} 	

static unsigned int charging_reset_watch_dog_timer(void *data)
{
	unsigned int status = STATUS_OK;

	return status;
}

static unsigned int charging_set_hv_threshold(void *data)
{
	unsigned int status = STATUS_OK;
	unsigned int set_hv_voltage;
	unsigned int array_size;
	unsigned short register_value;
	unsigned int voltage = *(unsigned int*)(data);

	array_size = GETARRAYNUM(VCDT_HV_VTH);
	set_hv_voltage = bmt_find_closest_level(VCDT_HV_VTH, array_size, voltage);
	register_value = charging_parameter_to_value(VCDT_HV_VTH, array_size ,set_hv_voltage);
	pmic_set_register_value(PMIC_RG_VCDT_HV_VTH,register_value);

	return status;
}

static unsigned int charging_get_hv_status(void *data)
{
	unsigned int status = STATUS_OK;

	*(bool*)(data) = pmic_get_register_value(PMIC_RGS_VCDT_HV_DET);

	return status;
}

static unsigned int charging_get_battery_status(void *data)
{
	unsigned int status = STATUS_OK;
	unsigned int val = 0;

	#if defined(CONFIG_POWER_EXT) || defined(CONFIG_MTK_FPGA)
	*(bool*)(data) = 0; // battery exist
	pr_notice("bat exist for evb\n");
	#else
	val=pmic_get_register_value(PMIC_BATON_TDET_EN);
	pr_info("[charging_get_battery_status] BATON_TDET_EN = %d\n", val);
	if (val) {
	pmic_set_register_value(PMIC_BATON_TDET_EN,1);
	pmic_set_register_value(PMIC_RG_BATON_EN,1);
	*(bool*)(data) = pmic_get_register_value(PMIC_RGS_BATON_UNDET);
	} else {
		*(bool*)(data) =  KAL_FALSE;
	}
	#endif  

	return status;
}

static unsigned int charging_get_charger_det_status(void *data)
{
	unsigned int status = STATUS_OK;
	unsigned int val = 0;

	#if defined(CONFIG_MTK_FPGA)
		val = 1;
		pr_notice("[charging_get_charger_det_status] chr exist for fpga.\n"); 
	#else    
		#if !defined(CONFIG_MTK_DUAL_INPUT_CHARGER_SUPPORT)
		val = pmic_get_register_value(PMIC_RGS_CHRDET);
		#else
		if(((g_diso_state >> 1) & 0x3) != 0x0 || pmic_get_register_value(PMIC_RGS_CHRDET))
			val = KAL_TRUE;
		else
			val = KAL_FALSE;
		#endif
	#endif

	*(bool*)(data) = val;

	return status;
}

static bool __maybe_unused charging_type_detection_done(void)
{
	return charging_type_det_done;
}

static unsigned int charging_get_charger_type(void *data)
{
	unsigned int status = STATUS_OK;

	#if defined(CONFIG_POWER_EXT) || defined(CONFIG_MTK_FPGA)
	*(CHARGER_TYPE*)(data) = STANDARD_HOST;
	#else
	*(CHARGER_TYPE*)(data) = hw_charging_get_charger_type();
	#endif

    return status;
}

static unsigned int charging_get_is_pcm_timer_trigger(void *data)
{
	unsigned int status = STATUS_OK;
	#if defined(CONFIG_POWER_EXT) || defined(CONFIG_MTK_FPGA)
	*(bool*)(data) = KAL_FALSE;
	#else 
//#pragma message "comment out due to compilation error"
	if(slp_get_wake_reason() == WR_PCM_TIMER)
		*(bool*)(data) = KAL_TRUE;
	else
		*(bool*)(data) = KAL_FALSE;
	pr_notice("slp_get_wake_reason=%d\n", slp_get_wake_reason());
	#endif
	return status;
}

static unsigned int charging_set_platform_reset(void *data)
{
	unsigned int status = STATUS_OK;
	#if defined(CONFIG_POWER_EXT) || defined(CONFIG_MTK_FPGA)	 
	#else 
	pr_notice("charging_set_platform_reset\n");
//	arch_reset(0,NULL);
	kernel_restart("battery service reboot system");
	#endif
	return status;
}

static unsigned int charging_get_platfrom_boot_mode(void *data)
{
	unsigned int status = STATUS_OK;
	#if defined(CONFIG_POWER_EXT) || defined(CONFIG_MTK_FPGA)   
	#else  
	*(unsigned int*)(data) = get_boot_mode();

	pr_notice("get_boot_mode=%d\n", get_boot_mode());
	#endif
	return status;
}

static unsigned int charging_set_power_off(void *data)
{
	unsigned int status = STATUS_OK;
	#if defined(CONFIG_POWER_EXT) || defined(CONFIG_MTK_FPGA)
	#else
	pr_notice("charging_set_power_off\n");
	mt_power_off();
	#endif

	return status;
}

static unsigned int charging_get_power_source(void *data)
{
	unsigned int status = STATUS_OK;

	#if 0	//#if defined(MTK_POWER_EXT_DETECT)
	if (MT_BOARD_PHONE == mt_get_board_type())
		*(bool *)data = KAL_FALSE;
	else
		*(bool *)data = KAL_TRUE;
	#else
	*(bool *)data = KAL_FALSE;
	#endif

	return status;
}

static unsigned int charging_get_csdac_full_flag(void *data)
{
	return STATUS_UNSUPPORTED;	
}

static unsigned int charging_set_ta_current_pattern(void *data)
{
	unsigned int increase = *(unsigned int*)(data);
	unsigned int charging_status = KAL_FALSE;

	#if defined(HIGH_BATTERY_VOLTAGE_SUPPORT)
	//BATTERY_VOLTAGE_ENUM cv_voltage = BATTERY_VOLT_04_340000_V;
	BATTERY_VOLTAGE_ENUM cv_voltage = BATTERY_VOLT_04_400000_V;
	#else
	BATTERY_VOLTAGE_ENUM cv_voltage = BATTERY_VOLT_04_200000_V;
	#endif

	charging_get_charging_status(&charging_status);
	if(KAL_FALSE == charging_status)
	{
		charging_set_cv_voltage(&cv_voltage);  //Set CV 
        sm5414_set_vbuslimit(VBUSLIMIT_500mA);//VBUSLIMIT
#if defined(GPIO_SM5414_CHGEN_PIN)
        mt_set_gpio_mode(GPIO_SM5414_CHGEN_PIN,GPIO_MODE_GPIO);  
        mt_set_gpio_dir(GPIO_SM5414_CHGEN_PIN,GPIO_DIR_OUT);
        mt_set_gpio_out(GPIO_SM5414_CHGEN_PIN,GPIO_OUT_ZERO);    
#else
        sm5414_set_chgen(CHARGE_EN);
#endif
	}

	if(increase == KAL_TRUE)
    {
        sm5414_set_vbuslimit(VBUSLIMIT_100mA); /* 100mA */
        msleep(85);
        
        sm5414_set_vbuslimit(VBUSLIMIT_500mA); /* 500mA */
        battery_log(BAT_LOG_FULL, "mtk_ta_increase() on 1");
        msleep(85);
        
        sm5414_set_vbuslimit(VBUSLIMIT_100mA); /* 100mA */
        battery_log(BAT_LOG_FULL, "mtk_ta_increase() off 1");
        msleep(85);
        
        sm5414_set_vbuslimit(VBUSLIMIT_500mA); /* 500mA */
        battery_log(BAT_LOG_FULL, "mtk_ta_increase() on 2");
        msleep(85);
        
        sm5414_set_vbuslimit(VBUSLIMIT_100mA); /* 100mA */
        battery_log(BAT_LOG_FULL, "mtk_ta_increase() off 2");
        msleep(85);
        
        sm5414_set_vbuslimit(VBUSLIMIT_500mA); /* 500mA */
        battery_log(BAT_LOG_FULL, "mtk_ta_increase() on 3");
        msleep(281);
        
        sm5414_set_vbuslimit(VBUSLIMIT_100mA); /* 100mA */
        battery_log(BAT_LOG_FULL, "mtk_ta_increase() off 3");
        msleep(85);
        
        sm5414_set_vbuslimit(VBUSLIMIT_500mA); /* 500mA */
        battery_log(BAT_LOG_FULL, "mtk_ta_increase() on 4");
        msleep(281);
        
        sm5414_set_vbuslimit(VBUSLIMIT_100mA); /* 100mA */
        battery_log(BAT_LOG_FULL, "mtk_ta_increase() off 4");
        msleep(85);
        
        sm5414_set_vbuslimit(VBUSLIMIT_500mA); /* 500mA */
        battery_log(BAT_LOG_FULL, "mtk_ta_increase() on 5");
        msleep(281);
        
        sm5414_set_vbuslimit(VBUSLIMIT_100mA); /* 100mA */
        battery_log(BAT_LOG_FULL, "mtk_ta_increase() off 5");
        msleep(85);
        
        sm5414_set_vbuslimit(VBUSLIMIT_500mA); /* 500mA */
        battery_log(BAT_LOG_FULL, "mtk_ta_increase() on 6");
        msleep(485);
        
        sm5414_set_vbuslimit(VBUSLIMIT_100mA); /* 100mA */
        battery_log(BAT_LOG_FULL, "mtk_ta_increase() off 6");
        msleep(50);
        
        battery_log(BAT_LOG_CRTI, "mtk_ta_increase() end \n");
        
        sm5414_set_vbuslimit(VBUSLIMIT_500mA); /* 500mA */
        msleep(200);
    }
    else
    {
        sm5414_set_vbuslimit(VBUSLIMIT_100mA); /* 100mA */
        msleep(85);
        
        sm5414_set_vbuslimit(VBUSLIMIT_500mA); /* 500mA */
        battery_log(BAT_LOG_FULL, "mtk_ta_decrease() on 1");
        msleep(281);
        
        sm5414_set_vbuslimit(VBUSLIMIT_100mA); /* 100mA */
        battery_log(BAT_LOG_FULL, "mtk_ta_decrease() off 1");
        msleep(85);
        
        sm5414_set_vbuslimit(VBUSLIMIT_500mA); /* 500mA */
        battery_log(BAT_LOG_FULL, "mtk_ta_decrease() on 2");
        msleep(281);
        
        sm5414_set_vbuslimit(VBUSLIMIT_100mA); /* 100mA */
        battery_log(BAT_LOG_FULL, "mtk_ta_decrease() off 2");
        msleep(85);
        
        sm5414_set_vbuslimit(VBUSLIMIT_500mA); /* 500mA */
        battery_log(BAT_LOG_FULL, "mtk_ta_decrease() on 3");
        msleep(281);
        
        sm5414_set_vbuslimit(VBUSLIMIT_100mA); /* 100mA */
        battery_log(BAT_LOG_FULL, "mtk_ta_decrease() off 3");
        msleep(85);
        
        sm5414_set_vbuslimit(VBUSLIMIT_500mA); /* 500mA */
        battery_log(BAT_LOG_FULL, "mtk_ta_decrease() on 4");
        msleep(85);
        
        sm5414_set_vbuslimit(VBUSLIMIT_100mA); /* 100mA */
        battery_log(BAT_LOG_FULL, "mtk_ta_decrease() off 4");
        msleep(85);
        
        sm5414_set_vbuslimit(VBUSLIMIT_500mA); /* 500mA */
        battery_log(BAT_LOG_FULL, "mtk_ta_decrease() on 5");
        msleep(85);
        
        sm5414_set_vbuslimit(VBUSLIMIT_100mA); /* 100mA */
        battery_log(BAT_LOG_FULL, "mtk_ta_decrease() off 5");
        msleep(85);
        
        sm5414_set_vbuslimit(VBUSLIMIT_500mA); /* 500mA */
        battery_log(BAT_LOG_FULL, "mtk_ta_decrease() on 6");
        msleep(485);
        
        sm5414_set_vbuslimit(VBUSLIMIT_100mA); /* 100mA */
        battery_log(BAT_LOG_FULL, "mtk_ta_decrease() off 6");
        msleep(50);
        
        battery_log(BAT_LOG_CRTI, "mtk_ta_decrease() end \n");
        
        sm5414_set_vbuslimit(VBUSLIMIT_500mA); /* 500mA */
    }

	return STATUS_OK;
}

#if defined(CONFIG_MTK_DUAL_INPUT_CHARGER_SUPPORT)
void set_vusb_auxadc_irq(bool enable, bool flag)
{
	hrtimer_cancel(&diso_kthread_timer);

	DISO_Polling.reset_polling = KAL_TRUE;
	DISO_Polling.vusb_polling_measure.notify_irq_en = enable;
	DISO_Polling.vusb_polling_measure.notify_irq = flag;

	hrtimer_start(&diso_kthread_timer, ktime_set(0, MSEC_TO_NSEC(SW_POLLING_PERIOD)), HRTIMER_MODE_REL);

	pr_info(" [%s] enable: %d, flag: %d!\n", __func__, enable, flag);
}

static void set_vdc_auxadc_irq(bool enable, bool flag)
{
	hrtimer_cancel(&diso_kthread_timer);

	DISO_Polling.reset_polling = KAL_TRUE;
	DISO_Polling.vdc_polling_measure.notify_irq_en = enable;
	DISO_Polling.vdc_polling_measure.notify_irq = flag;

	hrtimer_start(&diso_kthread_timer, ktime_set(0, MSEC_TO_NSEC(SW_POLLING_PERIOD)), HRTIMER_MODE_REL);
	pr_info(" [%s] enable: %d, flag: %d!\n", __func__, enable, flag);
}

static void diso_polling_handler(struct work_struct *work)
{
	int trigger_channel = -1;
	int trigger_flag = -1;

	if(DISO_Polling.vdc_polling_measure.notify_irq_en)
		trigger_channel = AP_AUXADC_DISO_VDC_CHANNEL;
	else if(DISO_Polling.vusb_polling_measure.notify_irq_en)
		trigger_channel = AP_AUXADC_DISO_VUSB_CHANNEL;

	pr_notice("[DISO]auxadc handler triggered\n" );
	switch(trigger_channel)
	{
		case AP_AUXADC_DISO_VDC_CHANNEL:
			trigger_flag = DISO_Polling.vdc_polling_measure.notify_irq;
			pr_notice("[DISO]VDC IRQ triggered, channel ==%d, flag ==%d\n", trigger_channel, trigger_flag );
			#ifdef MTK_DISCRETE_SWITCH /*for DSC DC plugin handle */
			set_vdc_auxadc_irq(DISO_IRQ_DISABLE, 0);
			set_vusb_auxadc_irq(DISO_IRQ_DISABLE, 0);
			set_vusb_auxadc_irq(DISO_IRQ_ENABLE, DISO_IRQ_FALLING);
			if (trigger_flag == DISO_IRQ_RISING) {
				DISO_data.diso_state.pre_vusb_state  = DISO_ONLINE;
				DISO_data.diso_state.pre_vdc_state  = DISO_OFFLINE;
				DISO_data.diso_state.pre_otg_state  = DISO_OFFLINE;
				DISO_data.diso_state.cur_vusb_state  = DISO_ONLINE;
				DISO_data.diso_state.cur_vdc_state  = DISO_ONLINE;
				DISO_data.diso_state.cur_otg_state  = DISO_OFFLINE;
				pr_notice(" cur diso_state is %s!\n",DISO_state_s[2]);
			}
			#else //for load switch OTG leakage handle
			set_vdc_auxadc_irq(DISO_IRQ_ENABLE, (~trigger_flag) & 0x1);
			if (trigger_flag == DISO_IRQ_RISING) {
				DISO_data.diso_state.pre_vusb_state  = DISO_OFFLINE;
				DISO_data.diso_state.pre_vdc_state  = DISO_OFFLINE;
				DISO_data.diso_state.pre_otg_state  = DISO_ONLINE;
				DISO_data.diso_state.cur_vusb_state  = DISO_OFFLINE;
				DISO_data.diso_state.cur_vdc_state  = DISO_ONLINE;
				DISO_data.diso_state.cur_otg_state  = DISO_ONLINE;
				pr_notice(" cur diso_state is %s!\n",DISO_state_s[5]);
			} else if (trigger_flag == DISO_IRQ_FALLING) {
				DISO_data.diso_state.pre_vusb_state  = DISO_OFFLINE;
				DISO_data.diso_state.pre_vdc_state  = DISO_ONLINE;
				DISO_data.diso_state.pre_otg_state  = DISO_ONLINE;
				DISO_data.diso_state.cur_vusb_state  = DISO_OFFLINE;
				DISO_data.diso_state.cur_vdc_state  = DISO_OFFLINE;
				DISO_data.diso_state.cur_otg_state  = DISO_ONLINE;
				pr_notice(" cur diso_state is %s!\n",DISO_state_s[1]);
			}
			else
				pr_notice("[%s] wrong trigger flag!\n",__func__);
			#endif
			break;
		case AP_AUXADC_DISO_VUSB_CHANNEL:
			trigger_flag = DISO_Polling.vusb_polling_measure.notify_irq;
			pr_notice("[DISO]VUSB IRQ triggered, channel ==%d, flag ==%d\n", trigger_channel, trigger_flag);
			set_vdc_auxadc_irq(DISO_IRQ_DISABLE, 0);
			set_vusb_auxadc_irq(DISO_IRQ_DISABLE, 0);
			if(trigger_flag == DISO_IRQ_FALLING) {
				DISO_data.diso_state.pre_vusb_state  = DISO_ONLINE;
				DISO_data.diso_state.pre_vdc_state  = DISO_ONLINE;
				DISO_data.diso_state.pre_otg_state  = DISO_OFFLINE;
				DISO_data.diso_state.cur_vusb_state  = DISO_OFFLINE;
				DISO_data.diso_state.cur_vdc_state  = DISO_ONLINE;
				DISO_data.diso_state.cur_otg_state  = DISO_OFFLINE;
				pr_notice(" cur diso_state is %s!\n",DISO_state_s[4]);
			} else if (trigger_flag == DISO_IRQ_RISING) {
				DISO_data.diso_state.pre_vusb_state  = DISO_OFFLINE;
				DISO_data.diso_state.pre_vdc_state  = DISO_ONLINE;
				DISO_data.diso_state.pre_otg_state  = DISO_OFFLINE;
				DISO_data.diso_state.cur_vusb_state  = DISO_ONLINE;
				DISO_data.diso_state.cur_vdc_state  = DISO_ONLINE;
				DISO_data.diso_state.cur_otg_state  = DISO_OFFLINE;
				pr_notice(" cur diso_state is %s!\n",DISO_state_s[6]);
			}
			else
				pr_notice("[%s] wrong trigger flag!\n",__func__);
			set_vusb_auxadc_irq(DISO_IRQ_ENABLE, (~trigger_flag)&0x1);
			break;
		default:
			set_vdc_auxadc_irq(DISO_IRQ_DISABLE, 0);
			set_vusb_auxadc_irq(DISO_IRQ_DISABLE, 0);
			pr_notice("[DISO]VUSB auxadc IRQ triggered ERROR OR TEST\n");
			return; /* in error or unexecpt state just return */
	}

	g_diso_state = *(int*)&DISO_data.diso_state;
	pr_notice("[DISO]g_diso_state: 0x%x\n", g_diso_state);
	DISO_data.irq_callback_func(0, NULL);

	return ;
}

#if defined(MTK_DISCRETE_SWITCH) && defined(MTK_DSC_USE_EINT)
void vdc_eint_handler()
{
	pr_notice("[diso_eint] vdc eint irq triger\n");
	DISO_data.diso_state.cur_vdc_state  = DISO_ONLINE;
	mt_eint_mask(CUST_EINT_VDC_NUM); 
	do_chrdet_int_task();
}
#endif

static unsigned int diso_get_current_voltage(int Channel)
{
    int ret = 0, data[4], i, ret_value = 0, ret_temp = 0, times = 5;

    if( IMM_IsAdcInitReady() == 0 )
    {
        pr_notice("[DISO] AUXADC is not ready");
        return 0;
    }

    i = times;
    while (i--)
    {
        ret_value = IMM_GetOneChannelValue(Channel, data, &ret_temp);

        if(ret_value == 0) {
            ret += ret_temp;
        } else {
            times = times > 1 ? times - 1 : 1;
            pr_notice("[diso_get_current_voltage] ret_value=%d, times=%d\n",
                ret_value, times);
        }
    }

    ret = ret*1500/4096 ;
    ret = ret/times;

    return  ret;
}

static void _get_diso_interrupt_state(void)
{
	int vol = 0;
	int diso_state =0;
	int check_times = 30;
	bool vin_state = KAL_FALSE;

	#ifndef VIN_SEL_FLAG
	mdelay(AUXADC_CHANNEL_DELAY_PERIOD);
	#endif

	vol = diso_get_current_voltage(AP_AUXADC_DISO_VDC_CHANNEL);
	vol =(R_DISO_DC_PULL_UP + R_DISO_DC_PULL_DOWN)*100*vol/(R_DISO_DC_PULL_DOWN)/100;
	pr_notice("[DISO]  Current DC voltage mV = %d\n", vol);

	#ifdef VIN_SEL_FLAG
	/* set gpio mode for kpoc issue as DWS has no default setting */
	mt_set_gpio_mode(vin_sel_gpio_number,0); // 0:GPIO mode
	mt_set_gpio_dir(vin_sel_gpio_number,0); // 0: input, 1: output

	if (vol > VDC_MIN_VOLTAGE/1000 && vol < VDC_MAX_VOLTAGE/1000) {
		/* make sure load switch already switch done */
		do{
			check_times--;
			#ifdef VIN_SEL_FLAG_DEFAULT_LOW
			vin_state = mt_get_gpio_in(vin_sel_gpio_number);
			#else
			vin_state = mt_get_gpio_in(vin_sel_gpio_number);
			vin_state = (~vin_state) & 0x1;
			#endif
			if(!vin_state)
				mdelay(5);
		} while ((!vin_state) && check_times);
		pr_notice("[DISO] i==%d  gpio_state= %d\n",
			check_times, mt_get_gpio_in(vin_sel_gpio_number));

		if (0 == check_times)
			diso_state &= ~0x4; //SET DC bit as 0
		else
			diso_state |= 0x4; //SET DC bit as 1
	} else {
		diso_state &= ~0x4; //SET DC bit as 0
	}
	#else
	mdelay(SWITCH_RISING_TIMING + LOAD_SWITCH_TIMING_MARGIN); /* force delay for switching as no flag for check switching done */
	if (vol > VDC_MIN_VOLTAGE/1000 && vol < VDC_MAX_VOLTAGE/1000)
			diso_state |= 0x4; //SET DC bit as 1
	else
			diso_state &= ~0x4; //SET DC bit as 0
	#endif


	vol = diso_get_current_voltage(AP_AUXADC_DISO_VUSB_CHANNEL);
	vol =(R_DISO_VBUS_PULL_UP + R_DISO_VBUS_PULL_DOWN)*100*vol/(R_DISO_VBUS_PULL_DOWN)/100;
	pr_notice("[DISO]  Current VBUS voltage  mV = %d\n",vol);

	if (vol > VBUS_MIN_VOLTAGE/1000 && vol < VBUS_MAX_VOLTAGE/1000) {
		if(!mt_usb_is_device())	{
			diso_state |= 0x1; //SET OTG bit as 1
			diso_state &= ~0x2; //SET VBUS bit as 0
		} else {
			diso_state &= ~0x1; //SET OTG bit as 0
			diso_state |= 0x2; //SET VBUS bit as 1;
		}

	} else {
		diso_state &= 0x4; //SET OTG and VBUS bit as 0
	}
	pr_notice("[DISO] DISO_STATE==0x%x \n",diso_state);
	g_diso_state = diso_state;
	return;
}

static int _get_irq_direction(int pre_vol, int cur_vol)
{
	int ret = -1;

	//threshold 1000mv
	if((cur_vol - pre_vol) > 1000)
		ret = DISO_IRQ_RISING;
	else if((pre_vol - cur_vol) > 1000)	
		ret = DISO_IRQ_FALLING;

	return ret;
}

static void _get_polling_state(void)
{
	int vdc_vol = 0, vusb_vol = 0;
	int vdc_vol_dir = -1;
	int vusb_vol_dir = -1;

	DISO_polling_channel* VDC_Polling = &DISO_Polling.vdc_polling_measure;
	DISO_polling_channel* VUSB_Polling = &DISO_Polling.vusb_polling_measure;

	vdc_vol = diso_get_current_voltage(AP_AUXADC_DISO_VDC_CHANNEL);
	vdc_vol =(R_DISO_DC_PULL_UP + R_DISO_DC_PULL_DOWN)*100*vdc_vol/(R_DISO_DC_PULL_DOWN)/100;

	vusb_vol = diso_get_current_voltage(AP_AUXADC_DISO_VUSB_CHANNEL);
	vusb_vol =(R_DISO_VBUS_PULL_UP + R_DISO_VBUS_PULL_DOWN)*100*vusb_vol/(R_DISO_VBUS_PULL_DOWN)/100;

	VDC_Polling->preVoltage = VDC_Polling->curVoltage;
	VUSB_Polling->preVoltage = VUSB_Polling->curVoltage;
	VDC_Polling->curVoltage = vdc_vol;
	VUSB_Polling->curVoltage = vusb_vol;

	if (DISO_Polling.reset_polling)
	{
		DISO_Polling.reset_polling = KAL_FALSE;
		VDC_Polling->preVoltage = vdc_vol;
		VUSB_Polling->preVoltage = vusb_vol;

		if(vdc_vol > 1000)
			vdc_vol_dir = DISO_IRQ_RISING;
		else
			vdc_vol_dir = DISO_IRQ_FALLING;

		if(vusb_vol > 1000)
			vusb_vol_dir = DISO_IRQ_RISING;
		else
			vusb_vol_dir = DISO_IRQ_FALLING;
	}
	else
	{
		//get voltage direction
		vdc_vol_dir = _get_irq_direction(VDC_Polling->preVoltage, VDC_Polling->curVoltage);
		vusb_vol_dir = _get_irq_direction(VUSB_Polling->preVoltage, VUSB_Polling->curVoltage);
	}

	if(VDC_Polling->notify_irq_en && 
		(vdc_vol_dir == VDC_Polling->notify_irq)) {
		schedule_delayed_work(&diso_polling_work, 10*HZ/1000); //10ms
		pr_notice("[%s] ready to trig VDC irq, irq: %d\n",
			__func__,VDC_Polling->notify_irq);
	} else if(VUSB_Polling->notify_irq_en && 
		(vusb_vol_dir == VUSB_Polling->notify_irq)) {
		schedule_delayed_work(&diso_polling_work, 10*HZ/1000);
		pr_notice("[%s] ready to trig VUSB irq, irq: %d\n",
			__func__, VUSB_Polling->notify_irq);
	} else if((vdc_vol == 0) && (vusb_vol == 0)) {
		VDC_Polling->notify_irq_en = 0;
		VUSB_Polling->notify_irq_en = 0;
	}
		
	return;
}

static enum hrtimer_restart diso_kthread_hrtimer_func(struct hrtimer *timer)
{
	diso_thread_timeout = KAL_TRUE;
	wake_up(&diso_polling_thread_wq);

	return HRTIMER_NORESTART;
}

static int diso_thread_kthread(void *x)
{
    /* Run on a process content */
    while (1) {
		wait_event(diso_polling_thread_wq, (diso_thread_timeout == KAL_TRUE));

		diso_thread_timeout = KAL_FALSE;

		mutex_lock(&diso_polling_mutex);

		_get_polling_state();

		if (DISO_Polling.vdc_polling_measure.notify_irq_en ||
			DISO_Polling.vusb_polling_measure.notify_irq_en)
			hrtimer_start(&diso_kthread_timer,ktime_set(0, MSEC_TO_NSEC(SW_POLLING_PERIOD)),HRTIMER_MODE_REL);
		else
			hrtimer_cancel(&diso_kthread_timer);

		mutex_unlock(&diso_polling_mutex);
	}

	return 0;
}
#endif

static unsigned int charging_diso_init(void *data)
{
	unsigned int status = STATUS_OK;

	#if defined(CONFIG_MTK_DUAL_INPUT_CHARGER_SUPPORT)
	DISO_ChargerStruct *pDISO_data = (DISO_ChargerStruct *)data;

	/* Initialization DISO Struct */
	pDISO_data->diso_state.cur_otg_state	= DISO_OFFLINE;
	pDISO_data->diso_state.cur_vusb_state = DISO_OFFLINE;
	pDISO_data->diso_state.cur_vdc_state	= DISO_OFFLINE;

	pDISO_data->diso_state.pre_otg_state	= DISO_OFFLINE;
	pDISO_data->diso_state.pre_vusb_state = DISO_OFFLINE;
	pDISO_data->diso_state.pre_vdc_state	= DISO_OFFLINE;

	pDISO_data->chr_get_diso_state = KAL_FALSE;
	pDISO_data->hv_voltage = VBUS_MAX_VOLTAGE;

	hrtimer_init(&diso_kthread_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	diso_kthread_timer.function = diso_kthread_hrtimer_func;
	INIT_DELAYED_WORK(&diso_polling_work, diso_polling_handler);

	kthread_run(diso_thread_kthread, NULL, "diso_thread_kthread");
	pr_notice("[%s] done\n", __func__);

	#if defined(MTK_DISCRETE_SWITCH) && defined(MTK_DSC_USE_EINT)
	pr_notice("[diso_eint]vdc eint irq registitation\n");
	mt_eint_set_hw_debounce(CUST_EINT_VDC_NUM, CUST_EINT_VDC_DEBOUNCE_CN);
	mt_eint_registration(CUST_EINT_VDC_NUM, CUST_EINTF_TRIGGER_LOW, vdc_eint_handler, 0);
	mt_eint_mask(CUST_EINT_VDC_NUM); 
	#endif
	#endif

	return status;	
}

static unsigned int charging_get_diso_state(void *data)
{
	unsigned int status = STATUS_OK;

	#if defined(CONFIG_MTK_DUAL_INPUT_CHARGER_SUPPORT)
	int diso_state = 0x0;
	DISO_ChargerStruct *pDISO_data = (DISO_ChargerStruct *)data;

	_get_diso_interrupt_state();
	diso_state = g_diso_state;
	pr_notice("[do_chrdet_int_task] current diso state is %s!\n", DISO_state_s[diso_state]);
	if(((diso_state >> 1) & 0x3) != 0x0)
	{
		switch (diso_state){
			case USB_ONLY:
				set_vdc_auxadc_irq(DISO_IRQ_DISABLE, 0);
				set_vusb_auxadc_irq(DISO_IRQ_DISABLE, 0);
				#ifdef MTK_DISCRETE_SWITCH
				#ifdef MTK_DSC_USE_EINT
				mt_eint_unmask(CUST_EINT_VDC_NUM); 
				#else
				set_vdc_auxadc_irq(DISO_IRQ_ENABLE, 1);
				#endif
				#endif
				pDISO_data->diso_state.cur_vusb_state  = DISO_ONLINE;
				pDISO_data->diso_state.cur_vdc_state	= DISO_OFFLINE;
				pDISO_data->diso_state.cur_otg_state	= DISO_OFFLINE;
				break;
			case DC_ONLY:
				set_vdc_auxadc_irq(DISO_IRQ_DISABLE, 0);
				set_vusb_auxadc_irq(DISO_IRQ_DISABLE, 0);
				set_vusb_auxadc_irq(DISO_IRQ_ENABLE, DISO_IRQ_RISING);
				pDISO_data->diso_state.cur_vusb_state  = DISO_OFFLINE;
				pDISO_data->diso_state.cur_vdc_state	= DISO_ONLINE;
				pDISO_data->diso_state.cur_otg_state	= DISO_OFFLINE;
				break;
			case DC_WITH_USB:
				set_vdc_auxadc_irq(DISO_IRQ_DISABLE, 0);
				set_vusb_auxadc_irq(DISO_IRQ_DISABLE, 0);
				set_vusb_auxadc_irq(DISO_IRQ_ENABLE,DISO_IRQ_FALLING);
				pDISO_data->diso_state.cur_vusb_state  = DISO_ONLINE;
				pDISO_data->diso_state.cur_vdc_state	= DISO_ONLINE;
				pDISO_data->diso_state.cur_otg_state	= DISO_OFFLINE;
				break;
			case DC_WITH_OTG:
				set_vdc_auxadc_irq(DISO_IRQ_DISABLE, 0);
				set_vusb_auxadc_irq(DISO_IRQ_DISABLE, 0);
				pDISO_data->diso_state.cur_vusb_state  = DISO_OFFLINE;
				pDISO_data->diso_state.cur_vdc_state	= DISO_ONLINE;
				pDISO_data->diso_state.cur_otg_state	= DISO_ONLINE;
				break;
			default: // OTG only also can trigger vcdt IRQ
				pDISO_data->diso_state.cur_vusb_state  = DISO_OFFLINE;
				pDISO_data->diso_state.cur_vdc_state	= DISO_OFFLINE;
				pDISO_data->diso_state.cur_otg_state	= DISO_ONLINE;
				pr_notice(" switch load vcdt irq triggerd by OTG Boost!\n");
				break; // OTG plugin no need battery sync action
		}
	}

	if (DISO_ONLINE == pDISO_data->diso_state.cur_vdc_state)
		pDISO_data->hv_voltage = VDC_MAX_VOLTAGE;
	else
		pDISO_data->hv_voltage = VBUS_MAX_VOLTAGE;
	#endif

	return status;
}


static unsigned int __maybe_unused charging_get_error_state(void)
{
	return charging_error;
}

static unsigned int charging_set_error_state(void *data)
{
	unsigned int status = STATUS_OK;
	charging_error = *(unsigned int*)(data);

	return status;
}

static unsigned int charging_set_boost_current_limit(void *data)
{
        int ret = 0;
        unsigned int current_limit = 0, reg_ilim = 0;
        current_limit = *((unsigned int *)data);
        current_limit = bmt_find_closest_level(BOOST_CURRENT_LIMIT,
                ARRAY_SIZE(BOOST_CURRENT_LIMIT), current_limit);
        reg_ilim = charging_parameter_to_value(BOOST_CURRENT_LIMIT,
                ARRAY_SIZE(BOOST_CURRENT_LIMIT), current_limit);
//        sm5414_set_boost_ilim(reg_ilim);
	sm5414_set_enboost(current_limit);
        return ret;
}

static unsigned int charging_enable_otg(void *data)
{
        int ret = 0;
        unsigned int enable = 0;

        enable = *((unsigned int *)data);
        sm5414_otg_enable(enable);

        return ret;
}

static  unsigned int charging_set_chrind_ck_pdn(void *data)
{
   int status = STATUS_OK;
       unsigned int pwr_dn;
            pwr_dn = *(unsigned int *) data;
#if defined(CONFIG_MTK_PMIC_CHIP_MT6353)
       pmic_set_register_value(PMIC_CLK_DRV_CHRIND_CK_PDN, pwr_dn);
#else
     pmic_set_register_value(PMIC_RG_DRV_CHRIND_CK_PDN, pwr_dn);
#endif
    return status;
}

static  unsigned  int charging_get_bif_vbat(void *data)
{
    int ret = 0;

    u32 vbat = 0;
        ret = mtk_bif_get_vbat(&vbat);
  if (ret < 0)
     return STATUS_UNSUPPORTED;
      *((u32 *)data) = vbat;
       return STATUS_OK;
}

static unsigned int charging_get_bif_tbat(void *data)
 {
  int ret = 0, tbat = 0;
 ret = mtk_bif_get_tbat(&tbat);
  if (ret < 0)
    return STATUS_UNSUPPORTED;
    *((int *)data) = tbat;
    return STATUS_OK;
}

static  unsigned int charging_get_bif_is_exist(void *data) 
{
    int bif_exist = 0;
    bif_exist = mtk_bif_is_hw_exist();  
    *(bool *)data = bif_exist;  
    return 0;
}

static  unsigned int charging_set_pwrstat_led_en(void *data)
{
    int ret = 0;
    unsigned int led_en;  
    led_en = *(unsigned int *) data; 
    pmic_set_register_value(PMIC_CHRIND_MODE, 0x2); /* register mode */  
    pmic_set_register_value(PMIC_CHRIND_EN_SEL, !led_en); /* 0: Auto, 1: SW */ 
    pmic_set_register_value(PMIC_CHRIND_EN, led_en);      
    return ret;    
} 

static unsigned int (* const charging_func[CHARGING_CMD_NUMBER])(void *data)= {
	[ CHARGING_CMD_INIT] = charging_hw_init,
	[ CHARGING_CMD_DUMP_REGISTER] = charging_dump_register ,
	[ CHARGING_CMD_ENABLE] = charging_enable ,
	[ CHARGING_CMD_SET_CV_VOLTAGE] = charging_set_cv_voltage ,
	[ CHARGING_CMD_GET_CURRENT] = charging_get_current ,
	[ CHARGING_CMD_SET_CURRENT] = charging_set_current ,
	[ CHARGING_CMD_SET_INPUT_CURRENT] = charging_set_input_current ,
	[ CHARGING_CMD_GET_CHARGING_STATUS] = charging_get_charging_status ,
	[ CHARGING_CMD_RESET_WATCH_DOG_TIMER] = charging_reset_watch_dog_timer ,
	[ CHARGING_CMD_SET_HV_THRESHOLD] = charging_set_hv_threshold ,
	[ CHARGING_CMD_GET_HV_STATUS] = charging_get_hv_status ,
	[ CHARGING_CMD_GET_BATTERY_STATUS] = charging_get_battery_status ,
	[ CHARGING_CMD_GET_CHARGER_DET_STATUS] = charging_get_charger_det_status ,
	[ CHARGING_CMD_GET_CHARGER_TYPE] = charging_get_charger_type ,
	[ CHARGING_CMD_GET_IS_PCM_TIMER_TRIGGER] = charging_get_is_pcm_timer_trigger ,
	[ CHARGING_CMD_SET_PLATFORM_RESET] = charging_set_platform_reset ,
	[ CHARGING_CMD_GET_PLATFORM_BOOT_MODE] = charging_get_platfrom_boot_mode ,
	[ CHARGING_CMD_SET_POWER_OFF] = charging_set_power_off ,
	[ CHARGING_CMD_GET_POWER_SOURCE] = charging_get_power_source ,
	[ CHARGING_CMD_GET_CSDAC_FALL_FLAG] = charging_get_csdac_full_flag ,
	[ CHARGING_CMD_SET_TA_CURRENT_PATTERN] = charging_set_ta_current_pattern ,
	[ CHARGING_CMD_SET_ERROR_STATE] = charging_set_error_state ,
	[ CHARGING_CMD_DISO_INIT] = charging_diso_init ,
	[ CHARGING_CMD_GET_DISO_STATE ] = charging_get_diso_state,
	[CHARGING_CMD_SET_BOOST_CURRENT_LIMIT] = charging_set_boost_current_limit ,
	[CHARGING_CMD_ENABLE_OTG] = charging_enable_otg ,
	[CHARGING_CMD_GET_BIF_IS_EXIST] = charging_get_bif_is_exist,
	[CHARGING_CMD_SET_PWRSTAT_LED_EN] = charging_set_pwrstat_led_en,
	[CHARGING_CMD_SET_CHRIND_CK_PDN] = charging_set_chrind_ck_pdn,
	[CHARGING_CMD_GET_BIF_VBAT] = charging_get_bif_vbat,
	[CHARGING_CMD_GET_BIF_TBAT] = charging_get_bif_tbat,
};

 
/*
* FUNCTION
*		Internal_chr_control_handler
*
* DESCRIPTION															 
*		 This function is called to set the charger hw
*
* CALLS  
*
* PARAMETERS
*		None
*	 
* RETURNS
*		
*
* GLOBALS AFFECTED
*	   None
*/
int sm5414_chr_control_interface(CHARGING_CTRL_CMD cmd, void *data);
int __weak chr_control_interface(CHARGING_CTRL_CMD cmd, void *data) {
	return sm5414_chr_control_interface(cmd, data);
}

int sm5414_chr_control_interface(CHARGING_CTRL_CMD cmd, void *data)
{
	int status;
	if(cmd < CHARGING_CMD_NUMBER)
	{
		if (charging_func[cmd] != NULL)
		{
			status = charging_func[cmd](data);
		}
		else
		{
			return STATUS_UNSUPPORTED;
		}

	}
	else
	{
		return STATUS_UNSUPPORTED;
	}
	

	return status;
}
