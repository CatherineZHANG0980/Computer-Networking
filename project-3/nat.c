#include "checksum.h"
#include "nat.h"

mynat* mappingTable;
bool port[2001] = {false};
unsigned int subnet_mask;
int bucket_size;
int fill_rate;
struct nfq_data **packet;
int count;
int head;
int tail;
int token_num;
uint32_t p_ip;
uint32_t i_ip;
pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;

static void * translationProcess(void *args){

  thread_tran_args *tran_tinfo = (thread_tran_args*) args;

  struct nfq_q_handle *myQueue = tran_tinfo->tinfo_myQueue;
  struct timeval token_time, new_token_time;
  struct timespec tim1, tim2;
  double time;
  // int is_send = 0; // not send
  double bucket = (double)bucket_size;
  tim1.tv_sec = 0;
  tim1.tv_nsec = 5000;

  gettimeofday(&token_time, NULL);

  while(1){
    while(1){
      if(bucket >= 1){
        //TODO verdict
        if(count != 0){
          // printf("count: %d\n", count);
          // printf("head: %d\n", head);
          // printf("tail: %d\n", tail);
          pthread_mutex_lock(&lock);
          struct nfq_data *pkt = packet[head];

          count--;
          head = (head + 1) % 10;
          struct  timeval tv;
          gettimeofday(&tv, NULL);
          long int sec = tv.tv_sec;
          long int usec = tv.tv_usec;

          unsigned int id = 0;

          struct nfqnl_msg_packet_hdr *pkt_header;

          pkt_header = nfq_get_msg_packet_hdr(pkt);
          if (pkt_header) {
            id = ntohl(pkt_header->packet_id);
          }

          // Access IP Packet
          unsigned char *pktData;
          int ip_pkt_len = nfq_get_payload(pkt, &pktData);
          struct iphdr *ipHeader = (struct iphdr *)pktData;

          if (ipHeader->protocol != IPPROTO_UDP) {
            fprintf(stderr, "hacknfqueue does not verdict for non-udp packet! Please implement by yourself!");
            exit(-1);
          }

          // Access UDP Packet
          struct udphdr *udph = (struct udphdr *) (((char*)ipHeader) + ipHeader->ihl*4);

          mynat *prev_ptr = NULL;
          mynat *ptr = mappingTable;
          // remove all expired entries
          // check if anything is removed
          bool expired = false;
          while(ptr != NULL){
            if(sec - ptr->sec > 10 || (sec - ptr->sec == 10 && usec - ptr->usec > 0)){
              // expired
              expired = true;
              port[ptr->wan_port - 10000] = false;
              if(!prev_ptr){
                mappingTable = ptr->next;
                prev_ptr = NULL;
                ptr = mappingTable;
                continue;
              }else{
                prev_ptr->next = ptr->next;
              }
            }
            prev_ptr = ptr;
            ptr = ptr->next;
          }

          if(expired){
            // print the table
            // printf("Print for removing some expired entries\n");
            show_nat(mappingTable);
          }

          // check if there exists an entry for out/inbound
          bool matched = false;
          if((ntohl(ipHeader->saddr) & subnet_mask) != (ntohl(i_ip) & subnet_mask)){
            // inbound
            prev_ptr = NULL;
            ptr = mappingTable;

            while(ptr != NULL){
              if(ptr->wan_port == ntohs(udph->dest)){
                // match
                matched = true;
                // printf("inbound match\n");
                break;
              };
              prev_ptr = ptr;
              ptr = ptr->next;
            }

            // if match or not
            if(matched){
              ptr->sec = sec;
              ptr->usec = usec;

              // printf("----------------------------------------\n");
              // printf("Before: (");
              // print_ip(ipHeader->saddr);
              // printf(", %05u) => (", udph->source);
              // print_ip(ipHeader->daddr);
              // printf(", %05u)\n", udph->dest);
              // show_checksum((unsigned char*) ipHeader, 1);

              ipHeader->daddr = ptr->lan_ip;
              udph->dest = htons(ptr->lan_port);
              udph->check = udp_checksum((unsigned char*) ipHeader);
              ipHeader->check = ip_checksum((unsigned char*) ipHeader);

              // printf("After:  (");
              // print_ip(ipHeader->saddr);
              // printf(", %05u) => (", udph->source);
              // print_ip(ipHeader->daddr);
              // printf(", %05u)\n", udph->dest);
              // show_checksum((unsigned char*) ipHeader, 1);
              // printf("----------------------------------------\n");
              nfq_set_verdict(myQueue, id, NF_ACCEPT, ip_pkt_len, pktData);
            }else{
              nfq_set_verdict(myQueue, id, NF_DROP, ip_pkt_len, pktData);
            }

          }else{
            // outbound
            prev_ptr = NULL;
            ptr = mappingTable;

            while(ptr != NULL){
              if(ptr->lan_ip == ipHeader->saddr && htons(ptr->lan_port) == udph->source){
                // match
                // printf("outbound match\n");
                matched = true;
                break;
              };
              prev_ptr = ptr;
              ptr = ptr->next;
            }

            if(!matched){
              // add new entry
              mynat *newNode = (mynat*)malloc(sizeof(mynat));
              newNode->sec = sec;
              newNode->usec = usec;
              newNode->lan_ip = ipHeader->saddr;
              newNode->lan_port = ntohs(udph->source);
              newNode->wan_ip = p_ip;
              // assign new->wan_port
              for(int i = 0; i < 2001; i++){
                if(port[i] == false){
                  port[i] = true;
                  newNode->wan_port = i + 10000;
                  break;
                }
              }
              newNode->next = mappingTable;
              mappingTable = newNode;

              // modify header
              // printf("----------------------------------------\n");
              // printf("Before: (");
              // print_ip(ipHeader->saddr);
              // printf(", %05u) => (", udph->source);
              // print_ip(ipHeader->daddr);
              // printf(", %05u)\n", udph->dest);
              // show_checksum((unsigned char*) ipHeader, 1);

              // ipHeader->saddr = newNode->wan_ip;
              ipHeader->saddr = p_ip;
              udph->source = htons(newNode->wan_port);
              udph->check = udp_checksum((unsigned char*) ipHeader);
              ipHeader->check = ip_checksum((unsigned char*) ipHeader);

              // printf("After:  (");
              // print_ip(ipHeader->saddr);
              // printf(", %05u) => (", udph->source);
              // print_ip(ipHeader->daddr);
              // printf(", %05u)\n", udph->dest);
              // show_checksum((unsigned char*) ipHeader, 1);
              // printf("----------------------------------------\n");

              // print the table
              printf("Print for adding some entries\n");
              show_nat(mappingTable);
            }else{
              ptr->sec = sec;
              ptr->usec = usec;

              // modify header
              // printf("----------------------------------------\n");
              // printf("Before: (");
              // print_ip(ipHeader->saddr);
              // printf(", %05u) => (", udph->source);
              // print_ip(ipHeader->daddr);
              // printf(", %05u)\n", udph->dest);
              // show_checksum((unsigned char*) ipHeader, 1);

              ipHeader->saddr = ptr->wan_ip;
              ipHeader->saddr = p_ip;
              udph->source = htons(ptr->wan_port);
              udph->check = udp_checksum((unsigned char*) ipHeader);
              ipHeader->check = ip_checksum((unsigned char*) ipHeader);

              // printf("After:  (");
              // print_ip(ipHeader->saddr);
              // printf(", %05u) => (", udph->source);
              // print_ip(ipHeader->daddr);
              // printf(", %05u)\n", udph->dest);
              // show_checksum((unsigned char*) ipHeader, 1);
              // printf("----------------------------------------\n");
            }
            nfq_set_verdict(myQueue, id, NF_ACCEPT, ip_pkt_len, pktData);
          }
          pthread_mutex_unlock(&lock);
          bucket = bucket - 1.0;
          // printf("Consume 1 token: %f\n", bucket);
        }else{
          break;
        }
      }else{
        //not consume bucket
        if(nanosleep(&tim1, &tim2) < 0){
          printf("ERROR: nanosleep() failed!\n");
          exit(-1);
        }
        gettimeofday(&new_token_time , NULL);
        // printf("Before: Add some buckets: %f\n", bucket);
        time = (new_token_time.tv_sec - token_time.tv_sec) + (new_token_time.tv_usec - token_time.tv_usec)/1000000.0;
        // printf("time: %f\n", time);
        double token = bucket + time * fill_rate;
        bucket = bucket_size > token ? token : (double)bucket_size;
        // printf("After: Add some buckets: %f\n\n", bucket);
        token_time.tv_sec = new_token_time.tv_sec;
        token_time.tv_usec = new_token_time.tv_usec;
      }
    }
  }
}

