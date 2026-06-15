#include "bsw.h"
#include "Car_Model.h"
#include "ee.h"

#define ResRpm      0

uint16_t throttle;
uint16_t lidar;
uint8_t camera;
uint16_t rpm;

ISR2(TimerISR)
{
    IncrementCounter(counter1);
}

TASK(ReadCAN)
{
    struct can_msg receive_msg = {0};
    CAN_readMsg(&receive_msg);
	if(receive_msg.id==0x456){
	printfSerial("enc + sig: %x ", receive_msg.buf[0]);
	printfSerial(" %x ", receive_msg.buf[1]);
	printfSerial(" %x ", receive_msg.buf[2]);
	printfSerial(" %x ", receive_msg.buf[3]);
	printfSerial(" %x ", receive_msg.buf[4]);
	printfSerial(" %x ", receive_msg.buf[5]);
	printfSerial(" %x ", receive_msg.buf[6]);
	printfSerial(" %x \n", receive_msg.buf[7]);

	unsigned char pub[3] = {0x5D, 0x66, 0xCB};
	unsigned char hash[3] = {0};
	unsigned char decrypted_sig[3] = {0};
	unsigned char message[5] = {};
	unsigned char sig[3] = {};
	unsigned char decrypted[8] = {};
	memcpy(message, receive_msg.buf, 5);
	memcpy(sig, receive_msg.buf+5, 3);
	printfSerial("%d\n", micro_verify(message, pub, sig, hash, decrypted_sig));
	printfSerial("Hash         :    %02X %02X %02X\n", hash[0], hash[1], hash[2] );
	printfSerial("decrypted_sig:    %02X %02X %02X\n", decrypted_sig[0], decrypted_sig[0], decrypted_sig[2]);

	decrypt(message, decrypted);

	printfSerial("raw: %x ", decrypted[0]);
	printfSerial(" %x ", decrypted[1]);
	printfSerial(" %x ", decrypted[2]);
	printfSerial(" %x ", decrypted[3]);
	printfSerial(" %x ", decrypted[4]);
	printfSerial(" %x ", decrypted[5]);
	printfSerial(" %x ", decrypted[6]);
	printfSerial(" %x \n", decrypted[7]);



	camera   = (uint8_t)decrypted[0];
    throttle = ((uint16_t)decrypted[1] << 8) | (uint16_t)decrypted[2];
    lidar    = ((uint16_t)decrypted[3] << 8) | (uint16_t)decrypted[4];

    printfSerial("thr: %u ", throttle);
	printfSerial("lidar: %u ", lidar);
	//printfSerial("%d ",searchRemainMemory());


    // 단순화된 camera 분기
    switch (camera) {
    case 1:
		printfSerial("camera: Front " );
		ActivateTask(SensorTask);
        SetEvent(SensorTask, EventFront);
        break;
    case 2:
		printfSerial("camera: Left " );
		ActivateTask(SensorTask);
        SetEvent(SensorTask, EventLeft);
        break;
    case 3:
		printfSerial("camera: Right " );
		ActivateTask(SensorTask);
        SetEvent(SensorTask, EventRight);
        break;
	default:
		printfSerial("camera: None " );
		ActivateTask(SensorTask);
		SetEvent(SensorTask, EventNone);
		break;
	}

	}
}

TASK(SensorTask)
{
    EventMaskType mask;
    WaitEvent(EventFront | EventLeft | EventRight | EventNone);
GetEvent(SensorTask, &mask);

GetResource(ResRpm);

if (lidar < 500 && (mask & (EventFront | EventLeft | EventRight))) {
    rpm = control_rpm(throttle, 1);  // 회피 모드
} else {
    rpm = control_rpm(throttle, 0);  // 기본 모드
}
ReleaseResource(ResRpm);
printfSerial("rpm:%u\n", rpm);
ClearEvent(mask);
TerminateTask();

}
