#define __LIBRARY__
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>

#define BUFFER_SIZE 10 /* 缓冲区数量 */
#define NUM 25		   /* 产品总数 */


#define __NR_sem_open 87
#define __NR_sem_wait 88
#define __NR_sem_post 89
#define __NR_sem_unlink 90
#define __NR_shmget 91
#define __NR_shmat 92

typedef void sem_t;

_syscall2(int, sem_open, const char*, name, unsigned int, value)
_syscall1(int, sem_wait, sem_t *, sem)
_syscall1(int, sem_post, sem_t *, sem)
_syscall1(int, sem_unlink, const char *, name)
_syscall2(int, shmget, int, key, int, size)
/* 手动定义shmat系统调用，返回void*指针 */
static inline void* shmat(int shmid, const void *shmaddr)
{
	long __res;
	__asm__ volatile ("int $0x80"
		: "=a" (__res)
		: "0" (__NR_shmat), "b" ((long)(shmid)), "c" ((long)(shmaddr)));
	if (__res >= 0)
		return (void*) __res;
	return (void*) 0;
}


int main()
{	
	/* 注意: 在应用程序中不能使用断点等调试功能 */
	int i, j;
	int consumeNum = 0; /* 消费者消费的产品号 */
	int produceNum = 0; /* 生产者生产的产品号 */
	int consume_pos = 0; /* 消费者从共享缓冲区中取出产品消费的位置 */
	int produce_pos = 0; /* 生产者生产产品向共享缓冲区放入的位置 */
	
	sem_t *empty, *full, *mutex;
	int shmid;
	int *shm_buffer = NULL;  /* 共享内存缓冲区指针 */
	pid_t producer_pid, consumer_pid;
	
	/* 创建empty、full、mutex三个信号量 */
	empty = (sem_t*)sem_open("empty", BUFFER_SIZE);
	full  = (sem_t*)sem_open("full", 0);
	mutex = (sem_t*)sem_open("mutex", 1);
	
	/* 创建共享内存作为缓冲区 */
	shmid = shmget(1, 4096);  /* key=1, size=4096 (一个页面) */
	if(shmid < 0)
	{
		printf("shmget failed!\n");
		return -1;
	}
	
	/* 将共享内存映射到当前进程的地址空间 */
	shm_buffer = (int*)shmat(shmid, (const void*)0);
	if(shm_buffer == NULL)
	{
		printf("shmat failed!\n");
		return -1;
	}
	
	/* 创建生产者进程 */
	if( ! fork() )
	{
		/* 子进程也需要映射共享内存 */
		shm_buffer = (int*)shmat(shmid, (const void*)0);
		
		producer_pid = getpid();
		printf("producer pid=%d create success!\n", producer_pid);
		for( i = 0 ; i < NUM; i++)
		{
			sem_wait(empty);
			sem_wait(mutex);
			
			produceNum = i;
			
			/* 将产品放入共享内存缓冲区 */
			shm_buffer[produce_pos] = produceNum;
			
			/* 输出生产产品的信息 */
			printf("Producer pid=%d : %02d at %d\n", producer_pid, produceNum, produce_pos); 
			fflush(stdout);
			
			/* 生产者的游标向后移动一个位置 */
			produce_pos = (produce_pos + 1) % BUFFER_SIZE;
			
			sem_post(mutex);
			sem_post(full);
			
			sleep(2);
		}
		exit(0);
	}
	
	/* 创建消费者进程 */
	if( ! fork() )
	{
		/* 子进程也需要映射共享内存 */
		shm_buffer = (int*)shmat(shmid, (const void*)0);
		
		consumer_pid = getpid();
		printf("\t\t\tconsumer pid=%d create success!\n", consumer_pid);
		for( j = 0; j < NUM; j++ ) 
		{
			sem_wait(full);
			sem_wait(mutex);
			
			/* 从共享内存缓冲区取出产品 */
			consumeNum = shm_buffer[consume_pos];
			
			/* 输出消费产品的信息 */
			printf("\t\t\tConsumer pid=%d: %02d at %d\n", consumer_pid, consumeNum, consume_pos);
			fflush(stdout);
			
			/* 消费者的游标向后移动一个位置 */
			consume_pos = (consume_pos + 1) % BUFFER_SIZE;
	
			sem_post(mutex);
			sem_post(empty);
			
			if(j<4)	sleep(8);
			else sleep(1);
		}
		exit(0);
	}

	wait(NULL);	/* 等待生产者进程结束 */
	wait(NULL);	/* 等待消费者进程结束 */
	
	/* 关闭所有信号量 */
	sem_unlink("empty");
	sem_unlink("full");
	sem_unlink("mutex");
	
	return 0;
}