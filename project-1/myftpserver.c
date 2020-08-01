# include "myftp.h"

char *list_files(char *root){
    struct dirent *to_read;
    DIR *dir = opendir(root);
    char *to_return = (char *)malloc(sizeof(char));

    while((to_read = readdir(dir)) != NULL){
        to_return = realloc(to_return, strlen(to_return) + strlen(to_read->d_name) + 1);
        strcat(to_return, to_read->d_name);
        strcat(to_return, "\n");
    }
    return to_return;
};

int check_file_exists(char *root, char *dirname){
    struct dirent *to_read;
    DIR *dir = opendir(root);

    while((to_read = readdir(dir)) != NULL){
        if(strcmp(dirname, to_read->d_name) == 0){
            return 1;
        }
    }
    return 0;
}

void *connection_handler(int client_sd){
    struct message_s message;
    if(recv(client_sd, &message, sizeof(message), 0) >= 0){
        struct message_s *to_sendback = (struct message_s *)malloc(sizeof(struct message_s));
        switch(message.type){
            case 0xA1: 
                printf("Command: List\n");
                char *filelist = list_files("./data");

                to_sendback = make_header(0xA2, strlen(filelist)+1);
                if(send(client_sd, to_sendback, sizeof(struct message_s), 0) < 0){
                    printf("Send Error: %s (Errno:%d)\n", strerror(errno), errno);
                    exit(0);
                };

                if(sendn(client_sd, filelist, strlen(filelist)+1) < 0){
                    printf("Send Error: %s (Errno:%d)\n", strerror(errno), errno);
                    exit(0);
                };

                free(filelist);
                break;

            case 0xB1:
                printf("Command: Get\n");   

                // receive get request
                int get_filenamelen = ntohl(message.length) - 10;
                char getfileName[200];
                char fileName[200];
                *getfileName = '\0';
                *fileName = '\0';

                if(recvn(client_sd, getfileName, get_filenamelen) < 0){
                    printf("Receive Error: %s (Errno:%d)\n", strerror(errno), errno);
                    exit(0);
                }; 

                // receive file data
                long fsize = 0;
                FILE *fp = NULL;
                strcat(fileName, "./data/");
                strcat(fileName, getfileName);
                fp = fopen(fileName, "rb");
                if(fp == NULL){
                    printf("File not exists\n");
                    struct message_s getnofile;
                    getnofile = replymessage(0xB3, 0);
                    if((send(client_sd, &getnofile, sizeof(getnofile),0))<0){
			            printf("Send Error: %s (Errno:%d)\n",strerror(errno),errno);
			            exit(0);
		            }
                }else{
                    struct message_s getfilereply;
                    getfilereply = replymessage(0xB2, 0);
                    if((send(client_sd, &getfilereply, sizeof(getfilereply),0))<0){
			            printf("Send Error: %s (Errno:%d)\n",strerror(errno),errno);
			            exit(0);
		            }

                    // find file size
                    fseek(fp, 0, SEEK_END);
                    fsize = ftell(fp);
                    fseek(fp, 0, SEEK_SET);

                    char *filedata = (char *)malloc((fsize + 1) * sizeof(char));
                    *filedata = '\0';
                    fread(filedata, fsize, 1, fp);
                    filedata[fsize] = '\0';
                    fclose(fp);

                    struct message_s filedata_mes = replymessage(0xFF, fsize);
                    if(send(client_sd, &filedata_mes, sizeof(filedata_mes), 0) <0){
                        printf("Send Error: %s (Errno:%d)\n",strerror(errno),errno);
                        exit(0);
                    }

                    sendn(client_sd, filedata, fsize);
                    free(filedata);
                }
                break;

            case 0xC1:
                printf("Command: Put\n");
                // receive put request
                char filename[200];
                char recfilename[200];
                int len;
	            filename[0] = '\0';
                recfilename[0] = '\0';
                strcat(filename, "./data/");
                
                int namelength = ntohl(message.length) - 10;
                recvn(client_sd, recfilename, namelength);
                strcat(filename, recfilename);
                printf("Ready to receive file %s\n", filename);

                struct message_s putreply;
                putreply = replymessage(0xC2, 0);
                if((len=send(client_sd, &putreply , sizeof(putreply),0))<0){
		            printf("Send Error: %s (Errno:%d)\n",strerror(errno),errno);
		            exit(0);
	            }

                // receive file data
                unsigned char *buff[CHUNK_SIZE];
                fp = NULL;
                fp = fopen(filename, "wb");
                if((len=recv(client_sd, buff,sizeof(struct message_s),0))<0){
		            printf("Receive Error: %s (Errno:%d)\n", strerror(errno),errno);
		            exit(0);
	            }

                struct message_s *filedatamessage = malloc(sizeof(struct message_s));
	            memcpy(filedatamessage,buff,10);
	            // printf("Header: 0x%02x, 0x%08x\n", filedatamessage->type, filedatamessage->length);

                fsize = 0;
                fsize = ntohl(filedatamessage->length) - 10;
                printf("receive file size %u\n", fsize);
                while((len = recvn(client_sd, buff, fsize>CHUNK_SIZE?CHUNK_SIZE:fsize)) > 0){
                    fwrite (buff , sizeof(char), fsize>CHUNK_SIZE?CHUNK_SIZE:fsize, fp);
                    fsize -= CHUNK_SIZE;
                    if(fsize <= 0){
                        break;
                    }
	            }

                fclose(fp);
                close(client_sd);
                break;
        }
    }
    pthread_exit(NULL);
}

int main(int argc, char** args){
    char* serverPort = args[1];
    int num_thread = 0;

    int sd = socket(AF_INET, SOCK_STREAM, 0);

    long val = 1;
    if(setsockopt(sd, SOL_SOCKET, SO_REUSEADDR, &val, sizeof(long)) == -1){
        perror("setsockopt");
        exit(0);
    }
    
    int client_sd;
    struct sockaddr_in server_addr;
    struct sockaddr_in client_addr;
    memset(&server_addr, 0, sizeof(server_addr));

    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    server_addr.sin_port = htons(atoi(serverPort));

    if(bind(sd, (struct sockaddr *) &server_addr, sizeof(server_addr)) < 0){
        printf("Bind Error: %s (Errno:%d)\n", strerror(errno), errno);
        exit(0);
    }

    if(listen(sd, 10) < 0){
        printf("Listen Error: %s (Errno:%d)\n", strerror(errno), errno);
        exit(0);
    }
    
    // server handle multiple request
    while(1){
        int addr_len = sizeof(client_addr);
        if((client_sd = accept(sd, (struct sockaddr *) &client_addr, &addr_len)) < 0){
            printf("Accept Error: %s (Errno:%d)\n", strerror(errno), errno);
            exit(0);
        }
            pthread_t thread;
            int rc = pthread_create(&thread, NULL, connection_handler, (void*)client_sd);
            num_thread++;
    }

    close(sd);
    return 0;
}
