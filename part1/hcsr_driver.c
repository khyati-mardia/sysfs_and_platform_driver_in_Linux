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

#define IOC_MAGIC1 'k' // defines the magic number
#define IOC_MAGIC2 'a' // defines the magic number
#define SEQ_NR_LOCAL '0'

int Number_Of_Devices; //total number of devices during insmod

module_param(Number_Of_Devices,int,0); //passing the number of devices

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
	//struct cdev cdev;						 /* character device */
	struct miscdevice miscdev;               /* The miscellaneous device structure */
	struct sensor sensor1;					/* Trigger and echo pins structure */
	struct param param1;					/*sampling measurement period structure */
	uint64_t start_time;					/*start time*/
	uint64_t stop_time;						/*stop time*/
	int flag;								/*flag for the interrupt*/
	int delay;								/*delay to provide sleep in trigger*/
	uint64_t* array;						/*array to save m+2 samples*/
	int index;								/*array index*/
	struct work_struct work;				/*work structure*/	
	struct buffer buf[5];					/*buffer structure*/	
	int buf_index_write;					/*buffer index write*/
	int buf_index_read;						/*buffer index read*/	
	int buf_length;							/*buffer length*/		
	int ongoingMeasurement;					/*flag for ongoing measurement*/
	struct mutex buff_mutex;				/*mutex*/
	struct semaphore sem;					/*semaphore*/
	char* name;								/*name of a device*/
}*HCSR_devp;

struct PinMapping {
	int gpioPin1;
	int gpioPin2;
	int gpioPin3;
	int gpioPin4;
}pinMappings[20]; // Defining the Pin Mapping structure.


#define CONFIG_PINS _IOW(IOC_MAGIC1, SEQ_NR_LOCAL, struct sensor) // Define IOCTL commands
#define SET_PARAMETERS _IOW(IOC_MAGIC2, SEQ_NR_LOCAL, struct param) // Define IOCTL commands
//typedef irqreturn_t (*irq_handler_t)(int, void *);

void initialisePinData(void);
void configuringPins(struct HCSR_dev *HCSR_devp);

static inline uint64_t __attribute__((__always_inline__))
tsc(void)
{
    uint32_t a, d;
    __asm __volatile("rdtsc" : "=a" (a), "=d" (d));
    return ((uint64_t) a) | (((uint64_t) d) << 32);
}

/*
* Open HCSR driver
*/
int HCSR_driver_open(struct inode *inode, struct file *file)
{
	printk(KERN_INFO "Opening the HCSR driver \n");
	
	return 0;
}

/*
 * Release HCSR driver
 */
int HCSR_driver_release(struct inode *inode, struct file *file)
{
	struct HCSR_dev *HCSR_devp = container_of(file->private_data, struct HCSR_dev, miscdev);

	//free irq
	free_irq(gpio_to_irq(pinMappings[(HCSR_devp->sensor1).io2].gpioPin1) , (void *)HCSR_devp);

	//free array
	kfree(HCSR_devp->array);

	//free queue
	flush_scheduled_work();

	//free mutex
	mutex_destroy(&HCSR_devp->buff_mutex);

	if (pinMappings[(HCSR_devp->sensor1).io1].gpioPin1 != 81) {
		printk("\n GPIO Free command :: Trigger :: gpio "
				"%d %s\n" , pinMappings[(HCSR_devp->sensor1).io1].gpioPin1,HCSR_devp->miscdev.name);
		gpio_free(pinMappings[(HCSR_devp->sensor1).io1].gpioPin1);

	}
	if (pinMappings[(HCSR_devp->sensor1).io1].gpioPin2 != 81) {
		printk("\n GPIO Free command :: Trigger :: gpio"
				"%d %s\n" , pinMappings[(HCSR_devp->sensor1).io1].gpioPin2,HCSR_devp->miscdev.name);
		gpio_free(pinMappings[(HCSR_devp->sensor1).io1].gpioPin2);

	}
	if (pinMappings[(HCSR_devp->sensor1).io1].gpioPin3 != 81) {
		printk("\n GPIO Free command :: Trigger :: gpio "
				"%d %s\n" , pinMappings[(HCSR_devp->sensor1).io1].gpioPin3,HCSR_devp->miscdev.name);
		gpio_free(pinMappings[(HCSR_devp->sensor1).io1].gpioPin3);

	}
	if (pinMappings[(HCSR_devp->sensor1).io1].gpioPin4  != 81) {
		printk("\n GPIO Free command :: Trigger :: gpio "
				"%d %s\n" , pinMappings[(HCSR_devp->sensor1).io1].gpioPin4,HCSR_devp->miscdev.name);
		gpio_free(pinMappings[(HCSR_devp->sensor1).io1].gpioPin4);
	}
	if (pinMappings[(HCSR_devp->sensor1).io2].gpioPin1 != 81) {
		printk("\n GPIO Free command :: Echo :: gpio"
				"%d %s\n" , pinMappings[(HCSR_devp->sensor1).io2].gpioPin1,HCSR_devp->miscdev.name);
		gpio_free(pinMappings[(HCSR_devp->sensor1).io2].gpioPin1);

	}
	if (pinMappings[(HCSR_devp->sensor1).io2].gpioPin2 != 81) {
		printk("\n GPIO Free command :: Echo :: gpio"
				"%d %s\n" , pinMappings[(HCSR_devp->sensor1).io2].gpioPin2,HCSR_devp->miscdev.name);
		gpio_free(pinMappings[(HCSR_devp->sensor1).io2].gpioPin2);

	}
	if (pinMappings[(HCSR_devp->sensor1).io2].gpioPin3 != 81) {
		printk("\n GPIO Free command :: Echo :: gpio"
				"%d %s\n" , pinMappings[(HCSR_devp->sensor1).io2].gpioPin3,HCSR_devp->miscdev.name);
		gpio_free(pinMappings[(HCSR_devp->sensor1).io2].gpioPin3);

	}
	if (pinMappings[(HCSR_devp->sensor1).io2].gpioPin4 != 81) {
		printk("\n GPIO Free command :: Echo :: gpio"
				"%d %s\n" , pinMappings[(HCSR_devp->sensor1).io2].gpioPin4,HCSR_devp->miscdev.name);
		gpio_free(pinMappings[(HCSR_devp->sensor1).io2].gpioPin4);
	}

	printk(KERN_INFO "\n HCSR is closing %s\n", HCSR_devp->miscdev.name);

	return 0;
} 

