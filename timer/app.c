#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <poll.h>
#include <signal.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>

int main(int argc, char **argv)
{
	int fd;
	int data=0;
	int i = 10;
	fd = open("/dev/globalmem", O_RDWR);
	if (fd < 0)
	{
		printf("can't open!\n");
		return -1;
	}
while(i--)
{
	read(fd, &data, sizeof(data));
	printf("data:%d\n", data);
sleep(1);
}
close(fd);
return 0;
}