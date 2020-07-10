#include <err.h>
#include <netinet/in.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include <thread>

#include <atomic>

std::atomic_int a(0);

void writer(int s)
{
	while(!a)
		usleep(1);
	while(1) {
		usleep(10000);
		uint32_t b = 0;
		const char *str = "              hello!\n";
		b = strlen(str) + 1;

		b = htonl(b);
		printf("writing new packet!\n");
		write(s, &b, 4);
		write(s, str, strlen(str) + 1);
	}
}

int main()
{
	int sockfd, newsockfd;
	socklen_t clilen;
	char buffer[256];
	struct sockaddr_in serv_addr, cli_addr;
	int n;
	sockfd = socket(AF_INET, SOCK_STREAM, 0);

	if(sockfd < 0) {
		err(1, "socket");
	}
	int optval = 1;
	setsockopt(sockfd, SOL_SOCKET, SO_REUSEPORT, &optval, sizeof(optval));

	memset((char *)&serv_addr, 0, sizeof(serv_addr));

	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = INADDR_ANY;
	serv_addr.sin_port = htons(1234);

	if(bind(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
		err(1, "bind");
	}

	listen(sockfd, 5);
	clilen = sizeof(cli_addr);

	newsockfd = accept(sockfd, (struct sockaddr *)&cli_addr, &clilen);
	if(newsockfd < 0) {
		err(1, "accept");
	}

	ssize_t r;
	char buf[128];
	std::thread thr(writer, newsockfd);
	while((r = read(newsockfd, buf, 128))) {
		printf("read: %ld bytes\n", r);
		for(ssize_t i = 0; i < r; i++) {
			printf("%x ", buf[i]);
		}
		printf("\n");
		a = 1;
		//	write(0, buf, r);
	}

	return 0;
}