static int Callback(struct nfq_q_handle *myQueue, struct nfgenmsg *msg,
    struct nfq_data *pkt, void *cbData) {
  
  // Get the id in the queue
  unsigned int id = 0;

  struct nfqnl_msg_packet_hdr *pkt_header;

  pkt_header = nfq_get_msg_packet_hdr(pkt);
  if (pkt_header) {
    id = ntohl(pkt_header->packet_id);
  }

  // Access IP Packet
  unsigned char *pktData;
  int ip_pkt_len = nfq_get_payload(pkt, &pktData);
  struct iphdr *ipHeader = (struct iphdr *)pktData;

  if (ipHeader->protocol != IPPROTO_UDP) {
    fprintf(stderr, "hacknfqueue does not verdict for non-udp packet! Please implement by yourself!");
    exit(-1);
  }

  // Access UDP Packet
  // struct udphdr *udph = (struct udphdr *) (((char*)ipHeader) + ipHeader->ihl*4);

  // int udp_header_len = 8;
  // int ip_header_len = ipHeader->ihl * 4;
  // int udp_payload_len = ip_pkt_len - udp_header_len - ip_header_len; // prompt: udp_payload_len + udp_header_len + ip_header_len = ip_pkt_len

  pthread_mutex_lock(&lock);
  if (count >= 10) {
    return nfq_set_verdict(myQueue, id, NF_DROP, ip_pkt_len, pktData);
  } else {
    packet[tail] = (struct nfq_data*)malloc(1500);
    memcpy(packet[tail], pkt, 1500);
    packet[tail] = pkt; // TODO: memcpy()
    tail += 1;
    tail %= 10;
    
    count++;
    // printf("count in t1: %d\n", count);
    // printf("head in t1: %d\n", head);
    // printf("tail in t1: %d\n", tail);
  }
  pthread_mutex_unlock(&lock);

  return 0;
}