int threadfun(void* ptr)
{
	int i;

	struct HCSR_dev *HCSR_devp  = ptr;

	printk("Inside thread function %s \n",HCSR_devp->miscdev.name);

	for(i=0;i<((HCSR_devp->param1).samples)+2;i++){
		gpio_set_value_cansleep(pinMappings[(HCSR_devp->sensor1).io1].gpioPin1, 0); //sending a trigger pulse
		gpio_set_value_cansleep(pinMappings[(HCSR_devp->sensor1).io1].gpioPin1, 1); //sending a trigger pulse
		udelay(20);
		//msleep(1000);
		gpio_set_value_cansleep(pinMappings[(HCSR_devp->sensor1).io1].gpioPin1, 0); // disabling the trigger pulse
		//msleep(200);
		msleep(HCSR_devp->delay);
	}

	return 0;
}

/*
 * Write to HCSR driver
 */
ssize_t HCSR_driver_write(struct file *file, const char *buf,
           size_t count, loff_t *ppos)
{
	struct HCSR_dev *HCSR_devp = container_of(file->private_data, struct HCSR_dev, miscdev);
	int  data, ret;	

	printk("Inside Write function %s \n",HCSR_devp->miscdev.name);

	if(HCSR_devp->ongoingMeasurement==1){
		printk("on going measurement in write %s\n",HCSR_devp->miscdev.name);
		return-EINVAL;
	}

	//buffer is cleared when an input integer has non zero value
	/*if(buf!=0){
		for(j=0;j<5;j++){
			HCSR_devp->buf[j].buf_array=0;
		}
	}*/
 
 //clears the buffer on writing 1	
 ret = copy_from_user(&data, buf, sizeof(int));
 if(data!=0){

	memset(HCSR_devp->buf,0,sizeof(HCSR_devp->buf));

	HCSR_devp->buf_length=0;
	HCSR_devp->buf_index_read=0;
	HCSR_devp->buf_index_write=0;
}
	HCSR_devp->ongoingMeasurement = 1;
	kthread_run(&threadfun,(void*)HCSR_devp,"kthreadfunction");
	
	return 0;
}


/*
 * Read to HCSR driver
 */
