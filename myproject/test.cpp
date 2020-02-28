#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>

#include <common/mavlink.h>

static int fd;
static struct sockaddr_in sockaddr;

static timespec auth_request_timeout;
static timespec heartbeat_timeout;

static uint8_t target_system;

int main(int argc, char *argv[])
{
    const char *ip = "127.0.0.1";
    int port = 5760;
    
    fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd == -1) {
        fprintf(stderr, "Could not create socket (%m)\n");
        return 1;
    }

    memset(&sockaddr, 0, sizeof(struct sockaddr_in));
    sockaddr.sin_family = AF_INET;
    sockaddr.sin_addr.s_addr = inet_addr(ip);
    sockaddr.sin_port = htons(port);

    int ret = connect(fd, (struct sockaddr *)&sockaddr, sizeof(struct sockaddr_in));
    if (ret) {
        fprintf(stderr, "Could not connect to socket (%m)\n");
        goto connect_error;
    }
    
    mavlink_message_t msg;
    
    // setup_signal_handlers();
    // loop();

    return 0;

connect_error:
    close(fd);
    return -1;
}