static void * recvPacket(void *args){
  // struct nfq_handle *nfqHandle = (struct nfq_handle *)args;
  thread_args *tinfo = (thread_args*) args;
  int fd = tinfo->tinfo_fd;
  struct nfq_handle *nfqHandle = tinfo->tinfo_nfqHandle;
  int res;
  char buf[BUF_SIZE];
  // printf("THREAD\n");

  while ((res = recv(fd, buf, sizeof(buf), 0)) && res >= 0) {
    printf("res\n");
    nfq_handle_packet(nfqHandle, buf, res);
  }
  return NULL;
}

int main(int argc, char** argv){

  inet_pton(AF_INET, (const char*)argv[1], &p_ip);
  inet_pton(AF_INET, (const char*)argv[2], &i_ip);

  // initialization
  packet = (struct nfq_data **)malloc(10 * sizeof(struct nfq_data *));

  int i;
  for (i = 0; i < 10; i++) {
    packet[i] = (struct nfq_data *)malloc(sizeof(struct nfq_data *));
  }

  head = 0;
  tail = 0;
  count = 0;
  
  if (argc != 6)
	{
		fprintf(stderr, "Usage: ./nat <IP> <LAN> <MASK> <bucket size> <fill rate>\n");
		exit(-1);
	}
  int fd;
  pthread_t tid_rec;
  pthread_t tid_tran;
  
  struct nfq_handle *nfqHandle;
  struct nfnl_handle *netlinkHandle;
  struct nfq_q_handle *nfQueue;

  subnet_mask = 0xffffffff << (32 - atoi(argv[3]));

  bucket_size = atoi(argv[4]);
  fill_rate = atoi(argv[5]);

  // mappingTable = (mynat*) malloc (sizeof(mynat));
  mappingTable = NULL;

  // Get a queue connection handle from the module
  // struct nfq_handle *nfqHandle;
  if (!(nfqHandle = nfq_open())) {
    fprintf(stderr, "Error in nfq_open()\n");
    exit(-1);
  }

  // Unbind the handler from processing any IP packets
  if (nfq_unbind_pf(nfqHandle, AF_INET) < 0) {
    fprintf(stderr, "Error in nfq_unbind_pf()\n");
    exit(1);
  }

  // Install a callback on queue 0
  // struct nfq_q_handle *nfQueue;
  if (!(nfQueue = nfq_create_queue(nfqHandle,  0, &Callback, NULL))) {
    fprintf(stderr, "Error in nfq_create_queue()\n");
    exit(1);
  }

  // nfq_set_mode: I want the entire packet 
  if(nfq_set_mode(nfQueue, NFQNL_COPY_PACKET, BUF_SIZE) < 0) {
    fprintf(stderr, "Error in nfq_set_mode()\n");
    exit(1);
  }

  // struct nfnl_handle *netlinkHandle;
  netlinkHandle = nfq_nfnlh(nfqHandle);

  // int fd;
  fd = nfnl_fd(netlinkHandle);

  // int res;
  // char buf[BUF_SIZE];

  thread_args *tinfo = (thread_args*)malloc(sizeof(thread_args));
  tinfo->tinfo_nfqHandle = nfqHandle;
  tinfo->tinfo_fd = fd;

  thread_tran_args *tran_tinfo = (thread_tran_args*)malloc(sizeof(thread_tran_args));
  tran_tinfo->tinfo_myQueue = nfQueue;

  pthread_create(&tid_rec, NULL, &recvPacket, (void*) tinfo);
  pthread_create(&tid_tran, NULL, &translationProcess, (void*) tran_tinfo);

  pthread_join(tid_rec, NULL);
  pthread_join(tid_tran, NULL);

  nfq_destroy_queue(nfQueue);
  nfq_close(nfqHandle);
}