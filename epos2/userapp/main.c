/*
 * vim: filetype=c:fenc=utf-8:ts=4:et:sw=4:sts=4
 */
#include <inttypes.h>
#include <stddef.h>
#include <math.h>
#include <stdio.h>
#include <sys/mman.h>
#include <syscall.h>
#include <netinet/in.h>
#include <time.h>
#include <stdlib.h>
#include "graphics.h"

extern void *tlsf_create_with_pool(void *mem, size_t bytes);
extern void *g_heap;

/**
 * GCC insists on __main
 *    http://gcc.gnu.org/onlinedocs/gccint/Collect2.html
 */
void __main()
{
	size_t heap_size = 32 * 1024 * 1024;
	void *heap_base = mmap(NULL, heap_size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON, -1, 0);
	g_heap = tlsf_create_with_pool(heap_base, heap_size);
}

#define Task_num 3 // 线程个数
#define N 1000	 // 随机数个数
#define MAX 1000   // 随机数范围

const double u_length = 0.2; // 直线单位长度
const double width = 1;		 // 直线之间的间距

// 定义结构体变量
struct data
{
	int array[N];
	int left;
	int right;
	int length;
};

// 实现比较函数
int prior(int a, int b)
{ // 较小的数在前
	if (a < b)
		return 1;
	else
		return 0;
}

// 实现交换函数，以及擦除与重画
void swap(int x, int y, int A[], int n, int a, int b, int cr1,int cr2, int cr3)  
{
	int temp, flag = 0; // flag为颜色渐变位
	int cr[3] = {cr1, cr2, cr3};

	// 擦除—————黑色完全覆盖,
	line(x + width * a, y, x + width * a, y + MAX * u_length, RGB(0, 0, 0));
	line(x + width * b, y, x + width * b, y + MAX * u_length, RGB(0, 0, 0));

	temp = A[a];
	A[a] = A[b];
	A[b] = temp;

	// 设置颜色渐变位
	if (cr[0] == 0)
		flag = 0;
	else if (cr[1] == 0)
		flag = 1;
	else if (cr[2] == 0)
		flag = 2;

	// 重画
	cr[flag] = 255 - (((double)a / N) * 255); // 根据位置由浅至深渐变
	line(x + width * a, y, x + width * a, y + A[a] * u_length, RGB(cr[0], cr[1], cr[2]));
	cr[flag] = 255 - (((double)b / N) * 255); // 由浅至深渐变
	line(x + width * b, y, x + width * b, y + A[b] * u_length, RGB(cr[0], cr[1], cr[2]));
}

/**
 * 实现排序函数
 */

// 实现冒泡排序
void bubsort(int x, int y, int A[], int n, int cr[])
{
	int i, j;
	for (i = 0; i < n - 1; i++)
		for (j = n - 1; j > i; j--)
			if (prior(A[j], A[j - 1]))
			{	
				swap(x, y, A, n, j, j - 1,cr[0],cr[1],cr[2]); 
			}
}

// 实现选择排序
void selsort(int x, int y, int A[], int n, int cr[])
{
	for (int i = 0; i < n - 1; i++)
	{
		int lowindex = i;
		for (int j = n - 1; j > i; j--)
		{
			nanosleep((const struct timespec[]){{0, 10000L}}, NULL);
			if (prior(A[j], A[lowindex]))
				lowindex = j;
		}
		swap(x, y, A,  n, i, lowindex,cr[0],cr[1],cr[2]);
	}
}

// 实现插入排序
void inssort(int x, int y, int A[], int n, int cr[])
{
	int i, j;
	for (i = 1; i < n; i++)
		for (j = i; (j > 0) && (prior(A[j], A[j - 1])); j--)
		{
			swap(x, y, A, n,  j, j - 1,cr[0],cr[1],cr[2]);
		}
}

/**
 * 初始化图形
 */
void draw(int x, int y, int A[], int n, int cr[])
{

	int i, flag = 0; // flag为颜色渐变位
	// 设置颜色渐变
	if (cr[0] == 0)
		flag = 0;
	else if (cr[1] == 0)
		flag = 1;
	else if (cr[2] == 0)
		flag = 2;

	for (i = 0; i < n; i++)
	{
		cr[flag] = 255 - (((double)i / N) * 255); // 由浅至深渐变
		line(x + width * i, y, x + width * i, y + A[i] * u_length, RGB(cr[0], cr[1], cr[2]));
	}
}