ssize_t HCSR_driver_read(struct file *file, char *buf,
           size_t count, loff_t *ppos)
{
	int ret;
	struct HCSR_dev *HCSR_devp = container_of(file->private_data, struct HCSR_dev, miscdev);

	printk("Inside Read function %s \n",HCSR_devp->miscdev.name);

//when buffer is empty
if(HCSR_devp->buf_length==0) 
{
	if(HCSR_devp->ongoingMeasurement==1){
		printk("on going measurement in read %s\n",HCSR_devp->miscdev.name);
	}
	else{
		printk("no on going measurements in read %s\n",HCSR_devp->miscdev.name);
		HCSR_devp->ongoingMeasurement=1;
		kthread_run(&threadfun,(void*)HCSR_devp,"kthreadfunction");
	}
	printk("semcount before down %d %s\n",HCSR_devp->sem.count,HCSR_devp->miscdev.name);
	ret=down_interruptible(&HCSR_devp->sem);
	printk("semcount after down %d %d %s\n",ret,HCSR_devp->sem.count,HCSR_devp->miscdev.name);
}

	printk(KERN_INFO"Buffer index read %d %s",HCSR_devp->buf_index_read,HCSR_devp->miscdev.name);

	if (copy_to_user(buf,&HCSR_devp->buf[HCSR_devp->buf_index_read],sizeof(struct buffer)))
		{
			printk("Can not copy to user %s",HCSR_devp->miscdev.name);
			return -EACCES;
		} //buffer values transfer to user

	HCSR_devp->buf_index_read++;

	if(HCSR_devp->buf_length>0){	
		HCSR_devp->buf_length--;
	}

	if(HCSR_devp->buf_index_read > 4)
	{
		HCSR_devp->buf_index_read = 0;
	}

	return 0;
}

	//Test distance = (pulse width * ultrasonic spreading velocity in air) / 2
	//Referring the data sheet , we get that the distance is (pulse_width* velocity of air)/2
	//pulse width here is difference in system ticks , generated at start time and stop time.
	//1 tick = 1/400MHz(Frequency of Intel Galileo board)
	//distance = (diff in ticks)* 340 (m/sec) / (2*400M)
	// Therefore distance = diff_in_time_ticks * 340/8000000

irqreturn_t hcsrgpio_irq_handler(int irq, void *dev_id){

//irq_handler_t hcsrgpio_irq_handler(unsigned int irq, void *dev_id, struct pt_regs *regs){

	//int value, flag;
	//int j=0, k=0, l=0, m=0, min, max;
	struct HCSR_dev *HCSR_devp = dev_id;
	//uint64_t distance;

	//printk(KERN_ALERT "Inside handle r\n");
	//printk(KERN_ALERT"interrupt received (irq: %d)\n", irq);

	//flag = 0;
	//value = gpio_get_value_cansleep(pinMappings[(HCSR_devp->sensor1).io2].gpioPin1);
	//printk("Echo pin value %d \n", value);

//	if(irq==gpio_to_irq(pinMappings[(HCSR_devp->sensor1).io2].gpioPin1)){
		
	//if(value==0){
	if(HCSR_devp->flag==0){
		HCSR_devp->start_time = tsc();
		HCSR_devp->flag=1;
		irq_set_irq_type(irq, IRQ_TYPE_EDGE_FALLING);
		//flag=0;
		//printk("Echo pin is low\n");	
	}
	else
	{
		HCSR_devp->stop_time = tsc();
		HCSR_devp->flag=0;
		irq_set_irq_type(irq, IRQ_TYPE_EDGE_RISING);
		schedule_work(&HCSR_devp->work);
		printk("Echo pin is high %s\n",HCSR_devp->miscdev.name);
		//flag=1;
	}

	//return (irq_handler_t) IRQ_HANDLED;      // Announce that the IRQ has been handled correctly
	return IRQ_HANDLED;
}

