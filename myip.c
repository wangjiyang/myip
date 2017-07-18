#include <stdio.h>
#include <sys/types.h>          /* See NOTES */
#include <sys/socket.h>
#include <netinet/ip.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/epoll.h>
#include <arpa/inet.h>

int main() {
	int fd;
	struct sockaddr_in myaddr;
	int flags;
	int ret;

	fd = socket(AF_INET, SOCK_STREAM, 0);
	if (fd < 0) {
		fprintf(stderr, "create socket failed(%s)\n", strerror(errno));
		return -1;
	}

	flags = fcntl(fd, F_GETFL, 0);                     
	ret = fcntl(fd, F_SETFL, flags | O_NONBLOCK);      
	if (ret < 0) {                                           
		fprintf(stderr, "set socket to non-blocking failed(%s)\n",     
				strerror(errno));                            
		close(fd);                                     
		return -1;                                           
	}                                                        

	myaddr.sin_family = AF_INET;
	myaddr.sin_addr.s_addr = htonl(INADDR_ANY);
	myaddr.sin_port = htons(1140);

	if (bind(fd, (struct sockaddr *)&myaddr, sizeof(struct sockaddr_in)) < 0) {
		fprintf(stderr, "bind socket failed(%s)\n", strerror(errno));
		close(fd);
		return -1;
	}

	if (listen(fd, 1024) < 0) {
		fprintf(stderr, "listen failed(%s)\n", strerror(errno));
		close(fd);
		return -1;
	}

#define MAX_EVENTS 16
	struct epoll_event events[MAX_EVENTS];

	int epollfd = epoll_create(MAX_EVENTS);
	if (epollfd < 0) {
		fprintf(stderr, "create epoll failed(%s)\n", strerror(errno));
		close(fd);
		return -1;
	}

	struct epoll_event ev;
	ev.events = EPOLLIN;
	ev.data.fd = fd;

	if (epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &ev) < 0) {
		fprintf(stderr, "add server fd failed(%s)\n", strerror(errno));
		close(fd);
		return -1;
	}

	while (1) {
		int i;
		int nfds = epoll_wait(epollfd, events, MAX_EVENTS, -1);
		if ((nfds < 0) && (errno != EINTR)) {                  
			fprintf(stderr, "epoll wait failed(%s)", strerror(errno));   
			return -1;                                         
		}                                                      

		for (i = 0; i < nfds; i++) {
			if ((events[i].events & EPOLLIN) == EPOLLIN) {
				struct sockaddr_in addr;
				socklen_t len;
				char buf[INET_ADDRSTRLEN + 1];

				int newfd = accept(events[i].data.fd, NULL, NULL);
				if (newfd < 0) {
					fprintf(stderr, "accept failed(%s)\n", strerror(errno));
					close(newfd);
					continue;
				}

				if (getpeername(newfd, (struct sockaddr*)&addr, &len) < 0) {
					fprintf(stderr, "get peer name failed(%s)\n", strerror(errno));
					close(newfd);
					continue;
				}

				if (inet_ntop(AF_INET, &addr.sin_addr, buf, sizeof(buf)) == NULL) {
					fprintf(stderr, "convert ip address failed\n");
					close(newfd);
					continue;
				}


				printf("ip address:%s\n", buf);

				if (send(newfd, buf, strlen(buf) + 1, 0) < 0) {
					fprintf(stderr, "send back failed(%s)\n", strerror(errno));
					close(newfd);
					continue;
				}

				close(newfd);
			}

			if ((events[i].events & EPOLLHUP) == EPOLLHUP) {
				fprintf(stderr, "fd %d received hup event\n", events[i].data.fd);
				close(events[i].data.fd);
			}
		}
	}

}
