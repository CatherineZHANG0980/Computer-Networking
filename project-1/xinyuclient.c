#include "myftp.h"

int n = 0;
int k = 0;
int block_size = 0;
char ips[5][20] = {{0}};
int ports[5] = {0};

int max_sock(int* socket_set, int n){
    int max = -1, i = 0;
    for(i=0; i<n; i++){
        if(socket_set[i] > max){
            max = socket_set[i];
        }
    }
    return max;
}

int valid_sock(struct message_s* signals, int n){
    int rt = 0, i;
    for(i=0; i<n; i++){
        if(signals[i].type == 0xB2){
            rt++;
        }else if(signals[i].type == 0xB3){
            printf("File not exist! Server address %s:%d\n", ips[i], ports[i]);
        }
    }
    if(rt < k){
        rt = -1;
    }
    return rt;
}

int main(int argc, char** args){
    if(argc < 3 || argc > 4){
        printf("Usage: ./myftpclient clientconfig.txt <list|get|put> <file>\n");
        exit(0);
    }
    char *method = args[2];
    char *fileName;

    /*
    * read clientconfig.txt
    */
    FILE *fp = NULL;
    fp = fopen(args[1], "r");
    fscanf(fp, "%d %d %d", &n, &k, &block_size);
    printf("n: %d, k: %d, block_size: %d\n", n, k, block_size);
    char br = fgetc(fp); //get the break
    int i = 0;
    char line[50];
    while(fgets(line, sizeof(line), fp) && strlen(line) > 1){
        char line_tmp[50];
        char *p = NULL;
        strcpy(line_tmp, line);
        p = strtok(line_tmp, ":");
        strcpy(ips[i], p);
        p = strtok(NULL, ":");
        ports[i++] = atoi(p);
    }
    fclose(fp);

    /*
    * set up sockets set
    */
    int *sd = (int *)malloc(sizeof(int) * n);
    memset (sd, 0, sizeof (int) * n);
    for(i=0; i<n; i++){
        char* serverIPAddress = ips[i];
        int serverPort = ports[i];

        sd[i] = socket(AF_INET, SOCK_STREAM, 0);

        struct sockaddr_in server_addr;
        memset(&server_addr, 0, sizeof(server_addr));
        server_addr.sin_family = AF_INET;
        server_addr.sin_addr.s_addr = inet_addr(serverIPAddress);
        server_addr.sin_port = htons(serverPort);

        if(connect(sd[i], (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0){
            printf("Connection Error: %s (Errno:%d)\n", strerror(errno),errno);
            // exit(0);
            continue;
        }

        //printf("serverIP: %s\tPort: %d\tsocket_fd[%d] is %d\n", serverIPAddress, serverPort, i+1, sd[i]);  
    }

    /*
    * prepare file message
    */
    struct message_s *message = malloc(sizeof(struct message_s));;
    if(strcmp(method, "list") == 0 && argc == 3){
        message = make_header(0xA1, sizeof(struct message_s));
    } else {
        fileName = args[3];
        if(strcmp(method, "get") == 0 && argc == 4){
            message = make_header(0xB1, sizeof(struct message_s) + strlen(fileName) + 1);
        }else if(strcmp(method, "put") == 0 && argc == 4){
            message = make_header(0xC1, sizeof(struct message_s) + strlen(fileName) + 1); 
        }else{
            printf("Usage: ./myftpclient clientconfig.txt <list|get|put> <file>\n");
        }
    }
    struct message_s signal;
    struct message_s *signal_set = malloc(sizeof(struct message_s) * n);
    for(i=0; i < n; i++){
        if(send(sd[i], message, sizeof(struct message_s), 0) < 0){
            printf("Send Error: %s (Errno:%d)\n", strerror(errno), errno);
            exit(0);
        }
        // put and get send fileName
        if(strcmp(method, "get") == 0 || strcmp(method, "put") == 0){
            if(sendn(sd[i], fileName, strlen(fileName)+1) < 0){
                printf("Send Error: %s (Errno:%d)\n", strerror(errno), errno);
                exit(0);
            };
        }
        // recv server reply   
        if(recv(sd[i], &signal, sizeof(struct message_s), 0) < 0){
            printf("Receive Error: %s (Errno:%d)\n", strerror(errno), errno);
            exit(0);
        }
        if(strcmp(method, "get") == 0){
            signal_set[i] = signal;
        }
    }
    printf("receive signal set:");
    for(int i=0; i<n; i++){
        printf("0x%02x", signal_set[i].type);
    }

    if(strcmp(method, "get") == 0){
        if(valid_sock(signal_set, n) < 0){
            printf("RECOVER ERROR: lost more than k blocks data\n");
            exit(0);
        }else{
            signal.type = 0xB2;
        }
    }

    /*
    * transfer data
    */
    int j, m;
    switch(signal.type){
        case 0xA2: 
            break;
        case 0xB2:
            //declare variables
            int numOfStripe = 0;
            int len = 0;
            long fsize = 0;     
            long *fsize_set = NULL;
            unsigned char *buff[CHUNK_SIZE];
            Stripe *allStripe = NULL;

            //initialize variables
            fsize_set = (long *)malloc(sizeof(long) * n);
            
            //create empty file
            FILE *fp1 = NULL;
            fp1 = fopen(fileName, "wb");

            // receive file data
            for(i=0; i<n; i++){
                if (recv(sd[i], buff, sizeof(struct message_s), 0) < 0) {
                    printf("Receive Error: %s (Errno:%d)\n", strerror(errno),errno);
                    continue;
                }
                struct message_s* fileMessage_b = malloc(sizeof(struct message_s)); 
                memcpy(fileMessage_b, buff, 10);
                // printf("File data message 0x%02x, 0x%08x\n", fileMessage_b->type, fileMessage_b->length);
        
                fsize_set[i] = ntohl(fileMessage_b->length) - 10;
                if(fsize_set[i] > fsize){
                    fsize = fsize_set[i];
                }
                
                free(fileMessage_b);
            }

            //initialize Stripe
            numOfStripe = ceil((double)fsize / block_size);
            printf("number of stripes: %d\n", numOfStripe);
            allStripe = malloc(sizeof(Stripe) * numOfStripe);
            for(m = 0; m < numOfStripe; m++){
                allStripe[m].sid = m;
                allStripe[m].data_block = malloc(sizeof(unsigned char *) * k);
                allStripe[m].parity_block = malloc(sizeof(unsigned char *) * (n - k));
                for(j = 0; j < k; j++){
                    allStripe[m].data_block[j] = malloc(block_size);
                }
                for(j = 0; j < (n - k); j++){
                    allStripe[m].parity_block[j] = malloc(block_size);
                }
            }

            //I-O multiplexing
            fd_set readfdset;
            int *count_receive = malloc(sizeof(int) * n);
            for(i = 0; i < n; i++){
                count_receive[i] = 0;
            }
            while(1){
                FD_ZERO(&readfdset);
                for(i=0; i < n && signal_set[i].type == 0xB2; i++){
                    FD_SET(sd[i], &readfdset);
                }

                //receive file data
                select(max_sock(sd, n) + 1, &readfdset, NULL, NULL, NULL);
                for(i=0; i < n; i++){
                    if(FD_ISSET(sd[i], &readfdset) && count_receive[i] == 0){
                        printf("socket %d ", i);
                        for(m = 0; m < numOfStripe; m++){
                            if(i < k){
                                len = recvn(sd[i], allStripe[m].data_block[i], fsize_set[i] > block_size ? block_size : fsize_set[i]);
                            }else{
                                len = recvn(sd[i], allStripe[m].parity_block[i - k], fsize_set[i] > block_size ? block_size : fsize_set[i]);
                            }
                            fsize_set[i] -= len;
                            printf("receive %d bytes\t", len);
                        }
                        count_receive[i] = 1;
                        printf("\n");
                    }          
                }

                int numRecv = 0;
                for(i = 0; i < n; i++){
                    numRecv += count_receive[i];
                }
                if(numRecv>=k){
                    break;
                }
            }

            //decode data
            unsigned char *buff1 = malloc(sizeof(unsigned char *) * (block_size * k + 1));
            *buff1 = '\0';
            for(i=0; i<numOfStripe; i++){
                decode_data(n, k, &allStripe[i], block_size, buff1);
                buff1[block_size * k] = '\0';
                fwrite(buff1, sizeof(unsigned char), block_size * k, fp1); 
                *buff1 = '\0';
            }

            fclose(fp1);
            free(fsize_set);
            free(buff);
            free(allStripe);
            free(count_receive);
            free(buff1);
            break;
        case 0xC2:
            FILE *fp = NULL;
            fp = fopen(fileName, "rb");
            if (fp == NULL){
                printf("File not exists\n");
                exit(0);
            }
            fseek(fp, 0, SEEK_END);
            fsize = 0;
            fsize = ftell(fp);
            fseek(fp, 0, SEEK_SET);

            // read whole data into buffer
            char *filedata = (char*)malloc((fsize + 1) * sizeof(char));
            *filedata = '\0';
            fread(filedata, fsize, 1, fp);
            // printf("filedata size %lu, length %lu\n", sizeof(filedata), strlen(filedata));
            filedata[fsize] = '\0';

            struct message_s put_filemessage;
            put_filemessage = replymessage(0xFF, fsize);
            // printf("put file message 0x%02x, 0x%08x\n",put_filemessage.type, put_filemessage.length);

            //chunk data & encode
            numOfStripe = ceil((double)fsize / (k * block_size));
            printf("fsize = %ld, numOfStripe = %d \n", fsize, numOfStripe);
            allStripe = malloc(sizeof(Stripe) * numOfStripe);
            for(i = 0; i < numOfStripe; i++){
                allStripe[i].sid = i;
                allStripe[i].data_block = malloc(sizeof(unsigned char *) * k);
                for(j = 0; j < k; j++){
                    allStripe[i].data_block[j] = malloc(block_size);
                    strncpy(allStripe[i].data_block[j], filedata + (((i * k) + j) * block_size), block_size);
                    //   printf("%s\n", allStripe[i].data_block[j]);
                }
                allStripe[i].parity_block = malloc(sizeof(unsigned char *) * (n - k));
                for(j = 0; j < (n - k); j++){
                    allStripe[i].parity_block[j] = malloc(block_size);
                }
                encode_data(n, k, &allStripe[i], block_size);
            }

            // I-O multiplexing
            fd_set writefdset;

            int *count_sent = (int*)malloc(sizeof(int) * n);
            for(i = 0; i < n; i++){
                count_sent[i] = 0;
            }
            while(1){
                FD_ZERO(&writefdset);
                for(i = 0; i < n; i++){
                    FD_SET(sd[i], &writefdset);
                }
                select(max_sock(sd, n) + 1, NULL, &writefdset, NULL, NULL);
                for(i = 0; i < n; i++){
                    if(FD_ISSET(sd[i], &writefdset) && count_sent[i] == 0){
                        if (send(sd[i], &put_filemessage, sizeof(put_filemessage), 0) < 0) {
                            printf("Send Error: %s (Errno:%d)\n", strerror(errno), errno);
                            fclose(fp);
                            free(filedata);
                            exit(0);
                        }
                        printf("Sending %d\n", i);
                        for(m=0; m<numOfStripe; m++){
                            if(i<k){
                                sendn(sd[i], allStripe[m].data_block[i], block_size);
                            }else{
                                sendn(sd[i], allStripe[m].parity_block[i-k], block_size);
                            }
                        }    
                        count_sent[i] = 1;
                    }
                }
                int numSend = 0;
                for(i = 0; i < n; i++){
                    numSend += count_sent[i];
                }
                if(numSend == n){
                    printf("Complete sending encoded data.\n");
                    break;
                }
            }
            for(i=0; i<n; i++){            
                close(sd[i]);      
            }

            fclose(fp);
            free(filedata);
            free(allStripe);
            free(count_sent);
            break;
    }

    free(sd);
    free(signal_set);
    free(message);

    return 0;
}