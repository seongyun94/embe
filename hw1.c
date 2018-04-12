#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/stat.h>
#include <sys/select.h>
#include <sys/time.h>
#include <sys/mman.h>
#include <errno.h>
#include <fcntl.h>
#include <dirent.h>
#include <linux/input.h>
#include <termios.h>
#include <signal.h>
#include <time.h>

#define BUFF_SIZE 64#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/stat.h>
#include <sys/select.h>
#include <sys/time.h>
#include <sys/mman.h>
#include <errno.h>
#include <fcntl.h>
#include <dirent.h>
#include <linux/input.h>
#include <termios.h>
#include <signal.h>
#include <time.h>

#define BUFF_SIZE 64

#define KEY_RELEASE 0
#define KEY_PRESS 1
#define MAX_BUTTON 9
#define FPGA_BASE_ADDRESS 0x08000000    //fpga_base address
#define LED_ADDR 0x16

int main() {

    pid_t input_pid, output_pid;
    int shmid;
    int* shmaddr;
    key_t key;

    time_t current_time;
    time_t total_time, current_hour, current_min;

    key = 1530;
    shmid = shmget(key, 1024, IPC_CREAT|0666);
    if(shmid == -1){
        perror("shmget");
        return -1;
    }

    input_pid = fork();
    if(input_pid < 0){
        printf("fork failed\n");
        return -1;
    }

    /**********************************************/
    /*************** input process ****************/
    /**********************************************/

    else if(input_pid == 0){
        struct input_event ev[BUFF_SIZE];
        int fd, rd, value, code, size = sizeof(struct input_event);
        char name[256] = "Unknown";
             
        int i;
        int dev;
        int buff_size;
        unsigned char push_sw_buff[MAX_BUTTON];

        int mode_num=0;

        char* device = "/dev/input/event0";
        if((fd = open(device, O_RDONLY|O_NONBLOCK)) == -1) {    //for read nonblocking
            printf("%s is not a valid device.n", device);
        }
        shmaddr = shmat(shmid, (void *)0, 0);
        
        dev = open("/dev/fpga_push_switch", O_RDWR);

        if(dev<0){
            printf("Device Open Error\n");
            close(dev);
            return -1;
        }

        buff_size = sizeof(push_sw_buff);

        while(1){
            read(dev, &push_sw_buff, buff_size);
            for(i=0; i<MAX_BUTTON; i++){
                shmaddr[i] = push_sw_buff[i];
                //printf("[%d] ",push_sw_buff[i]);
            }
            // if press -> shmaddr[i]=1
            //    release -> shmaddr[i]=0
            /*
            for(i=0; i<MAX_BUTTON; i++)
                printf("%d=%d ",i,shmaddr[i]);
            */
            //printf("\n");
            
            if((rd = read(fd, ev, size * BUFF_SIZE)) >= size){  //for read nonblocking
                value = ev[0].value;
                code = ev[0].code;

                if(value != ' ' && ev[1].value == 1 && ev[1].type == 1){    //Only read the key press event
                    printf("code%d\n", (ev[1].code));
                }
                if(value == KEY_RELEASE){
                    printf("key release\n");
                }
                else if(value == KEY_PRESS){
                    printf("key press\n");
                    // each case of mode_num
                    // 0 : mode 1
                    // 1 : mode 2
                    // 2 : mode 3
                    // 3 : mode 4

                    // when press 'back', terminate program
                    if(code == 158){
                        //shmaddr[9] = mode_num;
                        /* the number '-1' in  index 10 means i will terminate program */
                        shmaddr[10] = -1;
                        shmdt(shmaddr);
                        return 0;
                    }
                    // when press 'vol+', change mode
                    else if(code == 115){
                        mode_num = (mode_num+1)%4;
                        shmaddr[9] = mode_num;
                        printf("%d\n", shmaddr[9]);
                    }
                    // when press 'vol-' change mode(reverse)
                    else if(code == 114){
                        if(mode_num - 1 >= 0)
                            mode_num -= 1;
                        else
                            mode_num += 3;
                        shmaddr[9] = mode_num;
                        printf("%d\n", shmaddr[9]);
                    }
                }
                printf("Type[%d] Value[%d] Code[%d]\n", ev[0].type, ev[0].value, (ev[0].code));
            }
        }
        close(dev);
    }
    else{
        output_pid = fork();
        if(output_pid < 0){
            printf("fork failed\n");
            return -1;
        }
        else if(output_pid == 0){  

            /******************************************************/
            /************* this area is output process ************/
            /******************************************************/

            //printf("output enter!!!!!!!\n");

            shmaddr = shmat(shmid, (void *)0, 0);
            int fnd_dev;
            int fd;

            unsigned char fnd_data[4];
            unsigned char retval;
            unsigned long *fpga_addr = 0;
            unsigned char *led_addr = 0;
            unsigned char led_data;

            int i;
            int str_size;
            
            memset(fnd_data, 0, sizeof(fnd_data));

            fnd_dev = open("/dev/fpga_fnd", O_RDWR);
            fd = open("/dev/mem", O_RDWR | O_SYNC);
          
            if(fnd_dev<0){
                printf("Device open error : /dev/fpga_fnd\n");
                exit(1);
            }
            if(fd<0){
                printf("/dev/mem open error");
                exit(1);
            }

            fpga_addr = (unsigned long *)mmap(NULL, 4096, PROT_READ | PROT_WRITE, MAP_SHARED, fd, FPGA_BASE_ADDRESS);
            if(fpga_addr == MAP_FAILED){
                printf("mmap error!\n");
                close(fd);
                exit(1);
            }

            led_addr = (unsigned char*)((void *)fpga_addr+LED_ADDR);

            while(1){
                //printf("output\n");
                if(shmaddr[9] == 0){    // Clock output
                    //printf("Im here!!!!!!!!!!!!1\n");

                    /*
                    led_data = atoi("128");

                    *led_addr = led_data;   //write led
                    led_data = *led_addr;
                    */

                    retval = write(fnd_dev, &fnd_data, 4);
                    // device file <- data address write
                    if(retval<0){
                        printf("Write Error!\n");
                        return -1;
                    }
                    
                    while(1){
                        fnd_data[0] = shmaddr[11];
                        fnd_data[1] = shmaddr[12];
                        fnd_data[2] = shmaddr[13];
                        fnd_data[3] = shmaddr[14];
                        *led_addr = shmaddr[15];
                        //printf("%d %d %d %d\n", shmaddr[11], shmaddr[12], shmaddr[13], shmaddr[14]);

                        retval = write(fnd_dev, &fnd_data, 4);
                        if(retval<0){
                            printf("Write Error!\n");
                            return -1;
                        }
                    }
                    //memset(fnd_data, 0, sizeof(data));
                    //retval = read(dev, );
                    close(fnd_dev);
                    munmap(led_addr, 4096);
                    close(fd);
                }

                else if(shmaddr[9] == 1){   // Counter output

                }
        
                else if(shmaddr[9] == 2){   // Text editor output

                }

                else if(shmaddr[9] == 3){   // Draw board output
                    
                }
            }
        }
        else{   
            /*****************************************************/
            /************* this area is main process *************/
            /*****************************************************/

            shmaddr = shmat(shmid, (void *)0, 0);
            while(1){
                //printf("main\n");
                if(shmaddr[9] == 0){    // Clock
                    int flag=0;
                    while(1){
                        if(flag == 0){  // clock initialize
                            time(&current_time);
                            total_time = current_time % 86400;
                            current_hour = total_time / 3600;
                            current_min = (total_time % 3600)/60;
                            //printf("current_time=%ld, total_time=%ld, hour=%ld, min=%ld\n", current_time, total_time, current_hour, current_min);
                            shmaddr[11] = current_hour / 10;
                            shmaddr[12] = current_hour % 10;
                            shmaddr[13] = current_min / 10;
                            shmaddr[14] = current_min % 10;
                            
                        }
                        else{
                            if(shmaddr[0] == 1){    // if press SW(1)
                                
                            }
                            else if(shmaddr[1] == 1){   // if press SW(2)

                            }
                            else if(shmaddr[2] == 1){   // if press SW(3)

                            }
                            else if(shmaddr[3] == 1){   // if press SW(4)

                            }
                        }
                    }
                    time(&current_time);
                    //printf("%ld\n", current_time);
                    //printf(ctime(&current_time));
                    
                    /*
                    total_time = current_time % 86400;
                    current_hour = total_time / 3600;
                    current_min = (total_time % 3600)/60;
                    //printf("current_time=%ld, total_time=%ld, hour=%ld, min=%ld\n", current_time, total_time, current_hour, current_min);
                    shmaddr[11] = current_hour / 10;
                    shmaddr[12] = current_hour % 10;
                    shmaddr[13] = current_min / 10;
                    shmaddr[14] = current_min % 10;
                    */


                }
            }
        }
    }

    return 0;
}


