#ifndef BSW_H_
#define BSW_H_

#include "ee.h"
#include "Arduino.h"
#define CAN_OK              (0)
#define CAN_FAILINIT        (1)
#define CAN_FAILTX          (2)
#define CAN_MSGAVAIL        (3) // dont use- hong
#define CAN_NOMSG           (4)
#define CAN_CTRLERROR       (5)
#define CAN_GETTXBFTIMEOUT  (6)
#define CAN_SENDMSGTIMEOUT  (7)
#define CAN_FAIL            (0xff)

#ifdef __cplusplus
extern "C"{
#endif

struct can_msg {
    unsigned char len;
    unsigned long id;
    unsigned char* buf;
    
};

void mdelay(unsigned long delay_ms);
void printfSerial(const char *fmt, ... );

byte CAN_sendMsg(struct can_msg msg);
byte CAN_checkMsg();
byte CAN_readMsg(struct can_msg* msg);
int searchRemainMemory();
int freeMemory();
uint32_t feistel(uint32_t half, uint64_t key);
uint64_t simple_des(uint64_t input, uint64_t key, int encrypt);
uint64_t bytes_to_uint64(unsigned char *block);
void uint64_to_bytes(uint64_t value, unsigned char *block);
void encrypt(unsigned char *plaintext, unsigned char *encrypted);
void decrypt(unsigned char *encrypted, unsigned char *decrypted);
static void mini_hash24(const unsigned char *data, size_t len, unsigned char out[3]);
static int is_prime(uint32_t x);
static int32_t modinv32(int32_t e, int32_t phi);
static uint32_t modexp(uint32_t b, uint32_t e, uint32_t m);
static void derive_rsa(const unsigned char priv[2], uint32_t *n, uint32_t *d);
int micro_verify(const unsigned char message[5], const unsigned char pub[3], const unsigned char sig[3], unsigned char hash[3], unsigned char decrypted_sig[3]);


#ifdef __cplusplus
}
#endif






#endif /* BSW_H_ */
