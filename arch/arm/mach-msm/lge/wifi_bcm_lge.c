
#ifdef CONFIG_OF
#include <linux/of.h>
#include <linux/of_device.h>
#endif /* CONFIG_OF */
#include <linux/gpio.h>
#include <linux/regulator/consumer.h>
#include <asm/system_info.h>



#include <linux/of_gpio.h>

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/gpio.h>
#include <linux/irq.h>
#include <linux/if.h>
#include <linux/random.h>
#include <linux/skbuff.h>
#include <linux/wlan_plat.h>
#include <linux/module.h>
#include <asm/io.h>
#include "../pcie.h" //PTEST

#define ASYNC_INIT	/* use async_schedule to reduce boot-up time */
#ifdef ASYNC_INIT
#include <linux/async.h>
#endif

/*
	Memory allocation is done at dhd_attach
	so static allocation is only necessary in module type driver
*/
#if defined (CONFIG_BROADCOM_WIFI_RESERVED_MEM)

#define PREALLOC_WLAN_NUMBER_OF_SECTIONS    8
#define PREALLOC_WLAN_NUMBER_OF_BUFFERS     160
#define PREALLOC_WLAN_SECTION_HEADER        24

/*
This definition is from driver's dhd.h

enum dhd_prealloc_index {
	DHD_PREALLOC_PROT = 0, 
	DHD_PREALLOC_RXBUF, 
	DHD_PREALLOC_DATABUF, 
	DHD_PREALLOC_OSL_BUF, 
#if defined(STATIC_WL_PRIV_STRUCT) 
	DHD_PREALLOC_WIPHY_ESCAN0 = 5, 
#if defined(CUSTOMER_HW4) && defined(DUAL_ESCAN_RESULT_BUFFER) 
	DHD_PREALLOC_WIPHY_ESCAN1, 
#endif 
#endif
	DHD_PREALLOC_DHD_INFO = 7 
};
*/

/*
	DHD_PREALLOC_RXBUF, DHD_PREALLOC_DATABUF, DHD_PREALLOC_OSL_BUF
	and static socket buffer are not called at dhd.
	So set size of this section to 0
*/

#define WLAN_SECTION_SIZE_0	(PREALLOC_WLAN_NUMBER_OF_BUFFERS * 128)
#define WLAN_SECTION_SIZE_1 	0
#define WLAN_SECTION_SIZE_2 	0
#define WLAN_SECTION_SIZE_3 	(PREALLOC_WLAN_NUMBER_OF_BUFFERS*1024)
#define WLAN_SECTION_SIZE_4 	0 /* Index 4 is static socket buffer */
#define WLAN_SECTION_SIZE_5 	(65536)
#define WLAN_SECTION_SIZE_6 	(65536)
#define WLAN_SECTION_SIZE_7 	(16 * 1024)

struct wlan_mem_prealloc {
        void *mem_ptr;
        unsigned long size;
};

static struct wlan_mem_prealloc wlan_mem_array[PREALLOC_WLAN_NUMBER_OF_SECTIONS] = {
        { NULL, (WLAN_SECTION_SIZE_0 + PREALLOC_WLAN_SECTION_HEADER) },
        { NULL, (WLAN_SECTION_SIZE_1) },
        { NULL, (WLAN_SECTION_SIZE_2) },
        { NULL, (WLAN_SECTION_SIZE_3) },
        { NULL, (WLAN_SECTION_SIZE_4) },       
        { NULL, (WLAN_SECTION_SIZE_5) },
        { NULL, (WLAN_SECTION_SIZE_6) },
        { NULL, (WLAN_SECTION_SIZE_7) }         
};

void *bcm_wlan_get_mem(int section, unsigned long size)
{
	if ((section < 0) || (section > PREALLOC_WLAN_NUMBER_OF_SECTIONS))
		return NULL;

	if (wlan_mem_array[section].size < size)
		return NULL;

	return wlan_mem_array[section].mem_ptr;
}

static int bcm_init_wlan_mem(void)
{
	int i;
	int j;

	for (i = 0 ; i < PREALLOC_WLAN_NUMBER_OF_SECTIONS ; i++) {
		if (wlan_mem_array[i].size) {
			wlan_mem_array[i].mem_ptr =kmalloc(wlan_mem_array[i].size, GFP_KERNEL);
		}		
		if (!wlan_mem_array[i].mem_ptr && wlan_mem_array[i].size)
			goto err_mem_alloc;
	}
	
	printk("%s: WIFI MEM Allocated\n", __FUNCTION__);
	return 0;

 err_mem_alloc:
	pr_err("Failed to mem_alloc for WLAN\n");
	for (j = 0 ; j < i ; j++) {
		if (wlan_mem_array[i].size) {
			kfree(wlan_mem_array[j].mem_ptr);
		}
	}
	return -ENOMEM;
}

