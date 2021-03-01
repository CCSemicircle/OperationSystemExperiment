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
#include <stdlib.h>
#include "graphics.h"

extern void *tlsf_create_with_pool(void *mem, size_t bytes);
extern void *g_heap;

#define BUFF_NUM 10      // 缓冲区个数
#define N 600            // 随机数个数
#define MAX 1000         // 随机数范围
#define DELAY_WEIGHT 1 // 每一级别的权重,每次修改的时间10 * 10^6 ns
#define LEN 20           // 优先级进度条宽度
#define LINE 5           // 重复画线条个数
#define UP 0x4800
#define DOWN 0x5000
#define RIGHT 0x4d00
#define LEFT 0x4b00
#define ENTER 0x1c0d    

const double u_length = 0.1; // 直线单位长度
const double width = 1.2;    // 直线之间的间距

/**
* 定义信号量
 */
int empty, full, mutex[BUFF_NUM]; // 信号量ID，mutex为每个缓冲区进程的互斥信号量

int arr[BUFF_NUM][N]; // 全局变量，随机数组

// 定义睡眠时间
volatile int delay_productor = 3;
volatile int delay_consumer = 3;

// 定义键入字符变量
volatile int key=0;

// 实现比较函数
int prior(int a, int b)
{ // 较小的数在前
    if (a < b)
        return 1;
    else
        return 0;
}

/**
 * 生产者进程
 */
void tsk_productor(void *pv)
{
    // 每一部分weight取MAX * u_length和g_graphic_dev.XResolution / BUFF_NUM间的较大值
    int weight = (g_graphic_dev.XResolution / BUFF_NUM > MAX * u_length ? g_graphic_dev.XResolution / BUFF_NUM : MAX * u_length); 
    int x = 0;                               // 起始x坐标
    int y = 0;                               // 起始y坐标
    int curr=0;                              // 记录当前的缓冲区位置
    int cr[3] = {84,255,159};
    int i;
    /**
	 * 产生随机数组
	 */
    // 播种
    srand(time(NULL));

    do
    {
        if(key==ENTER)
            break;
        if (curr == BUFF_NUM)
            curr = 0;
        // 产生N个随机数，范围为：1-MAX
        sem_wait(empty);
        sem_wait(mutex[curr]);
        for (i = 0; i < N; i++)
        {
            arr[curr][i] = (rand() % MAX) + 1;
        }
        int *A = arr[curr];
        int n = N;
        x = weight * curr;
        // 初始化图形
        for (i = 0; i < n; i++)
        {
            nanosleep((const struct timespec[]){{delay_productor / 1000, (delay_productor % 1000) * 1000000L}}, NULL);
            line(x, y + width * i, x + A[i] * u_length, y + width * i, RGB(cr[0],cr[1],cr[2]));
        }
        sem_signal(full);
        sem_signal(mutex[curr]);           
        curr++;
    } while (1);

    task_exit(0);
}

/**
 * 消费者进程
 */
void tsk_consumer(void *pv)
{
    // 每一部分weight取MAX * u_length和g_graphic_dev.XResolution / BUFF_NUM间的较大值
    int weight = (g_graphic_dev.XResolution / BUFF_NUM > MAX * u_length ? g_graphic_dev.XResolution / BUFF_NUM : MAX * u_length); 
    int x = 0;                                         // 起始x坐标
    int y = 0;                                         // 起始y坐标
    int curr=0;
    int cr[3] = {84,255,159};
    do
    {
        if(key==ENTER)
            break;
        if(curr==BUFF_NUM)
            curr = 0;
        sem_wait(full);
        sem_wait(mutex[curr]);
        int *A = arr[curr];
        int n = N;
        int i, j;
        x = weight * curr;
        // 选择排序
        for (i = 0; i < n - 1; i++)
        {
            int lowindex = i;
            for (j = n - 1; j > i; j--)
            {
                //nanosleep((const struct timespec[]){{delay_consumer / 1000, (delay_consumer % 1000) * 1000000L}}, NULL);
                if (prior(A[j], A[lowindex]))
                    lowindex = j;
            }
            int temp, a = j, b = lowindex;
            nanosleep((const struct timespec[]){{delay_consumer / 1000, (delay_consumer % 1000) * 1000000L}}, NULL);

            // 擦除—————黑色完全覆盖
            line(x, y + width * a, x + MAX * u_length, y + width * a, RGB(0, 0, 0));
            line(x, y + width * b, x + MAX * u_length, y + width * b, RGB(0, 0, 0));
            // 交换
            temp = A[a];
            A[a] = A[b];
            A[b] = temp;
            // 重画
            line(x, y + width * a, x + A[a] * u_length, y + width * a, RGB(cr[0],cr[1],cr[2]));
            line(x, y + width * b, x + A[b] * u_length, y + width * b, RGB(cr[0],cr[1],cr[2]));
        }

        // 清除一个缓冲区
        for (i = 0; i < n; i++)
        {
            line(x, y + width * i, x + MAX * u_length, y + width * i, RGB(0,0,0));
        }
        sem_signal(empty);
        sem_signal(mutex[curr]);
        curr++;
    } while (1);
    task_exit(0);
}

/**
 * 控制线程
 */
