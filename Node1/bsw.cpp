#include "ee.h"
#include "Arduino.h"
#include "bsw.h"
#include <avr/io.h>
#include <avr/interrupt.h>
//can
#include <SPI.h>
#include "lib/ACAN2517FD/ACAN2517FD.h"

#define TIMER1_US	10000U	

#define LEN_BUF 128
#define BLOCK_SIZE 5
uint64_t key = 0x1234;
ACAN2517FD CAN(9,SPI, 2);

byte begin() {
    ACAN2517FDSettings settings (ACAN2517FDSettings::OSC_20MHz, 500UL * 1000UL, DataBitRateFactor::x1) ;
    settings.mRequestedMode = ACAN2517FDSettings::NormalFD;
	settings.mDriverTransmitFIFOSize = 1 ;
  	settings.mDriverReceiveFIFOSize = 1 ;
	const uint32_t errorCode = CAN.begin (settings, [] { CAN.isr () ; }) ;
    return errorCode;
}

byte CAN_sendMsg(can_msg msg)
{
	CANFDMessage message;
    message.id = msg.id;
	message.len = msg.len;
	message.type = CANFDMessage::CAN_DATA;
	message.ext = false; 
    memcpy(message.data, msg.buf, message.len);
    return CAN.tryToSend(message);
}

byte CAN_checkMsg()
{
	return CAN.available();
}

byte CAN_readMsg(can_msg *msg)
{
	CANFDMessage message;
    if (CAN.available()) {
        CAN.receive(message);
		msg->id=message.id;
		msg->len=message.len;
		msg->buf=message.data;
        return true;
    }
    return false;
}

void mdelay(unsigned long delay_ms)
{
	unsigned long prev_ms = millis(), current_ms = millis();
	unsigned long period_ms = 20, cnt = 0;
	while (cnt < (delay_ms / period_ms)) {
		current_ms = millis();
		if (current_ms - prev_ms >= period_ms) {
			cnt++;
			prev_ms = millis();
		}
	}
}
void printfSerial(const char *fmt, ... )
{
    char buf[LEN_BUF];
    va_list args;
    va_start (args, fmt );
    vsnprintf(buf, LEN_BUF, fmt, args);
    va_end (args);
    Serial.print(buf);
}

 /* extern "C" */

void loop(void)
{
	;
}

void setup(void)
{
	Serial.begin(115200);

	SPI.begin();
	printfSerial("let's serial \n");

	while (CAN_OK != begin()) {
		printfSerial("init fail\n");
		printfSerial("%d\n",begin());
	}
	OsEE_atmega_startTimer1(TIMER1_US);
	printfSerial("CAN init\n");
}
uint32_t feistel(uint32_t half, uint64_t key) {
    half ^= (uint32_t)(key & 0xFFFFFFFF);
    return (half << 3) | (half >> (32 - 3));
}

uint64_t simple_des(uint64_t input, uint64_t key40, int encrypt) {
    uint32_t left  = (input >> 20) & 0xFFFFF;
    uint32_t right = input & 0xFFFFF;

    if (encrypt) {
        for (int i = 0; i < 2; i++) {
            uint32_t temp = right;
            right = left ^ (feistel(right, key40) & 0xFFFFF);
            left = temp;
        }
    } else {
        for (int i = 0; i < 2; i++) {
            uint32_t temp = left;
            left = right ^ (feistel(left, key40) & 0xFFFFF);
            right = temp;
        }
    }

    return (((uint64_t)left << 20) | right) & 0xFFFFFFFFFFULL;
}

uint64_t bytes_to_uint64(unsigned char *block) {
    uint64_t result = 0;
    for (int i = 0; i < BLOCK_SIZE; i++) {
        result = (result << 8) | block[i];
    }
    return result;
}

void uint64_to_bytes(uint64_t value, unsigned char *block) {
    for (int i = BLOCK_SIZE-1; i >= 0; i--) {
        block[i] = value & 0xFF;
        value >>= 8;
    }
}
void encrypt(unsigned char *plaintext, unsigned char *encrypted) {
    uint64_t plain64 = bytes_to_uint64(plaintext);
    uint64_t cipher64 = simple_des(plain64, key, 1);
    uint64_to_bytes(cipher64, encrypted);
}

void decrypt(unsigned char *encrypted, unsigned char *decrypted) {
    uint64_t cipher64 = bytes_to_uint64(encrypted);
    uint64_t plain64 = simple_des(cipher64, key, 0);
    uint64_to_bytes(plain64, decrypted);
}
#define  _BV(bit) (1 << (bit))	// <<--
int main(void)
{
	PORTC = _BV(PC0); // PC0 == PA0
	PCICR = _BV(PCIE1);   //PCIE1
	PCMSK1 = _BV(PCINT8);  //PCINT8
	EIFR = 0xff;
	sei();

	init();

#if defined(USBCON)
	USBDevice.attach();
#endif
	
	setup();



	StartOS(OSDEFAULTAPPMODE);	/* never returns */

	return 0;
}
uint32_t feistel(uint32_t half, uint64_t key);
uint64_t simple_des(uint64_t input, uint64_t key, int encrypt);
uint64_t bytes_to_uint64(unsigned char *block);
void uint64_to_bytes(uint64_t value, unsigned char *block);
void encrypt(unsigned char *plaintext, unsigned char *encrypted);
void decrypt(unsigned char *encrypted, unsigned char *decrypted);