#endif /* defined (CONFIG_BROADCOM_WIFI_RESERVED_MEM) */


int bcm_wifi_reset(int on)
{
    return 0;
}


#define PCIE_VENDOR_ID_RCP	0x17cb
#define PCIE_DEVICE_ID_RCP	0x0101
#define PCIE_RCP_NAME		"0000:00:00.0"
#define PCIE_POWERUP_RETRY	5

int bcm_wifi_carddetect(int detect)
{
	int ret = 0;
	int rc_idx = 0;
	int found = 0;
	int count = 0;
	struct pci_dev *pcidev = NULL;

	if (detect == 1) {
		do {
			pcidev = pci_get_device(PCIE_VENDOR_ID_RCP, PCIE_DEVICE_ID_RCP, pcidev);
			if (pcidev && (!strcmp(pci_name(pcidev), PCIE_RCP_NAME) && pci_is_enabled(pcidev))) {
				printk("P:%s:PCI device found[%X:%X]!!!\n", __func__, pcidev->vendor, pcidev->device);
				found = 1;
			} else {
				count++;
				printk("P:%s:retry count[%d]\n", __func__, count);
				msleep(100);
			}
		} while(!found && (count < PCIE_POWERUP_RETRY));

		if (!found) {
			ret = msm_pcie_enumerate(rc_idx);
			if (ret != 0) {
				printk("P:%s:PCI enumeration was failed[%d]\n", __func__, ret);
			}
		}
	}
	return ret;
}

static int bcm_wifi_get_mac_addr(unsigned char *buf)
{
	uint rand_mac;
	static unsigned char mymac[6] = {0,};
	const unsigned char nullmac[6] = {0,};
	pr_debug("%s: %p\n", __func__, buf);

	if (buf == NULL)
		return -EAGAIN;

	if (memcmp( mymac, nullmac, 6) != 0 ) {		/* Mac displayed from UI are never updated..
		   So, mac obtained on initial time is used */
		memcpy(buf, mymac, 6);
		return 0;
	}

	prandom_seed((uint)jiffies);
	rand_mac = prandom_u32();

	buf[0] = 0x00;
	buf[1] = 0x90;
	buf[2] = 0x4c;
	buf[3] = (unsigned char)rand_mac;
	buf[4] = (unsigned char)(rand_mac >> 8);
	buf[5] = (unsigned char)(rand_mac >> 16);

	memcpy(mymac, buf, 6);

	printk(KERN_INFO "[%s] Exiting. MyMac :  %x : %x : %x : %x : %x : %x", __func__ , buf[0], buf[1], buf[2], buf[3], buf[4], buf[5]);

	return 0;
}

#define COUNTRY_BUF_SZ	4
struct cntry_locales_custom {
	char iso_abbrev[COUNTRY_BUF_SZ];
	char custom_locale[COUNTRY_BUF_SZ];
	int custom_locale_rev;
};