/**
 * 定义排序线程函数
*/
// 冒泡排序线程
void tsk_bubsort(void *pv)
{
	int x = 0;					  // 起始x坐标
	int y = 0;					  // 起始y坐标
	int Cyan1[3] = {0, 255, 255}; // Cyan1的RGB码
	draw(x, y, ((struct data *)pv)->array, ((struct data *)pv)->length, Cyan1);
	sleep(2); // 暂停看初始化效果
	bubsort(x, y, ((struct data *)pv)->array, ((struct data *)pv)->length, Cyan1);
	task_exit(0);
}

// 选择排序线程
void tsk_selsort(void *pv)
{
	int weight = g_graphic_dev.YResolution / Task_num; // 垂直等分Task_num部分
	int x = 0;									// 起始x坐标
	int y = weight;								// 起始y坐标
	int Magenta[3] = {255, 0, 255};				// Magenta的RGB码
	if (weight < MAX * u_length)
		y = MAX * u_length; // 每一部分weight取MAX * u_length和g_graphic_dev.YResolution / 3间的较大值
	draw(x, y, ((struct data *)pv)->array, ((struct data *)pv)->length, Magenta);
	sleep(2); // 暂停看初始化效果
	selsort(x, y, ((struct data *)pv)->array, ((struct data *)pv)->length, Magenta);
	task_exit(0);
}

// 插入排序线程
void tsk_inssort(void *pv)
{
	int weight = g_graphic_dev.YResolution / Task_num; // 垂直等分Task_num部分
	int x = 0;									// 起始x坐标
	int y = weight * 2;							// 起始y坐标
	int Yellow1[3] = {255, 255, 0};				// Yellow1的RGB码
	if (weight < MAX * u_length)
		y = MAX * u_length * 2; 				// 每一部分weight取MAX* u_length和g_graphic_dev.YResolution / Task_num间的较大值
	draw(x, y, ((struct data *)pv)->array, ((struct data *)pv)->length, Yellow1);
	sleep(2); // 暂停看初始化效果
	inssort(x, y, ((struct data *)pv)->array, ((struct data *)pv)->length, Yellow1);
	task_exit(0);
}

/**
 * 第一个运行在用户模式的线程所执行的函数
 */
void main(void *pv)
{
	struct data arr[Task_num]; // 定义结构体变量数组
	int tid_bubsort;		   // 定义最慢排序线程id
	int mode = 0x144;		   // 图形界面模式，不同的分辨率
	int temp[N];
	int i, k;

	// list_graphic_modes();  // 系统支持的图形界面模型

	// printf("task #%d: I'm the first user task(pv=0x%08x)!\r\n",task_getid(), pv);

	//TODO: Your code goes here

	/**
	 *  申请线程栈
	 */
	unsigned char *stack_bubsort, *stack_selsort, *stack_inssort;
	unsigned int stack_size = 1024 * 1024;
	stack_bubsort = (unsigned char *)malloc(stack_size);
	stack_selsort = (unsigned char *)malloc(stack_size);
	stack_inssort = (unsigned char *)malloc(stack_size);

	/**
	 * 数据初始化
	 */
	// 播种
	srand(time(NULL));

	// 对arr赋值		
	for (i = 0; i < N; i++)
		temp[i] = (rand() % MAX) + 1;
	for (k = 0; k < Task_num; k++)
	{
		arr[k].left = 0;
		arr[k].right = N - 1;
		arr[k].length = N;
		// 产生N个随机数，范围为：1-MAX
		for (i = 0; i < N; i++)
			arr[k].array[i] = temp[i];
	}
	/**
	 * 进入图形模式
	 */
	init_graphic(mode);

	/**
	 *  创建线程,仅最慢排序进程返回进程id
	 */
	tid_bubsort = task_create(stack_bubsort + stack_size, &tsk_bubsort, (void *)&arr[0]);
	task_create(stack_selsort + stack_size, &tsk_selsort, (void *)&arr[1]);
	task_create(stack_inssort + stack_size, &tsk_inssort, (void *)&arr[2]);

	/**
	 * 等待最后一个排序线程结束
	 */
	task_wait(tid_bubsort, NULL);

	free(stack_bubsort);
	free(stack_selsort);
	free(stack_inssort);

	while (1)
		;

	/**
	 *  退出图形模式
	 */
	exit_graphic();

	task_exit(0);
}
