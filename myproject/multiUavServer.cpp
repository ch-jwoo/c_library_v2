//N:N matching
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <map>
#include <vector>
#include <algorithm>

#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <sys/time.h>
#include <common/mavlink.h>

// #define BUF_SIZE 300
#define GCS_PORT 20250
#define UAV_PORT 20200

void error_handling(const char *message);
uint8_t extract_sys_id(uint8_t* buf, ssize_t buf_size);


int main(int argc, char *argv[])
{
	int fd_max = 0;

	//TCP socket
	int tcp_sock;
	sockaddr_in tcp_adr;
	tcp_sock = socket(PF_INET, SOCK_STREAM, 0);
	if(tcp_sock == -1)
		error_handling("TCP socket creation error");
	memset(&tcp_adr, 0, sizeof(tcp_adr));
    tcp_adr.sin_family=AF_INET;
    tcp_adr.sin_addr.s_addr=htonl(INADDR_ANY);
    tcp_adr.sin_port=htons(GCS_PORT);
    if(bind(tcp_sock, (struct sockaddr*) &tcp_adr, sizeof(tcp_adr))==-1)
        error_handling("bind() error");
    if(listen(tcp_sock, 3)==-1)
        error_handling("listen() error");

	fd_set reads;
	FD_ZERO(&reads);
    FD_SET(tcp_sock, &reads);
	fd_max = tcp_sock;
	
	//UDP socket
	int udp_sock;
	sockaddr_in udp_adr;
	udp_sock = socket(PF_INET, SOCK_DGRAM, 0);
	if(udp_sock==-1)
		error_handling("UDP socket creation error");
	
	memset(&udp_adr, 0, sizeof(udp_adr));
	udp_adr.sin_family=AF_INET;
	udp_adr.sin_addr.s_addr=htonl(INADDR_ANY);
	udp_adr.sin_port=htons(UAV_PORT);
	if(bind(udp_sock, (struct sockaddr*)&udp_adr, sizeof(udp_adr))==-1)
		error_handling("bind() error");
	
    FD_SET(udp_sock, &reads);
    fd_max=udp_sock;
	if(fd_max < udp_sock)
		fd_max=udp_sock;

	
	//TCP and UDP client
	int tcp_clnt_sock;
	sockaddr_in tcp_clnt_adr;
	socklen_t tcp_clnt_adr_sz = sizeof(tcp_clnt_adr);

	int udp_clnt_sock;
	sockaddr_in udp_clnt_adr;
	socklen_t udp_clnt_adr_sz = sizeof(udp_clnt_adr);

	uint8_t buf[MAVLINK_MAX_PACKET_LEN];

	//for iteration
	fd_set cpy_reads;
	timeval timeout;
	int fd_num;
	
	//data saving
	std::map<uint8_t, sockaddr_in> uavInfo; // system_id : addr
	std::vector<int> gcsSocketList;
	while(true) 
	{
		cpy_reads = reads;
		timeout.tv_sec = 0;
		timeout.tv_usec = 0;
		if((fd_num=select(fd_max+1, &cpy_reads, 0, 0, &timeout))==-1)
			break;
		
		if(fd_num==0)
			continue;
		
		for(int i=0; i<fd_max+1; i++)
		{
			if(FD_ISSET(i, &cpy_reads))
			{
				if(i == tcp_sock)//gcs connect
				{
					tcp_clnt_sock=
						accept(tcp_sock, (struct sockaddr*)&tcp_clnt_adr, &tcp_clnt_adr_sz);
					if(tcp_clnt_sock == -1){
						fputs("making connection fail\n", stderr);
						continue;
					}
					gcsSocketList.push_back(tcp_clnt_sock);
					FD_SET(tcp_clnt_sock, &reads);
					if(fd_max < tcp_clnt_sock)
						fd_max = tcp_clnt_sock;
					printf("connected client: %d \n", tcp_clnt_sock);
				}
				else if(i == udp_sock)//message from UAV
				{
					//udp recvfrom
					ssize_t n = recvfrom(udp_sock, buf, sizeof(buf), 0, 
											(struct sockaddr*)&udp_clnt_adr, &udp_clnt_adr_sz);
					if(n == -1){
						fputs("udp recvfrom fail\n", stderr);
						continue;
					}
					//tcp write
					auto iter = gcsSocketList.begin();
					for(; iter < gcsSocketList.end(); iter++){
						if(write(*iter, buf, n)==-1){
							fputc(*iter, stderr);
							fputs(" : tcp write fail\n", stderr);
							continue;
						}
					}
					// if(write(tcp_clnt_sock, buf, str_len)==-1){
					// 	fputs("tcp write fail\n", stderr);
					// 	continue;
					// }
				}
				else//tcp client, message from GCS
				{
					//tcp read
					ssize_t n = read(i, buf, sizeof(buf));
					if(n == 0)//connection terminate
					{
						std::vector<int>::iterator iter;
						iter = std::find(gcsSocketList.begin(), gcsSocketList.end(), i);
						gcsSocketList.erase(iter);
						FD_CLR(i, &reads);
						close(i);
						printf("closed tcp client: %d \n", i);
					}
					//udp sendto
					else{
						uint8_t sys_id = extract_sys_id(buf, n);
						if(sys_id = 0){//broadcasting
							for(auto uav : uavInfo)
							{
								if(sendto(udp_sock, buf, n, 0, 
											(struct sockaddr*)&uav, udp_clnt_adr_sz) == -1){
									fputs("udp sendto fail\n", stderr);
									continue;
								}
							}
						}
						//unicast
						else if(sendto(udp_sock, buf, n, 0, 
										(struct sockaddr*)&uavInfo[sys_id], udp_clnt_adr_sz) == -1){
							fputs("udp sendto fail\n", stderr);
							continue;
						}
					}
				}
			}
		}
	}	
	close(tcp_sock);
	close(udp_sock);
	return 0;
}

void error_handling(const char *message)
{
	fputs(message, stderr);
	fputc('\n', stderr);
	exit(1);
}

uint8_t extract_sys_id(uint8_t* buf, ssize_t buf_size)
{
	mavlink_message_t msg;
	mavlink_status_t status;
	for (int i = 0; i < buf_size; i++) {
		if (mavlink_parse_char(MAVLINK_COMM_0, buf[i], &msg, &status)) {
			// handle_new_message(&msg);
		}
	}
	uint8_t* ptr = (uint8_t*)&msg.payload64;
	const mavlink_msg_entry_t* payload_ofs;
	payload_ofs = mavlink_get_msg_entry(msg.msgid);
	if(payload_ofs->flags != MAV_MSG_ENTRY_FLAG_HAVE_TARGET_SYSTEM)
	{
		return 0;
	}
	return *(ptr+payload_ofs->target_system_ofs);
	// msg.payload64
	// return result->target_system_ofs;
}