/* Customized Locale table */
const struct cntry_locales_custom bcm_wifi_translate_custom_table[] = {
/* Table should be filled out based on custom platform regulatory requirement */
	{"",   "XZ", 11},	/* Universal if Country code is unknown or empty */
	{"CK", "XZ", 11},	/* Universal if Country code is Cook Island (13.4.27)*/
	{"CU", "XZ", 11},	/* Universal if Country code is Cuba (13.4.27)*/
	{"FO", "XZ", 11},	/* Universal if Country code is Faroe Island (13.4.27)*/
	{"GI", "XZ", 11},	/* Universal if Country code is Gibraltar (13.4.27)*/
	{"IL", "XZ", 11},	/* Universal if Country code is West Bank(Israel) (13.4.27)*/
	{"IM", "XZ", 11},	/* Universal if Country code is Isle of Man (13.4.27)*/
	{"IR", "XZ", 11},	/* Universal if Country code is IRAN, (ISLAMIC REPUBLIC OF) */
	{"JE", "XZ", 11},	/* Universal if Country code is Jersey (13.4.27)*/
	{"KP", "XZ", 11},	/* Universal if Country code is North Korea (13.4.27)*/
	{"MH", "XZ", 11},	/* Universal if Country code is MARSHALL ISLANDS */
	{"NF", "XZ", 11},	/* Universal if Country code is Norfolk Island (13.4.27)*/
	{"NU", "XZ", 11},	/* Universal if Country code is Niue (13.4.27)*/
	{"PM", "XZ", 11},	/* Universal if Country code is Saint Pierre and Miquelon (13.4.27)*/
	{"PN", "XZ", 11},	/* Universal if Country code is Pitcairn Islands (13.4.27)*/
	{"PS", "XZ", 11},	/* Universal if Country code is PALESTINIAN TERRITORY, OCCUPIED */
	{"SD", "XZ", 11},	/* Universal if Country code is SUDAN */
	{"SS", "XZ", 11},	/* Universal if Country code is South_Sudan (13.4.27)*/
	{"SY", "XZ", 11},	/* Universal if Country code is SYRIAN ARAB REPUBLIC */
	{"TL", "XZ", 11},	/* Universal if Country code is TIMOR-LESTE (EAST TIMOR) */
	{"AD", "AD", 0},
	{"AE", "AE", 6},
	{"AF", "AF", 0},
	{"AG", "AG", 2},
	{"AI", "AI", 1},
	{"AL", "AL", 2},
	{"AM", "AM", 0},
	{"AN", "AN", 3},
	{"AO", "AO", 0},
	{"AR", "AU", 6},
	{"AS", "AS", 12},  /* changed 2 -> 12*/
	{"AT", "AT", 4},
	{"AU", "AU", 6},
	{"AW", "AW", 2},
	{"AZ", "AZ", 2},
	{"BA", "BA", 2},
	{"BB", "BB", 0},
	{"BD", "BD", 2},
	{"BE", "BE", 4},
	{"BF", "BF", 0},
	{"BG", "BG", 4},
	{"BH", "BH", 4},  /* changed 24 -> 4*/
	{"BI", "BI", 0},
	{"BJ", "BJ", 0},
	{"BM", "BM", 12},
	{"BN", "BN", 4},
	{"BO", "BO", 0},
	{"BR", "BR", 15},
	{"BS", "BS", 2},
	{"BT", "BT", 0},
	{"BW", "BW", 0},
	{"BY", "BY", 3},
	{"BZ", "BZ", 0},
	{"CA", "US", 118},
	{"CD", "CD", 0},
	{"CF", "CF", 0},
	{"CG", "CG", 0},
	{"CH", "CH", 4},
	{"CI", "CI", 0},
	{"CL", "CL", 0},
	{"CM", "CM", 0},
	{"CN", "CN", 24},
	{"CO", "CO", 17},
	{"CR", "CR", 17},
	{"CV", "CV", 0},
	{"CX", "CX", 0},
	{"CY", "CY", 4},
	{"CZ", "CZ", 4},
	{"DE", "DE", 7},
	{"DJ", "DJ", 0},
	{"DK", "DK", 4},
	{"DM", "DM", 0},
	{"DO", "DO", 0},
	{"DZ", "DZ", 1},
	{"EC", "EC", 21},
	{"EE", "EE", 4},
	{"EG", "EG", 13},
	{"ER", "ER", 0},
	{"ES", "ES", 4},
	{"ET", "ET", 2},
	{"FI", "FI", 4},
	{"FJ", "FJ", 0},
	{"FK", "FK", 0},
	{"FM", "FM", 0},
	{"FR", "FR", 5},
	{"GA", "GA", 0},
	{"GB", "GB", 6},
	{"GD", "GD", 2},
	{"GE", "GE", 0},
	{"GF", "GF", 2},
	{"GH", "GH", 0},
	{"GM", "GM", 0},
	{"GN", "GN", 0},
	{"GP", "GP", 2},
	{"GQ", "GQ", 0},
	{"GR", "GR", 4},
	{"GT", "GT", 1},
	{"GU", "GU", 12},
	{"GW", "GW", 0},
	{"GY", "GY", 0},
	{"HK", "HK", 2},
	{"HN", "HN", 0},
	{"HR", "HR", 4},
	{"HT", "HT", 0},
	{"HU", "HU", 4},
	{"ID", "ID", 1},
	{"IE", "IE", 5},
	{"IL", "BO", 0},    //IL/7 -> BO/0
	{"IN", "IN", 3},
	{"IQ", "IQ", 0},
	{"IS", "IS", 4},
	{"IT", "IT", 4},
	{"JM", "JM", 0},
	{"JO", "JO", 3},
#if defined (CONFIG_MACH_MSM8974_G2_DCM)
	{"JP", "JP", 45},
#else
	{"JP", "JP", 58},
#endif
	{"KE", "SA", 0},
	{"KG", "KG", 0},
	{"KH", "KH", 2},
	{"KI", "KI", 0},
	{"KM", "KM", 0},
	{"KN", "KN", 0},
	{"KR", "KR", 57},
	{"KW", "KW", 5},
	{"KY", "KY", 3},
	{"KZ", "KZ", 0},
	{"LA", "LA", 2},
	{"LB", "LB", 5},
	{"LC", "LC", 0},
	{"LI", "LI", 4},
	{"LK", "LK", 1},
	{"LR", "LR", 0},
	{"LS", "LS", 2},
	{"LT", "LT", 4},
	{"LU", "LU", 3},
	{"LV", "LV", 4},
	{"LY", "LY", 0},
	{"MA", "MA", 2},
	{"MC", "MC", 1},
	{"MD", "MD", 2},
	{"ME", "ME", 2},
	{"MF", "MF", 0},
	{"MG", "MG", 0},
	{"MK", "MK", 2},
	{"ML", "ML", 0},
	{"MM", "MM", 0},
	{"MN", "MN", 1},
	{"MO", "MO", 2},
	{"MP", "MP", 0},
	{"MQ", "MQ", 2},
	{"MR", "MR", 2},
	{"MS", "MS", 0},
	{"MT", "MT", 4},
	{"MU", "MU", 2},
	{"MV", "MV", 3},
	{"MW", "MW", 1},
	{"MX", "AU", 6},
	{"MY", "MY", 3},
	{"MZ", "MZ", 0},
	{"NA", "NA", 0},
	{"NC", "NC", 0},
	{"NE", "NE", 0},
	{"NG", "NG", 0},
	{"NI", "NI", 2},
	{"NL", "NL", 4},
	{"NO", "NO", 4},
	{"NP", "ID", 5},
	{"NR", "NR", 0},
	{"NZ", "NZ", 4},
	{"OM", "OM", 4},
	{"PA", "PA", 17},
	{"PE", "PE", 20},
	{"PF", "PF", 0},
	{"PG", "AU", 6},
	{"PH", "PH", 5},
	{"PK", "PK", 0},
	{"PL", "PL", 4},
	{"PR", "US", 118},
	{"PT", "PT", 4},
	{"PW", "PW", 0},
	{"PY", "PY", 2},
	{"QA", "QA", 0},
	{"RE", "RE", 2},
	{"RKS", "KG", 0},
	{"RO", "RO", 4},
	{"RS", "RS", 2},
	{"RU", "RU", 13},
	{"RW", "RW", 0},
	{"SA", "SA", 0},
	{"SB", "SB", 0},
	{"SC", "SC", 0},
	{"SE", "SE", 4},
	{"SG", "SG", 0},
	{"SI", "SI", 4},
	{"SK", "SK", 4},
	{"SL", "SL", 0},
	{"SM", "SM", 0},
	{"SN", "SN", 2},
	{"SO", "SO", 0},
	{"SR", "SR", 0},
	{"ST", "ST", 0},
	{"SV", "SV", 25},
	{"SZ", "SZ", 0},
	{"TC", "TC", 0},
	{"TD", "TD", 0},
	{"TF", "TF", 0},
	{"TG", "TG", 0},
	{"TH", "TH", 5},
	{"TJ", "TJ", 0},
	{"TM", "TM", 0},
	{"TN", "TN", 1},
	{"TO", "TO", 0},
	{"TR", "TR", 7},
	{"TT", "TT", 3},
	{"TV", "TV", 0},
	{"TW", "TW", 1},
	{"TZ", "TZ", 0},
	{"UA", "UA", 8},
	{"UG", "UG", 2},
	{"US", "US", 118},
	{"UY", "UY", 1},
	{"UZ", "MA", 2},
	{"VA", "VA", 2},
	{"VC", "VC", 0},
	{"VE", "VE", 3},
	{"VG", "VG", 2},
	{"VI", "US", 118},
	{"VN", "VN", 4},
	{"VU", "VU", 0},
	{"WS", "WS", 0},
	{"YE", "YE", 0},
	{"YT", "YT", 2},
	{"ZA", "ZA", 6},
	{"ZM", "LA", 2},
	{"ZW", "ZW", 0},
};

