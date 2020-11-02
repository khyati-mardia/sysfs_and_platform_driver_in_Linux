/*
 * A sample program to show the binding of platform driver and device.
 */

#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/miscdevice.h>

#include "platform_device.h"

int Number_Of_Devices; //total number of devices during insmod
module_param(Number_Of_Devices,int,0); //passing the number of devices

/*
static struct P_chip P1_chip = {
		.name	= "xyz01",
		.dev_no 	= 20,
		.plf_dev = {
			.name	= "abcd",
			.id	= -1,
		}
};

static struct P_chip P2_chip = {
		.name	= "xyz02",
		.dev_no 	= 55,
		.plf_dev = {
			.name	= "defg",
			.id	= -1,
		}
};*/

void release_driver(struct device *dev){
	printk(KERN_INFO"Release devices");
}

/**
 * register the device when module is initiated
 */

static int p_device_init(void)
{
	/* Register the device */

	int i, ret;
	struct class* temp;
	char* name;

	printk(KERN_INFO "Initialization \n");

	//initialisePinData();

	/* Allocate memory for the per-device structure */
	HCSR_devp = kzalloc(sizeof(struct HCSR_dev)*Number_Of_Devices, GFP_KERNEL);

	if (!HCSR_devp) {
		printk("Bad Kmalloc\n"); 
		return -ENOMEM;
	}
	

	/* Populate sysfs entries */
	temp = class_create(THIS_MODULE, DEV_NAME);

	for(i=0;i<Number_Of_Devices;i++){

		HCSR_devp[i].HCSR_dev_class=temp;

		//Register platform device for each per device structure

		name = (char*) kzalloc(10* (sizeof (char)), GFP_KERNEL);
		sprintf(name,"hcsr%d",i);
		HCSR_devp[i].plf_dev.name = name;
		HCSR_devp[i].miscdev.name = name;
		HCSR_devp[i].plf_dev.dev.release = release_driver;
		HCSR_devp[i].name = name;
		//HCSR_devp[i].dev_no = i;

		ret = platform_device_register(&(HCSR_devp[i].plf_dev));

	    if (ret) {
			printk("Bad miscellaneous device\n");
			return ret;
			}
	}

	printk("HCSR driver initialized.\n\n");

	return 0;

}

static void p_device_exit(void)
{

	int i=0;
	struct class* temp;

	printk(KERN_INFO "Exit the driver");

	temp = HCSR_devp[i].HCSR_dev_class;

	for(i=0;i<Number_Of_Devices;i++){

	//Deregister platform device for each per device structure

	platform_device_unregister(&(HCSR_devp[i].plf_dev));
	
	}


	kfree(HCSR_devp);

		/* Destroy driver_class */
	class_destroy(temp);

	printk(KERN_ALERT "Goodbye, unregister the device\n");
}

module_init(p_device_init);
module_exit(p_device_exit);
MODULE_LICENSE("GPL");