//Worker function to calculate distance
void work_fun(struct work_struct *work){

		int j=0, k=0, l=0, m=0, n=0;
		
		struct HCSR_dev *HCSR_devp = container_of(work, struct HCSR_dev, work);
		uint64_t distance, min, max, Average, sum=0;

		distance=div_u64(((HCSR_devp->stop_time)-(HCSR_devp->start_time))*340,8000000);	

		//if(distance>=2 && distance<=400){

		printk("Distance observed %llu %llu %llu (cm) %s\n",HCSR_devp->start_time,HCSR_devp->stop_time,distance,HCSR_devp->miscdev.name);
		//HCSR_devp->stop_time=0;
		//HCSR_devp->start_time=0;

		//printk("index %d samples %d \n",HCSR_devp->index,((HCSR_devp->param1).samples)+2);

		if((HCSR_devp->index)<((HCSR_devp->param1).samples+2)){ 
			HCSR_devp->array[(HCSR_devp->index)++]=distance;
			//printk(KERN_INFO "array values %llu at index %d \n" , HCSR_devp->array[(HCSR_devp->index)], HCSR_devp->index);
		} //putting all m+2 values in a buffer

	//}
		if((HCSR_devp->index)==((HCSR_devp->param1).samples)+2){
		HCSR_devp->index = 0;

		//finding the min value
		min=HCSR_devp->array[0];
		for(j=0;j<((HCSR_devp->param1).samples)+2;j++){
			printk(KERN_INFO "array values %llu %s\n" , HCSR_devp->array[j],HCSR_devp->miscdev.name);
			if(HCSR_devp->array[j]<min){
				min=HCSR_devp->array[j];
				m=j;
			}
		}
		printk(KERN_INFO "min value %llu %s \n",min,HCSR_devp->miscdev.name);

		//finding the max value
		max=HCSR_devp->array[0];
		for(k=0;k<((HCSR_devp->param1).samples)+2;k++){
			if(HCSR_devp->array[k]>max){
				max=HCSR_devp->array[k];
				l=k;
			}
		}
		HCSR_devp->array[m]=0;
		HCSR_devp->array[l]=0;
		printk(KERN_INFO "max value %llu %s \n",max,HCSR_devp->miscdev.name);

		for(n=0;n<((HCSR_devp->param1).samples)+2;n++){
			sum += HCSR_devp->array[n];
		}
		printk(KERN_INFO "Sum %llu %s\n", sum,HCSR_devp->miscdev.name);
		Average=div_u64(sum,5);
		printk(KERN_INFO "Average %llu %s\n", Average,HCSR_devp->miscdev.name);

		//putting values in a buffer
		mutex_lock(&HCSR_devp->buff_mutex);
		
		if(HCSR_devp->buf_index_write>4){
			HCSR_devp->buf_index_write = 0;
		}


		HCSR_devp->buf[HCSR_devp->buf_index_write].buf_array = Average;   //saving buffer values
		HCSR_devp->buf[HCSR_devp->buf_index_write++].time_stamp = tsc(); //saving time stamp

		//check for buffer length
		if(HCSR_devp->buf_length<=4){
		HCSR_devp->buf_length++;
		}

		HCSR_devp->ongoingMeasurement=0;

		for(j=0;j<5;j++){
			printk(KERN_ALERT"buffer values %llu %s\n" ,HCSR_devp->buf[j].buf_array,HCSR_devp->miscdev.name);
			printk(KERN_ALERT"time_stamp %llu %s\n", HCSR_devp->buf[j].time_stamp,HCSR_devp->miscdev.name);
			}
		mutex_unlock(&HCSR_devp->buff_mutex);

		printk("semcount before up %d %s\n",HCSR_devp->sem.count,HCSR_devp->miscdev.name);

		//releasing the semaphore
		up(&HCSR_devp->sem);
		printk("semcount after up%d %s\n",HCSR_devp->sem.count,HCSR_devp->miscdev.name);
	} 

}