static void *bcm_wifi_get_country_code(char *ccode)
{
	int size, i;
	static struct cntry_locales_custom country_code;

	size = ARRAY_SIZE(bcm_wifi_translate_custom_table);

	if ((size == 0) || (ccode == NULL))
		return NULL;

	for (i = 0; i < size; i++) {
		if (strcmp(ccode, bcm_wifi_translate_custom_table[i].iso_abbrev) == 0)
			return (void *)&bcm_wifi_translate_custom_table[i];
	}

	memset(&country_code, 0, sizeof(struct cntry_locales_custom));
	strlcpy(country_code.custom_locale, ccode, COUNTRY_BUF_SZ);

	return (void *)&country_code;
}

#define WLAN_POWER		82
uint8_t wifi_power_gpio = WLAN_POWER;

int bcm_wifi_power(int on)
{
	#if 1
	int ret = 0;

	printk(KERN_INFO "%s: wifi_power_gpio = %d, on = %d\n", __FUNCTION__, wifi_power_gpio, on);
	if(on){
		ret = gpio_direction_output(wifi_power_gpio, 1);
		if (ret) {
			printk(KERN_ERR "%s: WL_REG_ON  failed to pull up ret = %d\n", __FUNCTION__, ret);
		}
		/* WLAN chip to reset */
		msleep(150); /* for booting time save */
		printk(KERN_INFO "J:%s: applied delay. 150ms\n", __func__);
		printk(KERN_ERR "%s: wifi power successed to pull up\n", __func__);

	}else{
		ret = gpio_direction_output(wifi_power_gpio, 0);
		if(ret){
			printk(KERN_ERR "%s: WL_REG_ON  failed to pull down ret = %d\n", __FUNCTION__,ret);
		}
	}
	#else
	printk(KERN_INFO "This is PCIe, so do nothing here\n");
	printk(KERN_INFO "%s: wifi_power_gpio = %d, on = %d\n", __FUNCTION__, wifi_power_gpio, on);
	#endif
	return 0;
}

