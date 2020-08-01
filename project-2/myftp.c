#include "myftp.h"

uint8_t *encode_data(int n, int k, Stripe *stripe, int block_size){
    uint8_t *encode_matrix = malloc(sizeof(uint8_t) * (n * k));
    uint8_t *table = malloc(sizeof(uint8_t) * (32 * k * (n - k)));
    gf_gen_rs_matrix(encode_matrix, n, k);
    ec_init_tables(k, n - k, &encode_matrix[k * k], table);
    // int i;
    // for(i = 0; i < 32 * k * (n - k); i++){
    //     if((i + 1) % 32 == 0){
    //         printf("\n");
    //     }
    // }
    ec_encode_data(block_size, k, n - k, table, (unsigned char**)stripe->data_block, (unsigned char**)stripe->parity_block);
    return encode_matrix;
}

unsigned char *decode_data_returned(int n, int k, Stripe *stripe, int block_size, int stripeNum, int numOfStripe, long filesize){
    uint8_t *encode_matrix = malloc(sizeof(uint8_t) * (n * k));
    uint8_t *errors_matrix = malloc(sizeof(uint8_t) * (k * k));
    uint8_t *invert_matrix = malloc(sizeof(uint8_t) * (k * k));
    uint8_t *decode_matrix = malloc(sizeof(uint8_t) * (k * k));
    uint8_t *table = malloc(sizeof(uint8_t) * (32 * k * (n - k)));
    // unsigned char *decoded_result = malloc(block_size * k);

    // genetare generator matrix
    gf_gen_rs_matrix(encode_matrix, n, k);

    // get working nodes id
    int i, j, t;
    j = 0;
    int *work_nodes = malloc(sizeof(int) * k);
    for(i = 0; i < n; i++){
        if(i < k){
            if(stripe->data_block[i] != NULL){
                work_nodes[j] = i;
                j++;
            }
        }else{
            if(stripe->parity_block[i - k] != NULL){
                work_nodes[j] = i;
                j++;
            }
        }
        if(j == k) break;
    }

    // print working nodes
    printf("\nworking nodes: ");
    for(i = 0; i < k; i++){
        printf("%d ", work_nodes[i]);
    }
    printf("\n");
    

    // check if there are enough blocks
    if(j < k){
        printf("Not able to decode\n");
        return "";
    }else{

        // remove failed blocks to make error matrix
        for(i = 0; i < k; i++){
            int r = work_nodes[i];
            for(j = 0; j < k; j++){
                errors_matrix[k * i + j] = encode_matrix[k * r + j];
            }
        }

        gf_invert_matrix(errors_matrix, invert_matrix, k);

        // get all not working nodes
        int *node_status = malloc(sizeof(int) * n);
        for(i = 0; i < n; i++){
            node_status[i] = 0;
        }
        for(i = 0; i < k; i++){
            node_status[work_nodes[i]] = 1;
        }
        // e.g. n = 5, k = 3, [1, 1, 0, 0, 1] -> data_block = [1, 1, 0], parity_block = [0, 1]

        // print node status
        printf("\nnode status: \n");
        for(i = 0; i < n; i++){
            printf("%d ", node_status[i]);
        }
        printf("\n");
        //

        // making decode matrix
        t = 0;
        for(i = 0; i < k; i++){
            if(node_status[i] == 0){
                for(j = 0; j < k; j++){
                    // printf("(%d, %d): %d \n", t, j, invert_matrix[k * i + j]);
                    decode_matrix[k * t + j] = invert_matrix[k * i + j];
                }
                t++;
            }
        }

        // initialize a decode table
        ec_init_tables(k, n - k, decode_matrix, table);

        // seperate available blocks and failed blocks
        unsigned char **available_blocks = malloc(sizeof(int*) * k);
        unsigned char **failed_blocks = malloc(sizeof(int*) * (n - k));
        int countAvailable = 0;
        int countFailed = 0;
        for(i = 0; i < n; i++){
            if(node_status[i] == 1){
                // available node
                available_blocks[countAvailable] = malloc(block_size);
                if(i < k){
                    available_blocks[countAvailable] = stripe->data_block[i];
                }else{
                    available_blocks[countAvailable] = stripe->parity_block[i - k];
                }
                // printf("%d, available: %s\n", i, available_blocks[countAvailable]);
                countAvailable++;
            }else{
                // failed node
                failed_blocks[countFailed] = malloc(block_size);
                countFailed++;
            }
        }

        ec_encode_data(block_size, k, n - k, table, available_blocks, failed_blocks);

        // // print available blocks
        // printf("\navailable blocks: \n");
        // for(i = 0; i < k; i++){
        //     printf("%s\n", available_blocks[i]);
        // }
        // //

        // // print failed blocks
        // printf("\nfailed blocks: \n");
        // for(i = 0; i < n - k; i++){
        //     printf("%s\n", failed_blocks[i]);
        // }
        // //

        // combine the result
        countAvailable = 0;
        countFailed = 0;
        // printf("decode done\n");
        int remainSize = filesize % (block_size * k); // 0
        int lastSize = filesize % block_size; // check full or not // 0
        int remainBlock = floor(remainSize / block_size); // 0 // TODO double
        printf("remainSize: %d, lastSize: %d, remainBlock: %d\n", remainSize, lastSize, remainBlock);
        unsigned char *decoded_result;
        if(stripeNum == numOfStripe - 1 && remainSize != 0){
            decoded_result = malloc(remainSize);
        }else{
            decoded_result = malloc(block_size * k);
        }

        if(stripeNum == numOfStripe - 1 && remainSize != 0){
            // last stripe
            printf("1\n");
            for(i = 0; i <= remainBlock; i++){
                printf("2\n");
                if(i == remainBlock){
                    printf("3\n");
                    if(node_status[i] == 1){
                        printf("4\n");
                        for(j = 0; j < lastSize; j++){
                            decoded_result[i * block_size + j] = available_blocks[countAvailable][j];
                        }
                        countAvailable++;
                    }else{
                        printf("5\n");
                        for(j = 0; j < lastSize; j++){
                            decoded_result[i * block_size + j] = failed_blocks[countFailed][j];
                        }
                        countFailed++;
                    }
                }else{
                    printf("6\n");
                    if(node_status[i] == 1){
                        printf("7\n");
                        for(j = 0; j < block_size; j++){
                            decoded_result[i * block_size + j] = available_blocks[countAvailable][j];
                        }
                        countAvailable++;
                    }else{
                        printf("8\n");
                        for(j = 0; j < block_size; j++){
                            decoded_result[i * block_size + j] = failed_blocks[countFailed][j];
                        }
                        countFailed++;
                    }
                }
            }
        }else{
            printf("9\n");
            for(i = 0; i < k; i++){
                printf("10\n");
                if(node_status[i] == 1){
                    printf("11\n");
                    for(j = 0; j < block_size; j++){
                        decoded_result[i * block_size + j] = available_blocks[countAvailable][j];
                    }
                    countAvailable++;
                }else{
                    printf("12\n");
                    for(j = 0; j < block_size; j++){
                        decoded_result[i * block_size + j] = failed_blocks[countFailed][j];
                    }
                    countFailed++;
                }
            }
        }
        
        printf("result given\n");
        return decoded_result;
    }
}

