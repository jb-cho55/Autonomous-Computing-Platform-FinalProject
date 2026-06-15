#include "bsw.h"

uint16_t throttle_adc;
uint16_t lidar_adc  ;
uint8_t camera_adc;
uint16_t a0;
ISR2(TimerISR)
{
    IncrementCounter(counter1);
}


TASK (TASK_readADC){
	throttle_adc = analogRead(A1);
    lidar_adc = analogRead(A2);
	TerminateTask();
}

TASK (Task1){
    struct can_msg send_msg = {0};

    send_msg.id = 0x123;
    unsigned int len = 8;
    send_msg.len = len;
    unsigned char* buf_send = (unsigned char*)malloc(sizeof(unsigned char)*len);

	ActivateTask(TASK_readADC);
    
	buf_send[0] = (camera_adc >> 0) & 0xff;
    buf_send[1] =(throttle_adc >> 8) & 0xff;
    buf_send[2] = (throttle_adc >> 0) & 0xff;
    buf_send[3] = (lidar_adc >> 8) & 0xff;
    buf_send[4] = (lidar_adc >> 0) & 0xff;
    buf_send[5] = 0;
    buf_send[6] = 0;
    buf_send[7] = 0;

    printfSerial("raw: %x ",buf_send[0]);
    printfSerial("%x ",buf_send[1]);
    printfSerial("%x ",buf_send[2]);
    printfSerial("%x ",buf_send[3]);
    printfSerial("%x ",buf_send[4]);
    printfSerial("%x ",buf_send[5]);
    printfSerial("%x ",buf_send[6]);
    printfSerial("%x \n",buf_send[7]);    

    encrypt(buf_send,buf_send);

    printfSerial("enc: %x ",buf_send[0]);
    printfSerial("%x ",buf_send[1]);
    printfSerial("%x ",buf_send[2]);
    printfSerial("%x ",buf_send[3]);
    printfSerial("%x ",buf_send[4]);
    printfSerial("%x ",buf_send[5]);
    printfSerial("%x ",buf_send[6]);
    printfSerial("%x \n",buf_send[7]); 
    printfSerial("thr: %u \t", throttle_adc);
	printfSerial(" lidar: %u \t", lidar_adc); 
    printfSerial("camera_adc: %u \n",camera_adc);

    send_msg.buf = buf_send;
    CAN_sendMsg(send_msg);
    free(buf_send);
	TerminateTask();
}

ISR2(ButtonISR)
{
    if ((PINC & 0x01) != 0) return;
    DisableAllInterrupts();
    a0 = analogRead(A0);
    if (a0 < 50) { // UP
        camera_adc =1;
        } 
    else if (a0 < 200) { // DOWN
        camera_adc = 0;
        } 
    else if (a0 < 380) { // LEFT
        camera_adc = 2;
        } 
    else if (a0 < 520) { // RIGHT
        camera_adc = 3;
        }

    EnableAllInterrupts();
}

TASK (Task2){
    camera_adc = 0;
    TerminateTask();
}