int __init bcm_wifi_init_gpio_mem(struct platform_device *platdev)
{
	printk(KERN_INFO "%s: %d\n", __FUNCTION__, wifi_power_gpio);

	/* WLAN_POWER */
	if (gpio_tlmm_config(GPIO_CFG(wifi_power_gpio, 0, GPIO_CFG_OUTPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_8MA), GPIO_CFG_ENABLE))
		printk(KERN_ERR "%s: Failed to configure WLAN_POWER\n", __func__);

	if (gpio_request(wifi_power_gpio, "WL_REG_ON"))
		printk(KERN_INFO "Failed to request gpio %d for WL_REG_ON\n", wifi_power_gpio);

	//Not sure, but I guess this might be moved to device tree
	//at pcie
	//wlan-enable-gpio = <&msmgpio 82 1>;

	if (gpio_direction_output(wifi_power_gpio, 0))
		printk(KERN_ERR "%s: WL_REG_ON  failed direction out\n", __func__);

	msleep(10);
	
#if defined (CONFIG_BROADCOM_WIFI_RESERVED_MEM)
	bcm_init_wlan_mem();
#endif
	printk(KERN_INFO "bcm_wifi_init_gpio_mem successfully\n");

	return 0;
}


struct wifi_platform_data bcm_wifi_control = {
	.set_power		= bcm_wifi_power,
	.set_reset		= bcm_wifi_reset,
	.set_carddetect		= bcm_wifi_carddetect, //PTEST
#if defined (CONFIG_BROADCOM_WIFI_RESERVED_MEM)
	.mem_prealloc	= bcm_wlan_get_mem,
#endif
	.get_mac_addr   = bcm_wifi_get_mac_addr,
	.get_country_code = bcm_wifi_get_country_code,
};

static struct platform_device bcm_wifi_device = {
	.name           = "bcmdhd_wlan",
	.id             = 1,
//	.num_resources  = 0,
	.dev            = {
		.platform_data = &bcm_wifi_control,
	},
};

#ifdef ASYNC_INIT
static void __init init_bcm_wifi_async(void *data, async_cookie_t cookie)
{
	bcm_wifi_init_gpio_mem(&bcm_wifi_device);
	platform_device_register(&bcm_wifi_device);
}
#endif


static int __init init_bcm_wifi(void)
{
#ifdef CONFIG_WIFI_CONTROL_FUNC
#ifdef ASYNC_INIT
	async_schedule(init_bcm_wifi_async, NULL);
#else
	bcm_wifi_init_gpio_mem(&bcm_wifi_device);
	platform_device_register(&bcm_wifi_device);	
#endif /* ASYNC_INIT */
#endif
	return 0;
}

subsys_initcall(init_bcm_wifi);

