#include <linux/platform_device.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/cdev.h>
#include <linux/moduleparam.h>
#include <asm/uaccess.h>
#include <linux/types.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/device.h>
#include <linux/mutex.h>
#include <linux/ioctl.h>
#include <linux/version.h>
#include <linux/gpio.h>
#include <linux/miscdevice.h>
#include <linux/interrupt.h>
#include <linux/kthread.h>
#include <linux/delay.h>
#include <linux/unistd.h>
#include <linux/spinlock_types.h>
#include <linux/workqueue.h>
#include <linux/mutex.h>

#ifndef __SAMPLE_PLATFORM_H__
  
#define __SAMPLE_PLATFORM_H__


//struct device *HCSR_device;				/* HCSR device */
//dev_t HCSR_dev1 = 1;

#define DEV_NAME  "HCSR"  		   // device name to be created and registered

#define DRIVER_NAME		"platform_driver_0"

/*struct P_chip {
		char 			*name;
		int				dev_no;
		struct platform_device 	plf_dev;
};*/

struct sensor {
	int io1; //trigger
	int io2; //echo
}sensor1;   // Input structure received from the user.

struct param {
	int samples; //samples per measurement
	int period; //sampling period
}param1;


/*write buffer*/
struct buffer{
	uint64_t buf_array; //buffer sized array
	uint64_t time_stamp;   //time stamps
};

/* per device structure */
struct HCSR_dev {
	struct platform_device 	plf_dev;        /* platform driver */
	//struct cdev cdev;			/* character device */
	struct miscdevice miscdev;              /* The miscellaneous device structure */
	struct sensor sensor1;			/* Trigger and echo pins structure */
	struct param param1;			/*sampling measurement period structure */
	uint64_t start_time;			/*start time*/
	uint64_t stop_time;			/*stop time*/
	int flag;				/*flag for the interrupt*/
	int delay;				/*delay to provide sleep in trigger*/
	uint64_t* array;			/*array to save m+2 samples*/
	int index;				/*array index*/
	struct work_struct work;		/*work structure*/	
	struct buffer buf[5];			/*buffer structure*/	
	int buf_index_write;			/*buffer index write*/
	int buf_index_read;			/*buffer index read*/	
	int buf_length;				/*buffer length*/		
	int ongoingMeasurement;			/*flag for ongoing measurement*/
	struct mutex buff_mutex;		/*mutex*/
	struct semaphore sem;			/*semaphore*/
	char* name;				/*name of a device*/
	int enable;				/*enable pin*/
	struct attribute_group *group; /*attributes*/
	struct class *HCSR_dev_class;          /* Tie with the device model */
	uint64_t distance;						/*distance value*/
	struct device* HCSR_device;
}*HCSR_devp;

#endif /* __GPIO_FUNC_H__ */
