# include "myftp.h"

int main(int argc, char** args){
    char* serverIPAddress = args[1];
    char* serverPort = args[2];
    char* method = args[3];
    char* fileName;

    int sd = socket(AF_INET, SOCK_STREAM, 0);

    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = inet_addr(serverIPAddress);
    server_addr.sin_port = htons(atoi(serverPort));

    if(connect(sd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0){
        printf("Connection Error: %s (Errno:%d)\n", strerror(errno),errno);
        exit(0);
    }

    struct message_s *message = malloc(sizeof(struct message_s));;
    if(strcmp(method, "list") == 0 && argc == 4){
        message = make_header(0xA1, sizeof(struct message_s));
    } else {
        fileName = args[4];
        if(strcmp(method, "get") == 0 && argc == 5){
            message = make_header(0xB1, sizeof(struct message_s) + strlen(fileName) + 1);
        }else if(strcmp(method, "put") == 0 && argc == 5){
            message = make_header(0xC1, sizeof(struct message_s) + strlen(fileName) + 1); //checked
        }else{
            printf("Usage: ./myftpclient <server ip addr> <server port> <list|get|put> <file>\n");
        }
    }

    if(send(sd, message, sizeof(struct message_s), 0) < 0){
        printf("Send Error: %s (Errno:%d)\n", strerror(errno), errno);
        exit(0);
    }

    // put and get send fileName
    if(strcmp(method, "get") == 0 || strcmp(method, "put") == 0){
        if(sendn(sd, fileName, strlen(fileName)+1) < 0){
            printf("Send Error: %s (Errno:%d)\n", strerror(errno), errno);
            exit(0);
        };
    }

    // recv server reply
    struct message_s signal;
    if(recv(sd, &signal, sizeof(struct message_s), 0) < 0){
        printf("Receive Error: %s (Errno:%d)\n", strerror(errno), errno);
        exit(0);
    }

    // analyze the signal
    switch(signal.type){
        case 0xA2:
            printf("Command: List\n");
            char listbuff[100];
            if(recv(sd, listbuff, sizeof(listbuff), 0) < 0){
                printf("Receive Error: %s (Errno:%d)\n", strerror(errno), errno);
                exit(0);
            }
            printf("%s", listbuff);
            close(sd);
            break;

            close(sd);
            break;
        case 0xB2:
            printf("Command: Get\n");
            int len;
            unsigned char *buff[CHUNK_SIZE];
            FILE *fp1 = NULL;
            fp1 = fopen(fileName, "wb");

            // receive file data
            if (recv(sd, buff, sizeof(struct message_s), 0) < 0) {
                printf("Receive Error: %s (Errno:%d)\n", strerror(errno),errno);
                exit(0);
            }
            struct message_s* fileMessage_b = malloc(sizeof(struct message_s)); 
            memcpy(fileMessage_b, buff, 10);
            // printf("File data message 0x%02x, 0x%08x\n", fileMessage_b->type, fileMessage_b->length);

            long fsize=0;
            fsize = ntohl(fileMessage_b->length) - 10;
            while((len = recvn(sd, buff, fsize>CHUNK_SIZE?CHUNK_SIZE:fsize)) > 0){
                fwrite(buff, sizeof(char), fsize>CHUNK_SIZE?CHUNK_SIZE:fsize, fp1);
                fsize -= CHUNK_SIZE;
                if(fsize <= 0){
                    break;
                }
            }
            fclose(fp1);
            close(sd);
            free(fileMessage_b);
            break;

        case 0xB3:
            printf("File not exists\n");
            close(sd);
            break;

        case 0xC2:
            printf("Command: Put\n");
            
            FILE *fp = NULL;
            fp = fopen(fileName, "rb");
            if (fp == NULL){
                printf("File not exists\n");
                exit(0);
            }
            fseek(fp, 0, SEEK_END);
            fsize = 0;
            fsize = ftell(fp);
            // printf("1-fsize : %lu", fsize);
            fseek(fp, 0, SEEK_SET); // move the file pointer fp to the begin of file

            // read whole data into buffer
            char *filedata = (char*)malloc((fsize + 1) * sizeof(char));
            *filedata = '\0';
            fread(filedata, fsize, 1, fp);
            // printf("2-fsize: %lu\n", filedata, fsize);
            // printf("filedata size %lu, length %lu\n", sizeof(filedata), strlen(filedata));
            filedata[fsize] = '\0';
            
            struct message_s put_filemessage;
            put_filemessage = replymessage(0xFF, fsize);
            // printf("put file message 0x%02x, 0x%08x\n",put_filemessage.type, put_filemessage.length);

            // send header message
            if (send(sd, &put_filemessage, sizeof(put_filemessage), 0) < 0) {
                printf("Send Error: %s (Errno:%d)\n", strerror(errno), errno);
                fclose(fp);
                free(filedata);
                exit(0);
            }
            // send the content of file
            sendn(sd, filedata, fsize);

            free(filedata);
            fclose(fp);
            close(sd);
            break;
    }

    return 0;
}
