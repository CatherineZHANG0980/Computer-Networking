#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <netinet/in.h> // required by "netfilter.h"
#include <arpa/inet.h> // required by ntoh[s|l]()
#include <signal.h> // required by SIGINT
#include <string.h> // required by strerror()
#include <sys/time.h> // required by gettimeofday()
#include <time.h> // required by nanosleep()
#include <errno.h> // required by errno
#include <pthread.h>
#include <netinet/ip.h>        // required by "struct iph"
#include <netinet/tcp.h>    // required by "struct tcph"
#include <netinet/udp.h>    // required by "struct udph"
#include <netinet/ip_icmp.h>    // required by "struct icmphdr"

extern "C" {
#include <linux/netfilter.h> // required by NF_ACCEPT, NF_DROP, etc...
#include <libnetfilter_queue/libnetfilter_queue.h>
}

#define BUF_SIZE 1500
#define PKT_NUM 10

typedef struct mynat{
    unsigned int lan_ip;
    unsigned int wan_ip;
    unsigned short lan_port;
    unsigned short wan_port;
    long int sec;
    long int usec;
    struct mynat *next;
} mynat;

typedef struct thread_args{
    struct nfq_handle *tinfo_nfqHandle;
    int tinfo_fd;
} thread_args;

typedef struct thread_tran_args{
    struct nfq_q_handle *tinfo_myQueue;
} thread_tran_args;

void *traffic_shaping(void *args);

static int Callback(struct nfq_q_handle *myQueue, struct nfgenmsg *msg,
    struct nfq_data *pkt, void *cbData);

void print_ip(unsigned ip){
    char ip_addr[16];
    int len, pad, i=0;
    sprintf(ip_addr, "%u.%u.%u.%u", ip & 0xff, (ip >> 8) & 0xff,
			(ip >> 16) & 0xff, (ip >> 24) & 0xff);
    len = strlen(ip_addr);
    pad = 15 - len;
    for(i=0; i<pad ; i++){
        strcat(ip_addr, " ");
    }

    printf("%s", ip_addr);
}

void show_nat(mynat *nat){
    mynat *p = nat;
    printf("The NAT table is: \n");
    printf("\n| WAN  IP         | WAN  PORT | LAN  IP         | LAN  PORT | Timestamp\n");
    while(p!=NULL){
        printf("| ");
        print_ip(p->wan_ip);
        printf(" |   %05u   | ", p->wan_port);
        print_ip(p->lan_ip);
        printf(" |   %05u   | ", p->lan_port);
        printf("%ld.%6ld\n", p->sec, p->usec);
        p = p->next;
    }
    printf("\n");
}
