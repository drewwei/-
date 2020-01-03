#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <poll.h>
#include <signal.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>

#define FIFO_CLEAR 0X1
#define BUFFER_LEN 20


int main(int argc, char **argv)
{
	int fd, num;
	char rd_ch[BUFFER_LEN];
	fd_set rfds, wfds; //读和写文件描述符集
	fd = open("/dev/globalmem", O_RDWR|O_NONBLOCK);//非阻塞打开
	if (fd < 0)
	{
		printf("can't open!\n");
		return -1;
	}
	while(1)
	{
		FD_ZERO(&rfds);   //1.清空描述符集
		FD_ZERO(&wfds);
		FD_SET(fd, &rfds); //2.将描述符设置到读写描述符集中，内部调用了set_bit
		FD_SET(fd, &wfds);
		//3.监控 参数一为描述符最大值+1 ，比如要监控两个描述符fd1,fd2 ,则maxfd = fd1>fd2?(fd1+1):(fd2+1);
		select(fd + 1, &rfds, &wfds, NULL, NULL);
		int pid;
		pid = getpid();
		
		if(FD_ISSET(fd, &rfds))
			printf("[%d] Poll monitor can be read\n", pid);
		
		if(FD_ISSET(fd, &wfds))
			printf("[%d] Poll monitor can be writen\n", pid);	
		
		sleep(5);
	}
	
	close(fd);
return 0;
}