//ioctl to HCSR driver
long HCSR_driver_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	struct HCSR_dev *HCSR_devp = container_of(file->private_data, struct HCSR_dev, miscdev);
	unsigned int irqNumber;
	int result;

	printk("Inside IOCTL function %s\n",HCSR_devp->miscdev.name);

	if(HCSR_devp->ongoingMeasurement==1){
		printk(KERN_INFO "On going measurement %s\n",HCSR_devp->miscdev.name);
		return-EINVAL;
	}

	switch(cmd){

		case CONFIG_PINS:
			printk("Inside config pins %s\n",HCSR_devp->miscdev.name);

			if (copy_from_user(&(HCSR_devp->sensor1), (struct sensor*)arg, sizeof(struct sensor)))
			{
				return -EACCES;
			}

			if((HCSR_devp->sensor1).io1<0 || (HCSR_devp->sensor1).io1 >20)
			{
				printk(KERN_INFO "\n Invalid trigger pin %s\n",HCSR_devp->miscdev.name);
				return-EINVAL;
			}

			if((HCSR_devp->sensor1).io2==7 || (HCSR_devp->sensor1).io2 ==8)
			{
				printk(KERN_INFO "\n Invalid echo pin %s\n",HCSR_devp->miscdev.name);
				return-EINVAL;
			}

			printk(KERN_INFO "\n IO Pin for trigger is Initialised with value of = %d %s\n",(HCSR_devp->sensor1).io1,HCSR_devp->miscdev.name);
			printk(KERN_INFO "\n IO Pin for echo is Initialised with value of = %d %s\n", (HCSR_devp->sensor1).io2,HCSR_devp->miscdev.name);

			configuringPins(HCSR_devp);
			

			//interrupt request on GPIO
			irqNumber = gpio_to_irq(pinMappings[(HCSR_devp->sensor1).io2].gpioPin1);
			printk(KERN_INFO "\n irqNumber %d %s\n", irqNumber,HCSR_devp->miscdev.name);  //returns irq number associated with echo pin

				if(irqNumber < 0)
				{
					printk("GPIO to irq mapping failed %s",HCSR_devp->miscdev.name);
					return 0;
				}
	//		result = request_irq(irqNumber,(irq_handler_t) hcsrgpio_irq_handler,IRQF_TRIGGER_RISING,"HCSR_gpio_handler",(void*)HCSR_devp);
			result = request_irq(irqNumber,hcsrgpio_irq_handler,IRQF_TRIGGER_RISING/*|IRQF_TRIGGER_FALLING*/,"HCSR_gpio_handler",(void*)HCSR_devp);
			printk(KERN_INFO "GPIO_TEST: The interrupt request result: %d %s\n", result,HCSR_devp->miscdev.name);

			INIT_WORK(&HCSR_devp->work,work_fun); //worker thread

			break;
			

		case SET_PARAMETERS:
			printk("\n inside set parameters %s",HCSR_devp->miscdev.name);

			if (copy_from_user(&(HCSR_devp->param1), (struct sensor*)arg, sizeof(struct param)))
			{
				return -EACCES;
			}

			if((HCSR_devp->param1).samples < 0)
			{
				printk(KERN_INFO "\n Invalid samples entered %s\n",HCSR_devp->miscdev.name);
				return-EINVAL;
			}

			if((HCSR_devp->param1).period < 0)
			{
				printk(KERN_INFO "\n Invalid period %s\n",HCSR_devp->miscdev.name);
				return-EINVAL;
			}
			
			HCSR_devp->delay = div_u64(((HCSR_devp->param1).period),((HCSR_devp->param1).samples)+2);

			if((HCSR_devp->delay) < 60){
				printk(KERN_INFO "\n too less time to process %s \n",HCSR_devp->miscdev.name);
				return-EINVAL;
			}

			HCSR_devp->array = (uint64_t*) kzalloc((((HCSR_devp->param1).samples)+2)* (sizeof (uint64_t)), GFP_KERNEL);

			printk(KERN_INFO "\n Number of samples per measurement m= %d %s\n",(HCSR_devp->param1).samples,HCSR_devp->miscdev.name);
			printk(KERN_INFO "\n sampling period delta= %d %s\n", (HCSR_devp->param1).period,HCSR_devp->miscdev.name);
			printk(KERN_INFO "\n delay = %d \n %s", HCSR_devp->delay,HCSR_devp->miscdev.name);

			//initialize mutex
			mutex_init(&HCSR_devp->buff_mutex);

			//initialize semaphore
			sema_init(&HCSR_devp->sem,0);
			printk("semcount init %d \n",HCSR_devp->sem.count);

			break;
	}

	printk("\n Exiting IOCTL \n");
	return 0;

}

/**
 * Pin Matrix data.
 */
