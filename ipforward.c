#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <netinet/ether.h>
#include <net/if.h>// struct ifreq
#include <sys/ioctl.h> // ioctl、SIOCGIFADDR
#include <sys/socket.h> // socket
#include <netinet/ether.h> // ETH_P_ALL
#include <netpacket/packet.h> // struct sockaddr_ll

#include <stdlib.h>

void set_misc(int sock_raw_fd,int misc,char *ethname)
{
	int ret;
	struct ifreq ifr;	//网络接口地址

	strncpy(ifr.ifr_name, ethname, IFNAMSIZ);		//指定网卡名称
	if(-1 == ioctl(sock_raw_fd, SIOCGIFFLAGS, &ifr)) {	//获取网络接口
		perror("ioctl");
		close(sock_raw_fd);
		exit(-1);
	}

	if (misc)
		ifr.ifr_flags |= IFF_PROMISC;
	else
		ifr.ifr_flags &= ~IFF_PROMISC;

	if (-1 == ioctl(sock_raw_fd, SIOCSIFFLAGS, &ifr)) {	//网卡设置混杂模式
		perror("ioctl");
		close(sock_raw_fd);
		exit(-1);
	}
}

int bind_eth(int skfd, char *ethname)
{
	struct sockaddr_ll sll;
	struct ifreq ifr;

	bzero(&sll, sizeof(sll));
	bzero(&ifr, sizeof(ifr));
	strcpy(ifr.ifr_name, ethname);

	//获取接口索引
	if (-1 == ioctl(skfd, SIOCGIFINDEX, &ifr)) {
		perror("get dev index error:");
		exit(1);
	}

	sll.sll_ifindex = ifr.ifr_ifindex;
	sll.sll_family = PF_PACKET;
	sll.sll_protocol = htons(ETH_P_ALL);

	return bind(skfd, (struct sockaddr*)&sll, sizeof(sll));
}

#define NIC_MAX	3
int main(int argc, char *argv[])
{
	int ret, i;
	struct sockaddr_ll sll[NIC_MAX];
	socklen_t len = sizeof(struct sockaddr_in);
	int fd[NIC_MAX];
	char *ethname[] = {"eth0", "eth1", "eth2"};
	char *buffer[2048];

	for (i = 0; i < NIC_MAX; i++) {
		fd[i] = socket(PF_PACKET, SOCK_RAW, htons(ETH_P_ALL));
		if(fd[i] < 0){
			printf("socket error\n");
			exit(-1);
		}

		set_misc(fd[i], 1, ethname[i]);
		ret = bind_eth(fd[i], ethname[i]);
		if(ret){
			perror("bind eth");
			return -1;
		}
	}

	while(1) {
		ret = read(fd[0], buffer, 2048);
		if(ret <= 0)
			continue;

		for (i = 1; i < NIC_MAX; i++) {
			ret = send(fd[i], buffer, ret, 0);
			if (ret == -1) {
				perror("sendto");
				continue;
			}
		}
	}


	for (i = 0; i < NIC_MAX; i++) {
		set_misc(fd[i], 0, ethname[i]);
		close(fd[i]);
	}

	return 0;
}
