#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <poll.h>
#include <signal.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>



void input_handler(int num)
{
	printf("Have caught SGNO: %d\n", num); //SIGIO的信号值是29
}
int main(int argc, char **argv)
{
	int oflags, fd;
	fd = open("/dev/globalmem", O_RDWR);
	if(fd < 0)
	{
		printf("open /dev/globalmem fail\n");
		return -1;
	}
	/* 启动信号驱动机制 */
	signal(SIGIO, input_handler);// 1.绑定信号处理函数和指定信号值
	fcntl(fd, F_SETOWN, getpid()); //2.将标准输入文件描述符STDIN_FILENO的拥有者设置为当前进程
	oflags = fcntl(fd, F_GETFL); //3.获得fags
	fcntl(fd, F_SETFL, oflags|FASYNC);//4.设置为异步模式FASYNC
	
	while(1);//避免程序执行完而信号驱动函数没有被调用
	
return 0;
}