void initialisePinData() {

	pinMappings[0].gpioPin1 = 11;
	pinMappings[0].gpioPin2 = 32;
	pinMappings[0].gpioPin3 = 81;
	pinMappings[0].gpioPin4 = 81;

	pinMappings[1].gpioPin1 = 12;
	pinMappings[1].gpioPin2 = 28;
	pinMappings[1].gpioPin3 = 45;
	pinMappings[1].gpioPin4 = 81;

	pinMappings[2].gpioPin1 = 13;
	pinMappings[2].gpioPin2 = 34;
	pinMappings[2].gpioPin3 = 77;
	pinMappings[2].gpioPin4 = 81;

	pinMappings[3].gpioPin1 = 14;
	pinMappings[3].gpioPin2 = 16;
	pinMappings[3].gpioPin3 = 76;
	pinMappings[3].gpioPin4 = 64;

	pinMappings[4].gpioPin1 = 6;
	pinMappings[4].gpioPin2 = 36;
	pinMappings[4].gpioPin3 = 81;
	pinMappings[4].gpioPin4 = 81;

	pinMappings[5].gpioPin1 = 0;
	pinMappings[5].gpioPin2 = 18;
	pinMappings[5].gpioPin3 = 66;
	pinMappings[5].gpioPin4 = 81;

	pinMappings[6].gpioPin1 = 1;
	pinMappings[6].gpioPin2 = 20;
	pinMappings[6].gpioPin3 = 68;
	pinMappings[6].gpioPin4 = 81;

	pinMappings[7].gpioPin1 = 38;
	pinMappings[7].gpioPin2 = 81;
	pinMappings[7].gpioPin3 = 81;
	pinMappings[7].gpioPin4 = 81;

	pinMappings[8].gpioPin1 = 40;
	pinMappings[8].gpioPin2 = 81;
	pinMappings[8].gpioPin3 = 81;
	pinMappings[8].gpioPin4 = 81;

	pinMappings[9].gpioPin1 = 4;
	pinMappings[9].gpioPin2 = 22;
	pinMappings[9].gpioPin3 = 70;
	pinMappings[9].gpioPin4 = 81;

	pinMappings[10].gpioPin1 = 10;
	pinMappings[10].gpioPin2 = 26;
	pinMappings[10].gpioPin3 = 74;
	pinMappings[10].gpioPin4 = 81;

	pinMappings[11].gpioPin1 = 5;
	pinMappings[11].gpioPin2 = 24;
	pinMappings[11].gpioPin3 = 44;
	pinMappings[11].gpioPin4 = 72;

	pinMappings[12].gpioPin1 = 15;
	pinMappings[12].gpioPin2 = 42;
	pinMappings[12].gpioPin3 = 81;
	pinMappings[12].gpioPin4 = 81;

	pinMappings[13].gpioPin1 = 7;
	pinMappings[13].gpioPin2 = 30;
	pinMappings[13].gpioPin3 = 46;
	pinMappings[13].gpioPin4 = 81;

	pinMappings[14].gpioPin1 = 48;
	pinMappings[14].gpioPin2 = 81;
	pinMappings[14].gpioPin3 = 81;
	pinMappings[14].gpioPin4 = 81;

	pinMappings[15].gpioPin1 = 50;
	pinMappings[15].gpioPin2 = 81;
	pinMappings[15].gpioPin3 = 81;
	pinMappings[15].gpioPin4 = 81;

	pinMappings[16].gpioPin1 = 52;
	pinMappings[16].gpioPin2 = 81;
	pinMappings[16].gpioPin3 = 81;
	pinMappings[16].gpioPin4 = 81;

	pinMappings[17].gpioPin1 = 54;
	pinMappings[17].gpioPin2 = 81;
	pinMappings[17].gpioPin3 = 81;
	pinMappings[17].gpioPin4 = 81;	

	pinMappings[18].gpioPin1 = 56;
	pinMappings[18].gpioPin2 = 81;
	pinMappings[18].gpioPin3 = 60;
	pinMappings[18].gpioPin4 = 78;

	pinMappings[19].gpioPin1 = 58;
	pinMappings[19].gpioPin2 = 81;
	pinMappings[19].gpioPin3 = 60;
	pinMappings[19].gpioPin4 = 79;

}

/**
 * Configuring pins for switching action.
 */
