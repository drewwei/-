#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <poll.h>
#include <signal.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>

//#define FIFO_CLEAR 0X1
#define MAX_LEN 200

void input_handler(int num)
{
	char data[MAX_LEN];
	int len;
	
	len = read(STDIN_FILENO, &data, MAX_LEN);//从标准输入读出数据
	data[len] = '\0';     
	printf("input available: %s\n", data);//打印数据
	printf("Have caught SGNO: %d\n", num); //SIGIO的信号值是29
}
int main(int argc, char **argv)
{
	int oflags;
	
	/* 启动信号驱动机制 */
	signal(SIGIO, input_handler);// 1.绑定信号处理函数和指定信号值
	fcntl(STDIN_FILENO, F_SETOWN, getpid()); //2.将标准输入文件描述符STDIN_FILENO的拥有者设置为当前进程
	oflags = fcntl(STDIN_FILENO, F_GETFL); //3.获得fags
	fcntl(STDIN_FILENO, F_SETFL, oflags|FASYNC);//4.设置为异步模式FASYNC
	
	while(1);//避免程序执行完而信号驱动函数没有被调用
	
return 0;
}