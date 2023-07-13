#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <net/if.h>
#include <net/if_dl.h>
#include <net/route.h>
#include <unistd.h>
#include <poll.h>
#include <errno.h>
#include <err.h>
#include <fcntl.h>

uint8_t rtmsgbuff[sizeof(struct rt_msghdr) + sizeof(struct if_msghdr)];

int main(int argc, const char ** argv) {
	if (argc != 3) {
		puts("Usage: ifmtuset (iface_name) (target_mtu)");
		return 1;
	}

	const char * targetifnam = argv[1];
	uint16_t target_mtu;
	if (!sscanf(argv[2], "%hu", &target_mtu)) {
		puts("Invalid MTU number");
		return 1;
	}

	// get interface ID
	int ifidx = if_nametoindex(targetifnam);
	if (!ifidx) {
		err(1, "Error getting interface name");
	}

	printf("Watching for '%s' and setting MTU to %hu\n", targetifnam, target_mtu);

	// socket to monitor network interface changes
	int rtfd = socket(AF_ROUTE, SOCK_RAW, 0);
	if (rtfd < 0) {
		err(1, "Error creating AF_ROUTE socket");
	}
	if (fcntl(rtfd, F_SETFL, O_NONBLOCK) < 0) {
		err(1, "Error setting nonblock on AF_ROUTE socket");
	}

	// socket to perform ioctl to set interface values
	int iocfd = socket(AF_INET, SOCK_DGRAM, 0);
	if (iocfd < 0) {
		err(1, "Error creating AF_INET socket");
	}

	struct ifreq ifr = {0};
	strlcpy(ifr.ifr_name, targetifnam, IFNAMSIZ);
	ifr.ifr_mtu = target_mtu;

	struct pollfd prt;
	prt.fd = rtfd;
	prt.events = POLLIN;
	for(;;) {
		if (poll(&prt, 1, -1) < 0) {
			if (errno == EINTR) {
				continue;
			}
			err(1, "Error polling AF_ROUTE socket");
		}

		// for all queued reads, we take the final value flag and do SIOCSIFFLAGS once
		uint32_t read_mtu = target_mtu;
		for(ssize_t len = 0;;) {
			len = read(rtfd, rtmsgbuff, sizeof(rtmsgbuff));
			if (len < 0) {
				if (errno == EINTR) {
					continue;
				} else if (errno == EAGAIN) {
					break;
				}
				err(1, "Error reading AF_ROUTE socket");
			}

			struct rt_msghdr * rtmsg = (void *)rtmsgbuff;
			if (rtmsg->rtm_type != RTM_IFINFO) {
				continue;
			}

			struct if_msghdr * ifmsg = (void *)rtmsg;
			if (ifmsg->ifm_index != ifidx) {
				// not the interface that we want
				continue;
			}

			read_mtu = ifmsg->ifm_data.ifi_mtu;
		}

		if (read_mtu != target_mtu) {
			printf("%s MTU = %u. Changing to %hu.\n", targetifnam, read_mtu, target_mtu);
			if (ioctl(iocfd, SIOCSIFMTU, &ifr) < 0) {
				err(1, "Error setting interface MTU");
			}
		}
	}
}
