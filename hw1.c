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
    
    struct tm *present_time;
    time_t current_time;
    time_t total_time=0, current_hour=0, current_min=0;

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
        unsigned char prev_push_sw_buff[MAX_BUTTON];

        
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
            memcpy(prev_push_sw_buff, push_sw_buff, buff_size);
            read(dev, &push_sw_buff, buff_size);

            /*  4/13 23:28 add  */
            if(memcmp(prev_push_sw_buff, push_sw_buff, buff_size) == -1){
                for(i=0; i<MAX_BUTTON; i++){
                //if(memcmp(prev_push_sw_buff, push_sw_buff, buff_size) == -1){
                    shmaddr[i] = push_sw_buff[i];
                    //printf("[%d] ",push_sw_buff[i]);
                //}
                }
                printf("\n");
                printf("prev array is ");
                for(i=0; i<9; i++)
                    printf("[%d] ", prev_push_sw_buff[i]);
                printf("\n");
                printf("push array is ");
                for(i=0; i<MAX_BUTTON; i++)
                    printf("[%d] ", push_sw_buff[i]);
                printf("\n");
            }
            else{
                for(i=0; i<9; i++)
                    shmaddr[i] = 0;
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
            /****************** output process ********************/
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
            /******************* main process ********************/
            /*****************************************************/

            shmaddr = shmat(shmid, (void *)0, 0);
            while(1){
                //printf("main\n");
                shmaddr[16]=0;
                if(shmaddr[9] == 0){    // Clock
                    int flag=0;
                    int change_flag=0, clock_init_flag=0;
                    int i;
                    int sw3_count=0;
                    int led_count=0;
                    while(1){
                        if(flag == 0){  // clock initialize
                            //printf("Clock initialize enter!!!!!!!!!!\n");
                            time(&current_time);
                            
                            /*  4/14 1:50   */
                            present_time = localtime(&current_time);
                            shmaddr[11] = present_time->tm_hour / 10;
                            shmaddr[12] = present_time->tm_hour % 10;
                            shmaddr[13] = present_time->tm_min / 10;
                            shmaddr[14] = present_time->tm_min % 10;

                            //printf("%d%d %d%d\n", shmaddr[11], shmaddr[12], shmaddr[13], shmaddr[14]);
                            //printf("%d %d\n", present_time->tm_hour, present_time->tm_min);
                            /*
                            while(current_time >= 86400)
                                current_time -= 43200;
                            */
                            //printf("current_time is %ld\n", current_time);
                            /*
                            total_time = current_time % 86400;
                            total_time = current_time;
                            current_hour = total_time / 3600;
                            current_min = (total_time % 3600)/60;
                            printf("current_time=%ld, total_time=%ld, hour=%ld, min=%ld\n", current_time, total_time, current_hour, current_min);
                            shmaddr[11] = current_hour / 10;
                            shmaddr[12] = current_hour % 10;
                            shmaddr[13] = current_min / 10;
                            shmaddr[14] = current_min % 10;
                            */
                            if(clock_init_flag == 0)
                                shmaddr[15] = 128;
                            else if(clock_init_flag == 1){
                                if(difftime(time(NULL), current_time) >= 1){
                                    current_time = time(NULL);
                                    if(led_count%2 == 0){
                                        shmaddr[15] = 16;
                                    }
                                    else if(led_count %2 == 1){
                                        shmaddr[15] = 32;
                                    }
                                    led_count++;
                                }
                            }
                            
                        }
                        else{
                            //printf("After Clock initialize!!!!!!!!!!!!\n");
                            //for(i=0; i<9; i++)
                            //    printf("[%d] ", shmaddr[i]);
                            //printf("\n");
                            //printf("change_flag is %d\n", change_flag);
                            if(shmaddr[0] == 1){    // if press SW(1)
                                if(change_flag == 0){   // change start
                                    change_flag = 1;
                                    clock_init_flag = 1;
                                }
                                else if(change_flag == 1){  // change end
                                    change_flag = 0;
                                    led_count=0;
                                    shmaddr[15] = 128;
                                }
                                //printf("change_flag is %d\n", change_flag);
                            }
                            else if(change_flag == 0){
                                //current_time = time(NULL);
                                //printf("%f\n", difftime(current_time, time(NULL)));
                                if(difftime(time(NULL), current_time) >= 5){
                                    current_time = time(NULL);
                                    if(shmaddr[14]+1 > 9){
                                        if(shmaddr[13]+1 > 5){
                                            if(shmaddr[11] == 2){
                                                if(shmaddr[12]+1 == 4){
                                                    shmaddr[11]=0;
                                                    shmaddr[12]=0;
                                                }
                                                else
                                                    shmaddr[12]+=1;
                                            }
                                            else{
                                                if(shmaddr[12]+1 > 9){
                                                    shmaddr[11]+=1;
                                                    shmaddr[12] =0;
                                                }
                                                else{
                                                    shmaddr[12] +=1;
                                                    shmaddr[13] = 0;
                                                    shmaddr[14] = 0;
                                                }
                                            }
                                        }
                                        else{
                                            shmaddr[13]+=1;
                                            shmaddr[14]=0;
                                        }
                                    }
                                    else{
                                        //shmaddr[13]+=1;
                                        shmaddr[14]+=1;
                                    }
                                }
                            }
                            else if(shmaddr[1] == 1 && change_flag == 1){   // if press SW(2)
                                flag=0;
                                usleep(100000);
                                time(&current_time);
                                /*  4/14 1:59   */
                                present_time = localtime(&current_time);
                                shmaddr[11] = present_time->tm_hour / 10;
                                shmaddr[12] = present_time->tm_hour % 10;
                                shmaddr[13] = present_time->tm_min / 10;
                                shmaddr[14] = present_time->tm_min % 10;
                                /*
                                while(current_time >= 86400)
                                    current_time -= 43200;
                                current_hour /= 3600;
                                current_min = (current_time%3600)/60;
                                shmaddr[11] = current_hour / 10;
                                shmaddr[12] = current_hour % 10;
                                shmaddr[13] = current_min / 10;
                                shmaddr[14] = current_min % 10;
                                */
                            }
                            else if(shmaddr[2] == 1 && change_flag == 1){   // if press SW(3)
                                usleep(100000);
                                if(sw3_count != 0)
                                    break;
                                else{

                                    if(shmaddr[11]==2){
                                        if(shmaddr[12]+1 == 4){
                                            shmaddr[11]=0;
                                            shmaddr[12]=0;
                                        }
                                        else
                                            shmaddr[12] += 1;
                                    }
                                    else{
                                        if(shmaddr[12]+1 > 9){
                                            shmaddr[11] += 1;
                                            shmaddr[12] = 0;
                                        }
                                        else
                                            shmaddr[12] += 1;
                                    }
                                    sw3_count++;
                                }
                                /*  4/13, 23:06
                                if(shmaddr[2] != shmaddr[16]){
                                    shmaddr[16] = shmaddr[2];
                                    if(shmaddr[11]==2){
                                        if(shmaddr[12]+1 > 4){
                                            shmaddr[11]=0;
                                            shmaddr[12]=0;
                                        }
                                        else
                                            shmaddr[12] += 1;
                                    }
                                    else{
                                        if(shmaddr[12]+1 > 9){
                                            shmaddr[11] += 1;
                                            shmaddr[12] = 0;
                                        }
                                        else
                                            shmaddr[12] = shmaddr[12]+1;
                                    }
                                }
                                */
                            }
                            else if(shmaddr[3] == 1){                       // if press SW(4)
                                usleep(100000);
                                if(shmaddr[14]+1 > 9){
                                    if(shmaddr[13]+1 != 6){     
                                        shmaddr[13] += 1;
                                        shmaddr[14] = 0;
                                    }
                                    else{
                                        shmaddr[13] = 0;
                                        shmaddr[14] = 0;
                                        if(shmaddr[11] == 2){
                                            if(shmaddr[12]+1 == 4){
                                                shmaddr[11]=0;
                                                shmaddr[12]=0;
                                            }
                                            else
                                                shmaddr[12] += 1;
                                        }
                                        else{
                                            if(shmaddr[12]+1 > 9){
                                                shmaddr[11] += 1;
                                                shmaddr[12] = 0;
                                            }
                                            else
                                                shmaddr[12] += 1;
                                        }
                                    }
                                }
                                else
                                    shmaddr[14] += 1;
     
                            }
                            if(change_flag == 1){
                                if(led_count == 0){
                                    shmaddr[15] = 48;
                                    //usleep(100000);
                                }
                                else if(led_count %2 == 0){
                                    if(difftime(time(NULL), current_time) >= 1){
                                        current_time=time(NULL);
                                        shmaddr[15] = 32;
                                        //usleep(100000);
                                    }
                                }
                                else if(led_count %2 == 1){
                                    if(difftime(time(NULL), current_time) >= 1){
                                        current_time=time(NULL);
                                        shmaddr[15] = 16;
                                        //usleep(100000);
                                    }
                                }
                                led_count++;
                            }
                            shmaddr[16]=0;
                            sw3_count=0;
                        }
                        flag++;
                    }
                    //time(&current_time);
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
