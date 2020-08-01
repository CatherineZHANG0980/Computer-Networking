#include "myftp.h"

int sendn(int sd, void * buff, int buf_len){
    int n_left = buf_len;
    int n = 0;
    while (n_left > 0){
        if((n = send(sd,buff+(buf_len-n_left),n_left, 0)) < 0){
            printf("Send Error: %s (Errno:%d)\n", strerror(errno), errno);
            if(errno == EINTR)
                n = 0;
            else
                return -1;
        }else if(n == 0){
            printf("End of file\n");
            return 0;
        }
        // printf("Send %d\n", n);
        n_left -= n;
    }
    return buf_len;
}


int recvn(int sd, void * buff, int buf_len){
    int n_left = buf_len;
    int n = 0;
    while (n_left > 0){
        if((n = recv(sd,buff+(buf_len-n_left),n_left, 0)) < 0){
            printf("Receive Error: %s (Errno:%d)\n", strerror(errno), errno);
            if(errno == EINTR)
                n = 0;
            else
                return -1;
        }else if(n == 0){
            printf("End of file\n");
            return 0;
        }
        // printf("recv %d\n", n);
        n_left -= n;
    }
    return buf_len;
}

struct message_s *make_header(unsigned char type, unsigned int length){
    struct message_s *message = (struct message_s*)malloc(sizeof(struct message_s));
    message->protocol[0] = 109; //m
	message->protocol[1] = 121; //y
	message->protocol[2] = 102; //f
	message->protocol[3] = 116; //t
	message->protocol[4] = 112; //p
    message->type = type;
    //Use network byte ordering for transferring data of type int
    message->length = htonl(length);
    // printf("protocol message 0x%02x, 0x%08x\n",message->type, message->length);
    return message;
}

struct message_s replymessage(unsigned char type, unsigned int payloadlen){
    struct message_s message;
	message.protocol[0] = 109; //m
	message.protocol[1] = 121; //y
	message.protocol[2] = 102; //f
	message.protocol[3] = 116; //t
	message.protocol[4] = 112; //p
	message.type = type;
	message.length = htonl(10 + payloadlen);
    // printf("protocol message 0x%02x, 0x%08x\n",message.type, message.length);
	return message;
}