void configuringPins(struct HCSR_dev *HCSR_devp) {
	
	//Trigger

	if (pinMappings[(HCSR_devp->sensor1).io1].gpioPin1 != 81) {
		printk("\n GPIO Request command :: Trigger:: gpio "
				"%d %s \n" , pinMappings[(HCSR_devp->sensor1).io1].gpioPin1,HCSR_devp->miscdev.name);
		gpio_request(pinMappings[(HCSR_devp->sensor1).io1].gpioPin1, "Trigger");
		gpio_direction_output(pinMappings[(HCSR_devp->sensor1).io1].gpioPin1, 1); //output (direction), value = 1

	}
	if (pinMappings[(HCSR_devp->sensor1).io1].gpioPin2 != 81) {
		printk("\n GPIO Request command :: Trigger :: gpio"
				"%d %s\n" , pinMappings[(HCSR_devp->sensor1).io1].gpioPin2,HCSR_devp->miscdev.name);
		gpio_request(pinMappings[(HCSR_devp->sensor1).io1].gpioPin2, "Trigger");
		gpio_direction_output(pinMappings[(HCSR_devp->sensor1).io1].gpioPin2, 0); //direction
	}
	if (pinMappings[(HCSR_devp->sensor1).io1].gpioPin3 != 81) {
		printk("\n GPIO Request command :: Trigger :: gpio "
				"%d %s\n" , pinMappings[(HCSR_devp->sensor1).io1].gpioPin3,HCSR_devp->miscdev.name);
		gpio_request(pinMappings[(HCSR_devp->sensor1).io1].gpioPin3, "Trigger");

		if ((pinMappings[(HCSR_devp->sensor1).io1].gpioPin3 < 64) && (pinMappings[(HCSR_devp->sensor1).io1].gpioPin3 != 60)){
		gpio_direction_output(pinMappings[(HCSR_devp->sensor1).io1].gpioPin3, 0); //direction & output value
	}else{
		gpio_set_value_cansleep(pinMappings[(HCSR_devp->sensor1).io1].gpioPin3,0); //value
	}

		if (pinMappings[(HCSR_devp->sensor1).io1].gpioPin3 == 60) {
		gpio_direction_output(pinMappings[(HCSR_devp->sensor1).io1].gpioPin3,1); //value - setting mux pin high for gpio 60
		}

	}

	if (pinMappings[(HCSR_devp->sensor1).io1].gpioPin4  != 81) {
		printk("\n GPIO Request command :: Trigger :: gpio "
				"%d %s\n" , pinMappings[(HCSR_devp->sensor1).io1].gpioPin4,HCSR_devp->miscdev.name);
		gpio_request(pinMappings[(HCSR_devp->sensor1).io1].gpioPin4, "Trigger");

	if ((pinMappings[(HCSR_devp->sensor1).io1].gpioPin4  < 64) && (pinMappings[(HCSR_devp->sensor1).io1].gpioPin4 != 78) && (pinMappings[(HCSR_devp->sensor1).io1].gpioPin4 != 79)) {
		gpio_direction_output(pinMappings[(HCSR_devp->sensor1).io1].gpioPin4, 0); //direction & value (low)
		}
	else{
		gpio_set_value_cansleep(pinMappings[(HCSR_devp->sensor1).io1].gpioPin4,0); //value
		}
			
	if (pinMappings[(HCSR_devp->sensor1).io1].gpioPin4 == 78) {
		gpio_set_value_cansleep(pinMappings[(HCSR_devp->sensor1).io1].gpioPin4,1); //value - setting mux pin high for gpio 78 and 79
		}
	
	if (pinMappings[(HCSR_devp->sensor1).io1].gpioPin4 == 79) {
		gpio_set_value_cansleep(pinMappings[(HCSR_devp->sensor1).io1].gpioPin4,1); //value - setting mux pin high for gpio 78 and 79
		}
	
	}

	//Echo

	if (pinMappings[(HCSR_devp->sensor1).io2].gpioPin1 != 81) {
		printk("\n GPIO Request command :: Echo :: gpio"
				"%d %s\n" , pinMappings[(HCSR_devp->sensor1).io2].gpioPin1,HCSR_devp->miscdev.name);
		gpio_request(pinMappings[(HCSR_devp->sensor1).io2].gpioPin1, "Echo");
		//gpio_direction_output(pinMappings[(HCSR_devp->sensor1).io2].gpioPin1, 0); //input (direction), value default 0
		gpio_direction_input(pinMappings[(HCSR_devp->sensor1).io2].gpioPin1);
	}
	if (pinMappings[(HCSR_devp->sensor1).io2].gpioPin2 != 81) {
		printk("\n GPIO Request command :: Echo :: gpio"
				"%d %s\n" , pinMappings[(HCSR_devp->sensor1).io2].gpioPin2,HCSR_devp->miscdev.name);
		gpio_request(pinMappings[(HCSR_devp->sensor1).io2].gpioPin2, "Echo");
		gpio_direction_output(pinMappings[(HCSR_devp->sensor1).io2].gpioPin2, 1); //direction
		//gpio_set_value_cansleep(pinMappings[(HCSR_devp->sensor1).io2].gpioPin2,1); //value
	}
	if (pinMappings[(HCSR_devp->sensor1).io2].gpioPin3 != 81) {
		printk("\n GPIO Request command :: Echo :: gpio"
				"%d %s\n" , pinMappings[(HCSR_devp->sensor1).io2].gpioPin3,HCSR_devp->miscdev.name);
		gpio_request(pinMappings[(HCSR_devp->sensor1).io2].gpioPin3, "Echo"); 

		if(pinMappings[(HCSR_devp->sensor1).io2].gpioPin3 < 64){
		gpio_direction_output(pinMappings[(HCSR_devp->sensor1).io2].gpioPin3, 0); //direction
	}else{
		gpio_set_value_cansleep(pinMappings[(HCSR_devp->sensor1).io2].gpioPin3,0); //value
	}

	if (pinMappings[(HCSR_devp->sensor1).io2].gpioPin3 == 60) {
		gpio_set_value_cansleep(pinMappings[(HCSR_devp->sensor1).io2].gpioPin3,1); //value - setting mux pin high for gpio 60
		}
	}

	if (pinMappings[(HCSR_devp->sensor1).io2].gpioPin4 != 81) {
		printk("\n GPIO Request command :: Echo :: gpio"
				"%d %s\n" , pinMappings[(HCSR_devp->sensor1).io2].gpioPin4,HCSR_devp->miscdev.name);
		gpio_request(pinMappings[(HCSR_devp->sensor1).io2].gpioPin4, "Echo");

	if((pinMappings[(HCSR_devp->sensor1).io2].gpioPin4 < 64) && (pinMappings[(HCSR_devp->sensor1).io2].gpioPin4 != 78) && (pinMappings[(HCSR_devp->sensor1).io2].gpioPin4 != 79)){
		gpio_direction_output(pinMappings[(HCSR_devp->sensor1).io2].gpioPin4, 0); //direction
	
	}
	else{
		gpio_set_value_cansleep(pinMappings[(HCSR_devp->sensor1).io2].gpioPin4,0); //value
	}

	if(pinMappings[(HCSR_devp->sensor1).io2].gpioPin4 == 78) {
		gpio_set_value_cansleep(pinMappings[(HCSR_devp->sensor1).io2].gpioPin4,1); //value - setting mux pin high for gpio 78 and 79
		}
	if(pinMappings[(HCSR_devp->sensor1).io2].gpioPin4 == 79) {
		gpio_set_value_cansleep(pinMappings[(HCSR_devp->sensor1).io2].gpioPin4,1); //value - setting mux pin high for gpio 78 and 79
		}
	
	}

	printk("trigger %d %d %d %d %s\n \n" ,gpio_get_value_cansleep(pinMappings[(HCSR_devp->sensor1).io1].gpioPin1)
								 ,gpio_get_value_cansleep(pinMappings[(HCSR_devp->sensor1).io1].gpioPin2)
								 ,gpio_get_value_cansleep(pinMappings[(HCSR_devp->sensor1).io1].gpioPin3)
								 ,gpio_get_value_cansleep(pinMappings[(HCSR_devp->sensor1).io1].gpioPin4),HCSR_devp->miscdev.name);

	printk("echo %d %d %d %d %s\n \n" ,gpio_get_value_cansleep(pinMappings[(HCSR_devp->sensor1).io2].gpioPin1)
								 ,gpio_get_value_cansleep(pinMappings[(HCSR_devp->sensor1).io2].gpioPin2)
								 ,gpio_get_value_cansleep(pinMappings[(HCSR_devp->sensor1).io2].gpioPin3)
								 ,gpio_get_value_cansleep(pinMappings[(HCSR_devp->sensor1).io2].gpioPin4),HCSR_devp->miscdev.name);
}

