/*
 * Copyright (C) Your copyright.
 *
 * Author: Khyati
 *
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <pthread.h>
#include <poll.h>
#include <sys/ioctl.h>
#include <pthread.h>
#include <unistd.h>
#include <stdint.h>

#define IOC_MAGIC1 'k' // defines the magic number
#define IOC_MAGIC2 'a' // defines the magic number
#define SEQ_NR_LOCAL '0' // Sequence number local to the driver.

#define CONFIG_PINS _IOW(IOC_MAGIC1, SEQ_NR_LOCAL, struct sensor) //Defines IOCTL Command.
#define SET_PARAMETERS _IOW(IOC_MAGIC2, SEQ_NR_LOCAL, struct param) //Defines IOCTL Command.

int Number_Of_Devices;

struct sensor {
	int io1; //trigger
	int io2; //echo 
};

struct param {
	int samples; //number of samples per measurement
	int period;	//sampling period
};

struct buffer{
	uint64_t buf_array;    //buffer sized array
	uint64_t time_stamp;   //time stamps
};

struct HCSR_data{
	struct sensor sensor1; //sensor structure
	struct param param1;   //parameter structure
	int dev_id; 		   //which HCSR	
	struct buffer buf;  //buffer structure
};

void* myThreadFun(void* args){

		int fd, ret1, ret2, ret3, ret4, ret5, buf=0;
		int buf1=1;

		char* name = (char*) malloc(15*(sizeof (char)));

		struct HCSR_data* data = args;

		memset(name,'\0',15);
		sprintf(name,"/dev/hcsr%d",data->dev_id);

		/*Opening the device*/
		fd = open(name,O_RDWR);

			if (fd < 0 ){
				printf("Can not open device  hcsr%d file\n",data->dev_id);		
				return 0;
			}
			else {
				printf("hcsr%d device is opened\n",data->dev_id);
			}

		ret1 = ioctl(fd,CONFIG_PINS,&data->sensor1);

			if(ret1 == -1){
			printf("ioctl 1 failed \n");
			}

		ret2 = ioctl(fd,SET_PARAMETERS,&data->param1);

			if(ret2 == -1){
			printf("ioctl 2 failed \n");
			}


		//writing 1 to the buffer
		ret3= write(fd,&buf1,sizeof(int));

			if(ret3 == -1){
			printf("Write failed \n");
			}

		sleep(2);

		//writing to the buffer
		ret4= write(fd,&buf,sizeof(int));

		if(ret4 == -1){
		printf("Write failed \n");
		}

		sleep(2);

/*		write(fd,&buf,sizeof(int));
		write(fd,&buf,sizeof(int));
		write(fd,&buf,sizeof(int));*/

		//reading the buffer
		ret5= read(fd,&data->buf,sizeof(struct buffer));

		if(ret5 == -1){
		printf("Read failed \n");
		}

		sleep(2);

		printf("distance is %llu (cm) time stamp is %llu for HCSR%d\n", data->buf.buf_array, data->buf.time_stamp,data->dev_id);
	
/*		read(fd,&data->buf,sizeof(struct buffer));

		sleep(2);

		printf("distance is %llu (cm) time stamp is %llu for HCSR%d\n", data->buf.buf_array, data->buf.time_stamp,data->dev_id);
*/
	close(fd);
	return 0;
}


int main(int argc, char *argv[])
{
	int i;

	Number_Of_Devices=atoi(argv[1]);

	pthread_t tid[Number_Of_Devices];

	struct HCSR_data data[Number_Of_Devices];

	//creating multiple pthreads
	for(i=0;i<Number_Of_Devices;i++){

		printf("Enter IO pin for trigger hcsr%d \n" ,i);
		scanf("%d",&(data[i].sensor1.io1));
		printf("Enter IO pin for echo hcsr%d \n",i);
		scanf("%d",&(data[i].sensor1.io2));
		printf("Enter the number of samples per measurement hcsr%d \n",i);
		scanf("%d",&(data[i].param1.samples));
		printf("Enter the sampling period hcsr%d \n",i);
		scanf("%d",&(data[i].param1.period));	
		
		data[i].dev_id=i;
	}

	for(i=0;i<Number_Of_Devices;i++){
		pthread_create(&tid[i], NULL, myThreadFun, &data[i]);		
	}

	//joining multiple pthreads
	for(i=0;i<Number_Of_Devices;i++){
		pthread_join(tid[i], NULL);
	}	

	return 0;
}
