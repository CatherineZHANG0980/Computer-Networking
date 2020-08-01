# include <stdio.h>
# include <stdlib.h>
# include <unistd.h>
# include <string.h>
# include <errno.h>
# include <sys/socket.h>
# include <sys/types.h>
# include <netinet/in.h>
# include <arpa/inet.h>
# include <dirent.h>
# include <pthread.h>
#include <sys/stat.h>
#include <sys/select.h>
# include <math.h>
# include <isa-l.h>

#define CHUNK_SIZE 1000

struct message_s {
    unsigned char protocol[5];
    unsigned char type;
    unsigned int length;
} __attribute__ ((packed));

typedef struct stripe {
    int sid; //stripe id
    unsigned char **data_block; //pointer to the first data block
    unsigned char **parity_block; //pointer to the first parity block
}Stripe;

int sendn(int sd, void * buff, int buf_len);

int recvn(int sd, void * buff, int buf_len);

struct message_s replymessage(unsigned char type, unsigned int payloadlen);

struct message_s *make_header(unsigned char type, unsigned int length);

unsigned char *decode_data_returned(int n, int k, Stripe *stripe, int block_size, int stripeNum, int numOfStripe, long filesize);

uint8_t *encode_data(int n, int k, Stripe *stripe, int block_size);
