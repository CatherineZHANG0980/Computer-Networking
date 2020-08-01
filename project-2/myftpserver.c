# include "myftp.h"
int n = 0;
int k = 0;
int block_size = 0;
int id = 0;
int port_num = 0;

// char *list_files(char *root){
//     struct dirent *to_read;
//     DIR *dir = opendir(root);
//     // char *to_return = (char *)malloc(sizeof(char));
//     char to_return[500];
//     *to_return = '\0';

//     while((to_read = readdir(dir)) != NULL){
//         printf("read file name: %s\n", to_read->d_name);
//         if (strstr(to_read->d_name, "meta_") != NULL) {
//             // read file name contain "meta_"
//             continue;
//         }
//         // to_return = realloc(to_return, strlen(to_return) + strlen(to_read->d_name) + 1);
//         strcat(to_return, to_read->d_name);
//         strcat(to_return, "\n");
//         printf("to_return: %s\n", to_return);
//     }
//     return to_return;
// };

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
            {
                printf("Command: List\n");
                // char *filelist = list_files("./data");

                // list_files
                struct dirent *to_read;
                DIR *dir = opendir("./data");
                // char *to_return = (char *)malloc(sizeof(char));
                char to_return[500];
                *to_return = '\0';

                while((to_read = readdir(dir)) != NULL){
                    printf("read file name: %s\n", to_read->d_name);
                    if (strstr(to_read->d_name, "meta_") != NULL) {
                        // read file name contain "meta_"
                        continue;
                    }
                    // to_return = realloc(to_return, strlen(to_return) + strlen(to_read->d_name) + 1);
                    strcat(to_return, to_read->d_name);
                    strcat(to_return, "\n");
                    printf("to_return: %s\n", to_return);
                }

                printf("1\n");
                printf("file list: %s", to_return);
                printf("4");

                to_sendback = make_header(0xA2, strlen(to_return)+1);
                if(send(client_sd, to_sendback, sizeof(struct message_s), 0) < 0){
                    printf("Send Error: %s (Errno:%d)\n", strerror(errno), errno);
                    exit(0);
                };
                printf("2\n");

                if(sendn(client_sd, to_return, strlen(to_return)+1) < 0){
                    printf("Send Error: %s (Errno:%d)\n", strerror(errno), errno);
                    exit(0);
                };
                printf("3\n");
                break;
            }
            case 0xB1:
            {
                printf("Command: Get\n");   

                // receive get request
                int get_filenamelen = ntohl(message.length) - 10;
                char getfileName[300];
                char fileName[300];
                *getfileName = '\0';
                *fileName = '\0';

                if(recvn(client_sd, getfileName, get_filenamelen) < 0){
                    printf("Receive Error: %s (Errno:%d)\n", strerror(errno), errno);
                    exit(0);
                }; 
                printf("1\n");
                // receive file data
                long fsize = 0;
                FILE *fp = NULL;
                strcat(fileName, "./data/");
                strcat(fileName, getfileName);

                struct dirent *to_read;
                DIR *dir = opendir("./data");
                char *to_return = (char *)malloc(sizeof(char));

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
                    printf("2\n");
                    struct message_s getfilereply;
                    getfilereply = replymessage(0xB2, 0);
                    if((send(client_sd, &getfilereply, sizeof(getfilereply),0))<0){
			            printf("Send Error: %s (Errno:%d)\n",strerror(errno),errno);
			            exit(0);
		            }

                    // take metadata and return file size
                    char filename_meta[300];
                    strcat(filename_meta, "./data/meta_");
                    strcat(filename_meta, getfileName);
                    printf("meta file: %s\n", filename_meta);
                    FILE *fp_meta = fopen(filename_meta, "rb");
                    fseek(fp_meta, 0, SEEK_END);
                    int fsize_meta = ftell(fp_meta);
                    fseek(fp_meta, 0, SEEK_SET);
                    printf("3\n");
                    long real_fsize;
                    fscanf(fp_meta, "%ld", &real_fsize);
                    printf("long filesize_str: %ld", real_fsize);
                    // real_fsize = 1000000;
                    fclose(fp_meta);

                    // send file data

                    // LALALALA
                    fseek(fp, 0, SEEK_END);
                    fsize = ftell(fp);
                    fseek(fp, 0, SEEK_SET);
                    printf("4\n");

                    char *filedata = (char *)malloc((fsize + 1) * sizeof(char));
                    *filedata = '\0';
                    fread(filedata, fsize, 1, fp);
                    filedata[fsize] = '\0';
                    fclose(fp);
                    printf("5\n");

                    struct message_s filedata_mes;
                    filedata_mes = replymessage(0xFF, real_fsize);
                    if(send(client_sd, &filedata_mes, sizeof(filedata_mes), 0) <0){
                        printf("Send Error: %s (Errno:%d)\n",strerror(errno),errno);
                        exit(0);
                    }
                    printf("6\n");
                    int numOfStripe = ceil((double)real_fsize/ (block_size * k));
                    printf("numOfStripe: %d", numOfStripe);
                    long len = sendn(client_sd, filedata, (block_size * numOfStripe));
                    printf("len sent: %ld\n", len);
                    printf("7\n");
                    free(filedata);
                    // LALALALA
                }
                break;
            }
            case 0xC1:
            {
                printf("Command: Put\n");
                // receive put request
                char filename[300];
                char recfilename[300];
                char filename_meta[300];
                int len;
	            filename[0] = '\0';
                recfilename[0] = '\0';
                filename_meta[0] = '\0';
                // strcat(filename_meta, filename);
                // strcat("meta", filename_meta);
                strcat(filename_meta, "./data/"); // ./data/
                strcat(filename_meta, "meta_"); // ./data/meta_
                strcat(filename, "./data/");
                
                int namelength = ntohl(message.length) - 10;
                recvn(client_sd, recfilename, namelength);

                struct message_s putreply;
                putreply = replymessage(0xC2, 0);
                if((len=send(client_sd, &putreply , sizeof(putreply),0))<0){
		            printf("Send Error: %s (Errno:%d)\n",strerror(errno),errno);
		            exit(0);
	            }

                // receive file data
                unsigned char *buff[CHUNK_SIZE];
                if((len=recv(client_sd, buff,sizeof(struct message_s),0))<0){
		            printf("Receive Error: %s (Errno:%d)\n", strerror(errno),errno);
		            exit(0);
	            }
                // printf("Header: 0x%02x, 0x%08x\n", filedatamessage->type, filedatamessage->length);

                struct message_s *filedatamessage = malloc(sizeof(struct message_s));
	            memcpy(filedatamessage,buff,10);
	            printf("Header: 0x%02x, 0x%08x\n", filedatamessage->type, filedatamessage->length);

                long fsize = 0;
                fsize = ntohl(filedatamessage->length) - 10;
                strcat(filename, recfilename);
                printf("1 fs: %ld\n", fsize);

                printf("Ready to receive file %s\n", filename);

                //write metadata
                FILE *fp_meta = NULL;
                
                strcat(filename_meta, recfilename); // ./data/meta_filename
                fp_meta = fopen(filename_meta, "w+");
                // printf("2 fs: %ld\n", fsize);
                // printf("file_meta: %s\n", filename_meta);
                fprintf(fp_meta, "%ld", fsize);
                // printf("3 fs: %ld\n", fsize);
                // printf("Done writing\n");

                fclose(fp_meta);
                printf("4 fs: %ld\n", fsize);

                FILE *fp = NULL;
                fp = fopen(filename, "wb");
                printf("file name: %s\n", filename);
                printf("5 fs: %ld\n", fsize);

                int numOfStripe = ceil((double)fsize / (k * block_size));
                printf("6 fs: %ld\n", fsize);
                int to_receive_real_size = block_size * numOfStripe;
                
                printf("receive file size %ld\n", fsize);

                printf("count in block receive file size %ld\n", to_receive_real_size);
                while((len = recvn(client_sd, buff, to_receive_real_size>CHUNK_SIZE?CHUNK_SIZE:to_receive_real_size)) > 0){
                    fwrite (buff , sizeof(char), to_receive_real_size>CHUNK_SIZE?CHUNK_SIZE:to_receive_real_size, fp);
                    to_receive_real_size -= CHUNK_SIZE;
                    if(to_receive_real_size <= 0){
                        break;
                    }
	            }

                fclose(fp);
                close(client_sd);
                break;
            }
        }
    }
    pthread_exit(NULL);
}

int main(int argc, char** args){
    //read serverconfig.txt
    FILE *fp = NULL;
    fp = fopen(args[1], "r");
    fscanf(fp, "%d %d %d %d %d", &n, &k, &id, &block_size, &port_num);
    fclose(fp);

    int num_thread = 0;

    int sd = socket(AF_INET, SOCK_STREAM, 0);

    long val = 1;
    if(setsockopt(sd, SOL_SOCKET, SO_REUSEADDR, &val, sizeof(long)) == -1){
        perror("setsockopt");
        exit(0);
    }
    
    //set up socket
    int client_sd;
    struct sockaddr_in server_addr;
    struct sockaddr_in client_addr;
    memset(&server_addr, 0, sizeof(server_addr));

    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    server_addr.sin_port = htons(port_num);

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