void decode_data(int n, int k, Stripe *stripe, int block_size, unsigned char *decoded_result){
    uint8_t *encode_matrix = malloc(sizeof(uint8_t) * (n * k));
    uint8_t *errors_matrix = malloc(sizeof(uint8_t) * (k * k));
    uint8_t *invert_matrix = malloc(sizeof(uint8_t) * (k * k));
    uint8_t *decode_matrix = malloc(sizeof(uint8_t) * (k * k));
    uint8_t *table = malloc(sizeof(uint8_t) * (32 * k * (n - k)));

    // genetare generator matrix
    gf_gen_rs_matrix(encode_matrix, n, k);

    // get working nodes id
    int i, j, t;
    j = 0;
    int *work_nodes = malloc(sizeof(int) * k);
    for(i = 0; i < n; i++){
        if(i < k){
            if(stripe->data_block[i] != NULL){
                work_nodes[j] = i;
                j++;
            }
        }else{
            if(stripe->parity_block[i - k] != NULL){
                work_nodes[j] = i;
                j++;
            }
        }
        if(j == k) break;
    }

    // print working nodes
    printf("\nworking nodes: ");
    for(i = 0; i < k; i++){
        printf("%d ", work_nodes[i]);
    }
    printf("\n");
    //

    // check if there are enough blocks
    if(j < k){
        printf("Not able to decode\n");
        return;
    }else{

        // remove failed blocks to make error matrix
        for(i = 0; i < k; i++){
            int r = work_nodes[i];
            for(j = 0; j < k; j++){
                errors_matrix[k * i + j] = encode_matrix[k * r + j];
            }
        }

        gf_invert_matrix(errors_matrix, invert_matrix, k);

        // get all not working nodes
        int *node_status = malloc(sizeof(int) * n);
        for(i = 0; i < n; i++){
            node_status[i] = 0;
        }
        for(i = 0; i < k; i++){
            node_status[work_nodes[i]] = 1;
        }
        // e.g. n = 5, k = 3, [1, 1, 0, 0, 1] -> data_block = [1, 1, 0], parity_block = [0, 1]

        // print node status
        printf("\nnode status: \n");
        for(i = 0; i < n; i++){
            printf("%d ", node_status[i]);
        }
        printf("\n");
        //

        // making decode matrix
        t = 0;
        for(i = 0; i < k; i++){
            if(node_status[i] == 0){
                for(j = 0; j < k; j++){
                    // printf("(%d, %d): %d \n", t, j, invert_matrix[k * i + j]);
                    decode_matrix[k * t + j] = invert_matrix[k * i + j];
                }
                t++;
            }
        }

        // initialize a decode table
        ec_init_tables(k, n - k, decode_matrix, table);

        // seperate available blocks and failed blocks
        unsigned char **available_blocks = malloc(sizeof(int*) * k);
        unsigned char **failed_blocks = malloc(sizeof(int*) * (n - k));
        int countAvailable = 0;
        int countFailed = 0;
        for(i = 0; i < n; i++){
            if(node_status[i] == 1){
                // available node
                available_blocks[countAvailable] = malloc(block_size);
                if(i < k){
                    available_blocks[countAvailable] = stripe->data_block[i];
                }else{
                    available_blocks[countAvailable] = stripe->parity_block[i - k];
                }
                countAvailable++;
            }else{
                // failed node
                failed_blocks[countFailed] = malloc(block_size);
                countFailed++;
            }
        }

        ec_encode_data(block_size, k, n - k, table, available_blocks, failed_blocks);

        // print available blocks
        printf("\navailable blocks: \n");
        for(i = 0; i < k; i++){
            printf("%s\n", available_blocks[i]);
        }
        //

        // print failed blocks
        printf("\nfailed blocks: \n");
        for(i = 0; i < n - k; i++){
            printf("%s\n", failed_blocks[i]);
        }
        //

        // combine the result
        countAvailable = 0;
        countFailed = 0;
        printf("decode done\n");
        for(i = 0; i < k; i++){
            printf("assigning %d\n", i);
            if(node_status[i] == 1){
                for(j = 0; j < block_size; j++){
                    decoded_result[i * block_size + j] = available_blocks[countAvailable][j];
                }
                countAvailable++;
            }else{
                for(j = 0; j < block_size; j++){
                    decoded_result[i * block_size + j] = failed_blocks[countFailed][j];
                }
                countFailed++;
            }
        }
        printf("result given\n");
    }
}

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
