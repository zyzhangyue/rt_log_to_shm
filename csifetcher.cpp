#include "csifetcher.h"
#include "csiparser.h"
#include "iwl_connector.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
#include <unistd.h>
#include <sched.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <linux/netlink.h>

/* Local variables */
static int sock_fd = -1;
static struct sockaddr_nl proc_addr;
static struct sockaddr_nl kern_addr;	// addrs for recv, send, bind
static CSIParser *parser = nullptr;

void exit_program_err()
{
    if (sock_fd != -1)
    {
        close(sock_fd);
        sock_fd = -1;
    }
    if (parser != nullptr) {
        delete parser;
    }
}

bool csi_driver_init(void)
{
    /* Setup the socket */
    sock_fd = socket(PF_NETLINK, SOCK_DGRAM, NETLINK_CONNECTOR);

    if (sock_fd == -1) {
        fprintf(stderr, "Error creating socket: %s\n", strerror(errno));
        return false;
    }

    /* Initialize the address structs */
    memset(&proc_addr, 0, sizeof(struct sockaddr_nl));
    proc_addr.nl_family = AF_NETLINK;
    proc_addr.nl_pid = getpid();			// this process' PID
    proc_addr.nl_groups = CN_IDX_IWLAGN;
    memset(&kern_addr, 0, sizeof(struct sockaddr_nl));
    kern_addr.nl_family = AF_NETLINK;
    kern_addr.nl_pid = 0;					// kernel
    kern_addr.nl_groups = CN_IDX_IWLAGN;

    /* Now bind the socket */
    if (bind(sock_fd, (struct sockaddr *)&proc_addr, sizeof(struct sockaddr_nl)) < 0) {
        fprintf(stderr, "Error binding socket: %s\n", strerror(errno));
        exit_program_err();
        return false;
    }

    /* And subscribe to netlink group */
    {
        int on = proc_addr.nl_groups;
        if (setsockopt(sock_fd, 270, NETLINK_ADD_MEMBERSHIP, &on, sizeof(on)) < 0) {
            fprintf(stderr, "Error setsocketopt: %s\n", strerror(errno));
            exit_program_err();
            return false;
        }
    }

    /* RT scheduler */
    struct sched_param param;
    param.sched_priority = 1;
    if (sched_setscheduler( getpid(), SCHED_FIFO, &param) < 0) {
        fprintf(stderr, "Error setting scheduler: %s\n", strerror(errno));
        exit_program_err();
        return false;
    }
    parser = new CSIParser();
    return true;
}

bool get_csi_from_driver(csi_packet *packet)
{
    static uint8_t buffer[1024], buf[1024];
    static struct cn_msg *cmsg;

    if (recv(sock_fd, buf, 215, 0) < 0) {
        exit_program_err();
        return false;
    }

    cmsg = (struct cn_msg*)NLMSG_DATA(buf);
    unsigned short len = cmsg->len;

    memcpy(buffer, &len, 2);
    memcpy(buffer+2, cmsg->data, len);
    return parser->parse_csi_from_buffer(packet, buffer);
}

csi_packet* get_all_csi_from_file(const char *filename, int *count)
{
    if (parser == nullptr) {
        parser = new CSIParser();
        fprintf(stderr, "CSIParser created\n");
    }
    return parser->parse_csi_from_file(filename, count);
}