#define KEY_RELEASE 0
#define KEY_PRESS 1
#define MAX_BUTTON 9
#define FPGA_BASE_ADDRESS 0x08000000    //fpga_base address
#define LED_ADDR 0x16

int main() {

    pid_t input_pid, output_pid;
    int shmid;
    int* shmaddr;
    key_t key;

    time_t current_time;
    time_t total_time, current_hour, current_min;

    key = 1530;
    shmid = shmget(key, 1024, IPC_CREAT|0666);
    if(shmid == -1){
        perror("shmget");
        return -1;
    }

    input_pid = fork();
    if(input_pid < 0){
        printf("fork failed\n");
        return -1;
    }

    /**********************************************/
    /*************** input process ****************/
    /**********************************************/

    else if(input_pid == 0){
        struct input_event ev[BUFF_SIZE];
        int fd, rd, value, code, size = sizeof(struct input_event);
        char name[256] = "Unknown";
             
        int i;
        int dev;
        int buff_size;
        unsigned char push_sw_buff[MAX_BUTTON];

        int mode_num=0;

        char* device = "/dev/input/event0";
        if((fd = open(device, O_RDONLY|O_NONBLOCK)) == -1) {    //for read nonblocking
            printf("%s is not a valid device.n", device);
        }
        shmaddr = shmat(shmid, (void *)0, 0);
        
        dev = open("/dev/fpga_push_switch", O_RDWR);

        if(dev<0){
            printf("Device Open Error\n");
            close(dev);
            return -1;
        }

        buff_size = sizeof(push_sw_buff);

        while(1){
            read(dev, &push_sw_buff, buff_size);
            for(i=0; i<MAX_BUTTON; i++){
                shmaddr[i] = push_sw_buff[i];
                //printf("[%d] ",push_sw_buff[i]);
            }
            // if press -> shmaddr[i]=1
            //    release -> shmaddr[i]=0
            /*
            for(i=0; i<MAX_BUTTON; i++)
                printf("%d=%d ",i,shmaddr[i]);
            */
            //printf("\n");
            
            if((rd = read(fd, ev, size * BUFF_SIZE)) >= size){  //for read nonblocking
                value = ev[0].value;
                code = ev[0].code;

                if(value != ' ' && ev[1].value == 1 && ev[1].type == 1){    //Only read the key press event
                    printf("code%d\n", (ev[1].code));
                }
                if(value == KEY_RELEASE){
                    printf("key release\n");
                }
                else if(value == KEY_PRESS){
                    printf("key press\n");
                    // each case of mode_num
                    // 0 : mode 1
                    // 1 : mode 2
                    // 2 : mode 3
                    // 3 : mode 4

                    // when press 'back', terminate program
                    if(code == 158){
                        //shmaddr[9] = mode_num;
                        /* the number '-1' in  index 10 means i will terminate program */
                        shmaddr[10] = -1;
                        shmdt(shmaddr);
                        return 0;
                    }
                    // when press 'vol+', change mode
                    else if(code == 115){
                        mode_num = (mode_num+1)%4;
                        shmaddr[9] = mode_num;
                        printf("%d\n", shmaddr[9]);
                    }
                    // when press 'vol-' change mode(reverse)
                    else if(code == 114){
                        if(mode_num - 1 >= 0)
                            mode_num -= 1;
                        else
                            mode_num += 3;
                        shmaddr[9] = mode_num;
                        printf("%d\n", shmaddr[9]);
                    }
                }
                printf("Type[%d] Value[%d] Code[%d]\n", ev[0].type, ev[0].value, (ev[0].code));
            }
        }
        close(dev);
    }
    else{
        output_pid = fork();
        if(output_pid < 0){
            printf("fork failed\n");
            return -1;
        }
        else if(output_pid == 0){  

            /******************************************************/
            /************* this area is output process ************/
            /******************************************************/

            //printf("output enter!!!!!!!\n");

            shmaddr = shmat(shmid, (void *)0, 0);
            int fnd_dev;
            int fd;

            unsigned char fnd_data[4];
            unsigned char retval;
            unsigned long *fpga_addr = 0;
            unsigned char *led_addr = 0;
            unsigned char led_data;

            int i;
            int str_size;
            
            memset(fnd_data, 0, sizeof(fnd_data));

            fnd_dev = open("/dev/fpga_fnd", O_RDWR);
            fd = open("/dev/mem", O_RDWR | O_SYNC);
          
            if(fnd_dev<0){
                printf("Device open error : /dev/fpga_fnd\n");
                exit(1);
            }
            if(fd<0){
                printf("/dev/mem open error");
                exit(1);
            }

            fpga_addr = (unsigned long *)mmap(NULL, 4096, PROT_READ | PROT_WRITE, MAP_SHARED, fd, FPGA_BASE_ADDRESS);
            if(fpga_addr == MAP_FAILED){
                printf("mmap error!\n");
                close(fd);
                exit(1);
            }

            led_addr = (unsigned char*)((void *)fpga_addr+LED_ADDR);

            while(1){
                //printf("output\n");
                if(shmaddr[9] == 0){    // Clock output
                    //printf("Im here!!!!!!!!!!!!1\n");

                    /* led initialize */
                    led_data = atoi("128");

                    *led_addr = led_data;   //write led
                    led_data = *led_addr;

                    retval = write(fnd_dev, &fnd_data, 4);
                    // device file <- data address write
                    if(retval<0){
                        printf("Write Error!\n");
                        return -1;
                    }
                    
                    while(1){
                        fnd_data[0] = shmaddr[11];
                        fnd_data[1] = shmaddr[12];
                        fnd_data[2] = shmaddr[13];
                        fnd_data[3] = shmaddr[14];
                        
                        //printf("%d %d %d %d\n", shmaddr[11], shmaddr[12], shmaddr[13], shmaddr[14]);

                        retval = write(fnd_dev, &fnd_data, 4);
                        if(retval<0){
                            printf("Write Error!\n");
                            return -1;
                        }
                    }
                    //memset(fnd_data, 0, sizeof(data));
                    //retval = read(dev, );
                    close(fnd_dev);
                    munmap(led_addr, 4096);
                    close(fd);
                }

                else if(shmaddr[9] == 1){   // Counter output

                }
        
                else if(shmaddr[9] == 2){   // Text editor output

                }

                else if(shmaddr[9] == 3){   // Draw board output
                    
                }
            }
        }
        else{   
            /*****************************************************/
            /************* this area is main process *************/
            /*****************************************************/

            shmaddr = shmat(shmid, (void *)0, 0);
            while(1){
                //printf("main\n");
                if(shmaddr[9] == 0){    // Clock
                    time(&current_time);
                    //printf("%ld\n", current_time);
                    //printf(ctime(&current_time));
                    
                    total_time = current_time % 86400;
                    current_hour = total_time / 3600;
                    current_min = (total_time % 3600)/60;
                    //printf("current_time=%ld, total_time=%ld, hour=%ld, min=%ld\n", current_time, total_time, current_hour, current_min);
                    shmaddr[11] = current_hour / 10;
                    shmaddr[12] = current_hour % 10;
                    shmaddr[13] = current_min / 10;
                    shmaddr[14] = current_min % 10;
                }
            }
        }
    }

    return 0;
}