/**
 * 如果key=0x4800(up)/0x5000(down)
 * 调用setpriority调高/低生产者线程的速度
 * 更新生产者的速度进度条
 * 如果key=0x4d00(right)/0x4b00(left)
 * 调用setpriority调高/低消费者线程的速度
 * 更新消费者的速度进度条
*/
void tsk_controller(void *pv)
{

    // 绘制初始进度条
    int x = g_graphic_dev.XResolution / 2;  // 起始x坐标
    int y = g_graphic_dev.YResolution - 40; // 起始y坐标
    int crA[3] = {0, 255, 255};           // A进程的RGB码
    int crB[3] = {255, 255, 0};         // B进程的RGB码
    int i;

    // 绘制初始化速度进度条
    for (i = 1; i <= delay_productor / DELAY_WEIGHT; i++)
    {
        for (int j = 0; j < LINE; j++)
            line(x - (width * i + LINE * (i - 1) + j), y, x - (width * i + LINE * (i - 1) + j), y + LEN, RGB(crA[0], crA[1], crA[2]));
    }

    for (i = 1; i <= delay_consumer / DELAY_WEIGHT; i++)
    {
        for (int j = 0; j < LINE; j++)
            line(x + (width * i + LINE * (i - 1) + j), y, x + (width * i + LINE * (i - 1) + j), y + LEN, RGB(crB[0], crB[1], crB[2]));
    }

    // 等待键盘输入，控制进程速度
    do
    {
        key = getchar();
        if(key==ENTER)
            break;
        if (key == UP && (x - (width * (i + 1) + LINE * i)) >= 0)
        {
            delay_productor += DELAY_WEIGHT;
            i = delay_productor / DELAY_WEIGHT;
            for (int j = 0; j < LINE; j++)
                line(x - (width * i + LINE * (i - 1) + j), y, x - (width * i + LINE * (i - 1) + j), y + LEN, RGB(crA[0], crA[1], crA[2]));
        }
        else if (key == DOWN && delay_productor / DELAY_WEIGHT > 1)
        {
            i = delay_productor / DELAY_WEIGHT;
            delay_productor -= DELAY_WEIGHT;
            for (int j = 0; j < LINE; j++)
                line(x - (width * i + LINE * (i - 1) + j), y, x - (width * i + LINE * (i - 1) + j), y + LEN, RGB(0, 0, 0));
        }
        else if (key == RIGHT && (x + (width * (i + 1) + LINE * i)) < g_graphic_dev.XResolution)
        {
            delay_consumer += DELAY_WEIGHT;
            i = delay_consumer / DELAY_WEIGHT;
            for (int j = 0; j < LINE; j++)
                line(x + (width * i + LINE * (i - 1) + j), y, x + (width * i + LINE * (i - 1) + j), y + LEN, RGB(crB[0], crB[1], crB[2]));
        }
        else if (key == LEFT && delay_consumer / DELAY_WEIGHT > 1)
        {
            i = delay_consumer / DELAY_WEIGHT;
            delay_consumer -= DELAY_WEIGHT;
            for (int j = 0; j < LINE; j++)
                line(x + (width * i + LINE * (i - 1) + j), y, x + (width * i + LINE * (i - 1) + j), y + LEN, RGB(0, 0, 0));
        }
        else
            continue;
    } while (1);

    task_exit(0);
}

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

/**
 * 第一个运行在用户模式的线程所执行的函数
 */
void main(void *pv)
{
    int tid_productor, tid_consumer, tid_controller; // 定义线程id
    int mode = 0x144;                // 图形界面模式，不同的分辨率
    int i;

    printf("task #%d: I'm the first user task(pv=0x%08x)!\r\n",task_getid(), pv);

    for (i = 0; i < 3;i++)
        printf("\n");

    printf("Wait for 8s, enter the graphical mode to demonstrate the results, press Enter to exit the graphical mode!\n");

    sleep(8);

    //TODO: Your code goes here

    // 创建信号量
    empty = sem_create(BUFF_NUM);
    full = sem_create(0);
    for (i = 0; i < BUFF_NUM;i++)
        mutex[i] = sem_create(1);

    /**
	 *  申请线程栈
	 */
    unsigned char *stack_productor, *stack_consumer, *stack_controller;
    unsigned int stack_size = 1024 * 1024;
    stack_productor = (unsigned char *)malloc(stack_size);
    stack_consumer = (unsigned char *)malloc(stack_size);
    stack_controller = (unsigned char *)malloc(stack_size);

    /**
	 * 进入图形模式
	 */
    init_graphic(mode);

    /**
	 *  创建线程
	 */
    tid_productor = task_create(stack_productor + stack_size, &tsk_productor, (void *)0);
    tid_consumer = task_create(stack_consumer + stack_size, &tsk_consumer, (void *)0);
    tid_controller = task_create(stack_controller + stack_size, &tsk_controller, (void *)0);


    /**
	 * 等待线程结束
	 */
    task_wait(tid_productor, NULL);
    task_wait(tid_consumer, NULL);
    task_wait(tid_controller, NULL);

    sem_destroy(empty);
    sem_destroy(full);
    for(i=0;i<BUFF_NUM;i++)
        sem_destroy(mutex[i]);

    free(stack_productor);
    free(stack_consumer);
    free(stack_controller);

    /**
	 *  退出图形模式
	 */
    exit_graphic();

    printf("The presentation is over, thanks for watching!\n");

    while (1)
        ;

    task_exit(0);
}
