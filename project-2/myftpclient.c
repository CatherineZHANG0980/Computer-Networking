# include "myftp.h"
int n = 0;
int k = 0;
int block_size = 0;
int num_stripe = 0;
char ips[5][20] = {{0}}; //n<=5
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

// n = 5, k = 2
// 1 loss, 2 data loss

int valid_sock(struct message_s* signals, int *connected_socket){
    int rt = 0, i;
    for(i=0; i<n; i++){
        if(signals[i].type == 0xB2 && connected_socket[i] == 1){
            rt++;
            connected_socket[i] *= 1;
        }else{
            connected_socket[i] *= 0;
        }
    }
    if(rt < k){
        rt = -1;
    }
    return rt;
}

int main(int argc, char** args){
    char* method = args[2];
    char* fileName;

    //read clientconfig.txt
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
        //read ip addresses
        p = strtok(line_tmp, ":");
        strcpy(ips[i], p);
        //read port numbers
        p = strtok(NULL, ":");
        ports[i++] = atoi(p);
    }
    fclose(fp);

    //set up sockets set
    int *sd = (int *)malloc(sizeof(int) * n);
    memset (sd, 0, sizeof (int) * n);
    int connection_number = 0;
    int *connected_socket = malloc(sizeof(int) * n);
    for(i = 0; i < n; i++){
        connected_socket[i] = 0;
    }
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
            continue;
        }

        connected_socket[i] = 1;
        connection_number++;
        printf("connect socker number: %d\n", connection_number);

        printf("serverIP: %s\tPort: %d\tsocket_fd[%d] is %d\n", serverIPAddress, serverPort, i+1, sd[i]);
        
    }
    printf("connected sockets: \n");
    for(i = 0; i < n; i++){
        printf("%d ", connected_socket[i]);
    }
    // printf("\n");
    printf("\nconnect socker number: %d\n", connection_number);
    if(strcmp(method, "put") == 0){
        if(connection_number != n) {
            printf("No enough server is online for ");
            printf("%s\n", method);
            exit(0);
        }
    }else if(strcmp(method, "get") == 0){
        if(connection_number < k) {
            printf("No enough server is online for ");
            printf("%s\n", method);
            exit(0);
        }
    }else if(strcmp(method, "list") == 0){
        if(connection_number == 0) {
            printf("No enough server is online for ");
            printf("%s\n", method);
            exit(0);
        }
    }

    // prepare file name message
    struct message_s *message = malloc(sizeof(struct message_s));;
    if(strcmp(method, "list") == 0 && argc == 3){
        message = make_header(0xA1, sizeof(struct message_s));
    } else {
        fileName = args[3];
        if(strcmp(method, "get") == 0 && argc == 4){
            message = make_header(0xB1, sizeof(struct message_s) + strlen(fileName) + 1);
        }else if(strcmp(method, "put") == 0 && argc == 4){
            message = make_header(0xC1, sizeof(struct message_s) + strlen(fileName) + 1); //checked
        }else{
            printf("Usage: ./myftpclient clientconfig.txt <list|get|put> <file>\n");
        }
    }


    int count = 0;

    struct message_s *signal_set = malloc(sizeof(struct message_s) * n);
    struct message_s signal;
    for(i=0; i < n; i++){
        if(strcmp(method, "get") == 0 || strcmp(method, "list") == 0){
            if(connected_socket[i] == 1){
                if(send(sd[i], message, sizeof(struct message_s), 0) < 0){
                    printf("Send Error: %s (Errno:%d)\n", strerror(errno), errno);
                    exit(0);
                }
                // put and get send fileName
                if(strcmp(method, "get") == 0){
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
                signal_set[i] = signal;
            }else{
                continue;
            }
        }else{
            if(send(sd[i], message, sizeof(struct message_s), 0) < 0){
                printf("Send Error: %s (Errno:%d)\n", strerror(errno), errno);
                exit(0);
            }
            // put and get send fileName
            if(strcmp(method, "put") == 0){
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
        }
    }

    if(strcmp(method, "get") == 0){
        int valid_socket = valid_sock(signal_set, connected_socket);
        if(valid_socket < 0){
            printf("GET: receive signals ");
            for(i=0; i<n; i++){
                printf("0x%02x\t", signal_set[i].type);
            }
            printf("\n");
            printf("Error: data loss error\n");
            exit(0);
        }else{
            signal.type = 0xB2;
        }
    }

    // analyze the signal
    switch(signal.type){
        case 0xA2:
            printf("Command: List\n");
            int printed = 0;

            for(i = 0; i < n; i++){
                if(connected_socket[i] == 1){
                    char listbuff[100] = "\0";
                    if(recv(sd[i], listbuff, sizeof(listbuff), 0) < 0){
                        printf("Receive Error: %s (Errno:%d)\n", strerror(errno), errno);
                        continue;
                    }else{
                        if(printed == 0){
                            printf("%s\n", listbuff);
                            printed = 1;
                        }
                        // printf("from socket %d:\n%s", i, listbuff);
                        // break;
                    }
                }
                printf("\n");
            }
            break;

        case 0xB2:
        {
            unsigned char *buff[CHUNK_SIZE];
            FILE *fp1 = NULL;
            long fsize = 0;
            int numOfStripe = 0, count_stripe = 0;
            Stripe *allStripe = NULL;
            fp1 = fopen(fileName, "wb");
            int j;
            // printf("1\n");
                
            for(i=0; i<n; i++){
                if(connected_socket[i] == 1){
                    //receive 0xFF
                    if (recv(sd[i], buff, sizeof(struct message_s), 0) < 0) {
                        printf("Receive Error: %s (Errno:%d)\n", strerror(errno),errno);
                        continue;
                    }
                    struct message_s* fileMessage_b = malloc(sizeof(struct message_s)); 
                    memcpy(fileMessage_b, buff, 10);
                    printf("File data message 0x%02x, 0x%08x\n", fileMessage_b->type, fileMessage_b->length);
                    // printf("2\n");
            
                    fsize = ntohl(fileMessage_b->length) - 10;

                    printf("%ld", fsize);

                    free(fileMessage_b);
                }
            }
            // printf("3\n");

            numOfStripe = ceil((double)fsize / (block_size * k));
            allStripe = malloc(sizeof(Stripe) * numOfStripe);
            for(count_stripe = 0; count_stripe < numOfStripe; count_stripe++){
                allStripe[count_stripe].sid = count_stripe;
                allStripe[count_stripe].data_block = malloc(sizeof(unsigned char *) * k);
                allStripe[count_stripe].parity_block = malloc(sizeof(unsigned char *) * (n - k));
                for(j = 0; j < k; j++){
                    allStripe[count_stripe].data_block[j] = malloc(block_size);
                }
                for(j = 0; j < (n - k); j++){
                    allStripe[count_stripe].parity_block[j] = malloc(block_size);
                }
            }
            // printf("4\n");

            for(i = 0; i < n; i++){
                if(connected_socket[i] == 0){
                    // set null
                    if(i < k){
                        for(count_stripe = 0; count_stripe < numOfStripe; count_stripe++){
                            allStripe[count_stripe].data_block[i] = NULL;
                        }
                    }else{
                        for(count_stripe = 0; count_stripe < numOfStripe; count_stripe++){
                            allStripe[count_stripe].parity_block[i - k] = NULL;
                        }
                    }
                }
            }

            //file data
            fd_set readfdset;
            int *count_receive = malloc(sizeof(int) * n);
            for(i = 0; i < n; i++){
                count_receive[i] = 0;
            }
            // printf("5\n");
            while(1){
                FD_ZERO(&readfdset);
                for(i=0; i < n; i++){
                    if(signal_set[i].type == 0xB2){
                        FD_SET(sd[i], &readfdset);
                    }
                }
                // printf("6\n");

                select(max_sock(sd, n) + 1, &readfdset, NULL, NULL, NULL);
                for(i=0; i < n; i++){
                    // int tmp_fsize = fsize;
                    if(FD_ISSET(sd[i], &readfdset) && count_receive[i] == 0){
                        for(count_stripe = 0; count_stripe < numOfStripe; count_stripe++){
                            if(i < k){
                                int lennn = recvn(sd[i], allStripe[count_stripe].data_block[i], block_size); // 3
                                printf("length: %d!!\n", lennn);
                            }else{
                                int lennnn = recvn(sd[i], allStripe[count_stripe].parity_block[i - k], block_size); // 3
                                printf("length: %d!!\n", lennnn);
                            }
                        }
                        count_receive[i] = 1;
                    }
                }
                int x = 0;
                for(i = 0; i < n; i++){
                    x += count_receive[i];
                }
                printf("x: %d\n", x);
                if(x >= k){
                    break;
                }
            }
            printf("Done receiving\n");

            // decode
            for(i=0; i < numOfStripe; i++){
                // printf("7");
                unsigned char *buff2 = decode_data_returned(n, k, &allStripe[i], block_size, i, numOfStripe, fsize);
                // printf("stripe %d: %s\n", i, buff2);
                int remainSize = fsize % (block_size * k); // 0
                if(i == numOfStripe - 1 && remainSize != 0){
                    fwrite(buff2, sizeof(unsigned char), (fsize % (block_size * k)), fp1); 
                }else{
                    fwrite(buff2, sizeof(unsigned char), block_size * k, fp1); 
                }

                free(buff2);
            }
            fclose(fp1);

            break;
        }

        case 0xB3:
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
            long pfsize = 0;
            pfsize = ftell(fp);
            fseek(fp, 0, SEEK_SET); // move the file pointer fp to the begin of file

            // read whole data into buffer
            char *filedata = (char*)malloc((pfsize + 1) * sizeof(char));
            *filedata = '\0';
            fread(filedata, pfsize, 1, fp);
            // printf("filedata size %lu, length %lu\n", sizeof(filedata), strlen(filedata));
            filedata[pfsize] = '\0';
            
            struct message_s put_filemessage;
            printf("pf size: %ld", pfsize);
            put_filemessage = replymessage(0xFF, pfsize);
            printf("Header: 0x%02x, 0x%08x\n", put_filemessage.type, put_filemessage.length);

            // chunk data & encode
            int pnumOfStripe = ceil((double)pfsize / (k * block_size));
            Stripe *pallStripe = malloc(sizeof(Stripe) * pnumOfStripe);
            int pj=0;
            for(i = 0; i < pnumOfStripe; i++){
                pallStripe[i].sid = i;
                pallStripe[i].data_block = malloc(sizeof(unsigned char *) * k);
                for(pj = 0; pj < k; pj++){
                    pallStripe[i].data_block[pj] = malloc(block_size);
                    strncpy(pallStripe[i].data_block[pj], filedata + (((i * k) + pj) * block_size), block_size);
                    //   printf("%s\n", pallStripe[i].data_block[pj]);
                }
                pallStripe[i].parity_block = malloc(sizeof(unsigned char *) * (n - k));
                for(pj = 0; pj < (n - k); pj++){
                    pallStripe[i].parity_block[pj] = malloc(block_size);
                }
                encode_data(n, k, &pallStripe[i], block_size);
            }

            // I-O multiplexing
            fd_set writefdset;

            int *count_sent = malloc(sizeof(int) * n);
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
                        for(pj=0; pj<pnumOfStripe; pj++){
                            if(i<k){
                                sendn(sd[i], pallStripe[pj].data_block[i], block_size);
                            }else{
                                sendn(sd[i], pallStripe[pj].parity_block[i-k], block_size);
                            }
                        }    
                        count_sent[i] = 1;
                    }
                }
                int x = 0;
                for(i = 0; i < n; i++){
                    x += count_sent[i];
                }
                if(x == n){
                    printf("Done sending\n");
                    break;
                }
            }

            for(i=0; i<n; i++){            
                close(sd[i]);      
            }

            free(filedata);
            free(pallStripe);
            fclose(fp);
            break;
    }
       

    for(i = 0; i < n; i++){
        if(connected_socket[i] == 1){
            close(sd[i]);
        }
    }
    
    free(sd);
    return 0;
}
