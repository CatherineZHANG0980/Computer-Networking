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
    int sid;                        //stripe id
    uint8_t *encode_matrix;         //encode matrix
    uint8_t *table;                 
    unsigned char **data_block;     //pointer to the first data block
    unsigned char **parity_block;   //pointer to the first parity block
}Stripe;

int sendn(int sd, void * buff, int buf_len);

int recvn(int sd, void * buff, int buf_len);

struct message_s replymessage(unsigned char type, unsigned int payloadlen);

struct message_s *make_header(unsigned char type, unsigned int length);

uint8_t *encode_data(int n, int k, Stripe *stripe, int block_size);

void decode_data(int n, int k, Stripe *stripe, int block_size, unsigned char *decoded_result);