/* File operations structure. Defined in linux/fs.h */
static struct file_operations HCSR_fops = {
    .owner			= THIS_MODULE,         /* Owner */
    .open			= HCSR_driver_open,    /* Open method */
    .release		= HCSR_driver_release, /* Release method */
    .write			= HCSR_driver_write,   /* Write method */
    .read			= HCSR_driver_read,    /* Read method */
    .unlocked_ioctl	= HCSR_driver_ioctl   /*IOCTL method */
};

/*
 * Driver Initialization
 */
static int __init HCSR_driver_init(void)
{
	int i, ret;

	printk(KERN_INFO "Initialization \n");

	initialisePinData();

	/* Allocate memory for the per-device structure */
	HCSR_devp = kzalloc(sizeof(struct HCSR_dev)*Number_Of_Devices, GFP_KERNEL);

	if (!HCSR_devp) {
		printk("Bad Kmalloc \n"); 
		return -ENOMEM;
	}
	
	for(i=0;i<Number_Of_Devices;i++){

		//Register miscellaneous device for each per device structure

		char* name = (char*) kzalloc(10* (sizeof (char)), GFP_KERNEL);
		HCSR_devp[i].miscdev.minor = MISC_DYNAMIC_MINOR;
		sprintf(name,"hcsr%d",i);
		HCSR_devp[i].miscdev.name = name;
		HCSR_devp[i].miscdev.fops = &HCSR_fops;

		ret = misc_register(&(HCSR_devp[i].miscdev));

	    if (ret) {
			printk("Bad miscellaneous device\n");
			return ret;
			}
	}

    
	printk("HCSR driver initialized.\n\n");

	return 0;
}

/* Driver Exit */
static void __exit HCSR_driver_exit(void)
{
	int i;
	printk(KERN_INFO "Exit the driver");

	for(i=0;i<Number_Of_Devices;i++){

	//Deregister miscellaneous device for each per device structure

	misc_deregister(&(HCSR_devp[i].miscdev));
	
	}

	kfree(HCSR_devp);
	printk("HCSR driver removed.\n");
}

module_init(HCSR_driver_init);
module_exit(HCSR_driver_exit);
MODULE_LICENSE("GPL v2");