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

#define TASK_NUM 2       // 排序线程个数
#define N 600            // 随机数个数
#define MAX 1000         // 随机数范围
#define LOCATION_LEFT 0  // 屏幕左区
#define LOCATION_RIGHT 1 // 屏幕右区
#define PRIO_WEIGHT 2    // 每一级别的权重
#define MAX_PRIO 40      // prio的最大值+1
#define MIN_PRIO 0       // prio的最小值
#define LEN 20           // 优先级进度条宽度
#define LINE 5           // 重复画线条个数
#define UP 0x4800
#define DOWN 0x5000
#define RIGHT 0x4d00
#define LEFT 0x4b00

const double u_length = 0.1; // 直线单位长度
const double width = 1.2;    // 直线之间的间距

// 定义数据结构体变量
struct data
{
    int array[N];
    int left;
    int right;
    int length;
};

// 定义控制器结构体
struct tsk_prio
{
    int tid[TASK_NUM];
    int prio[TASK_NUM];
    int wait[TASK_NUM];
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
void swap(int x, int y, int A[], int n, int a, int b, int location, int cr1, int cr2, int cr3)
{
    int temp, flag = 0; // flag为颜色渐变位
    int cr[3] = {cr1, cr2, cr3};

    nanosleep((const struct timespec[]){{0, 500000L}}, NULL);

    // 擦除—————黑色完全覆盖
    if (location == LOCATION_LEFT)
    {
        line(x - MAX * u_length, y + width * a, x, y + width * a, RGB(0, 0, 0));
        line(x - MAX * u_length, y + width * b, x, y + width * b, RGB(0, 0, 0));
    }
    else
    {
        line(x, y + width * a, x + MAX * u_length, y + width * a, RGB(0, 0, 0));
        line(x, y + width * b, x + MAX * u_length, y + width * b, RGB(0, 0, 0));
    }

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
    if (location == LOCATION_LEFT)
    {
        cr[flag] = 255 - (((double)a / N) * 255); // 根据位置由浅至深渐变
        line(x - A[a] * u_length, y + width * a, x, y + width * a, RGB(cr[0], cr[1], cr[2]));
        cr[flag] = 255 - (((double)b / N) * 255); // 由浅至深渐变
        line(x - A[b] * u_length, y + width * b, x, y + width * b, RGB(cr[0], cr[1], cr[2]));
    }
    else
    {
        cr[flag] = 255 - (((double)a / N) * 255); // 根据位置由浅至深渐变
        line(x, y + width * a, x + A[a] * u_length, y + width * a, RGB(cr[0], cr[1], cr[2]));
        cr[flag] = 255 - (((double)b / N) * 255); // 由浅至深渐变
        line(x, y + width * b, x + A[b] * u_length, y + width * b, RGB(cr[0], cr[1], cr[2]));
    }
}

// 实现冒泡排序
void bubsort(int x, int y, int A[], int n, int loaction, int cr[])
{
    int i, j;
    for (i = 0; i < n - 1; i++)
        for (j = n - 1; j > i; j--)
            if (prior(A[j], A[j - 1]))
            {
                swap(x, y, A, n, j, j - 1, loaction, cr[0], cr[1], cr[2]);
            }
}

/**
 * 初始化图形
 */
void draw(int x, int y, int A[], int n, int location, int cr[])
{

    int i, flag = 0; // flag为颜色渐变位
    // 设置颜色渐变
    if (cr[0] == 0)
        flag = 0;
    else if (cr[1] == 0)
        flag = 1;
    else if (cr[2] == 0)
        flag = 2;

    if (location == LOCATION_LEFT)
    {
        for (i = 0; i < n; i++)
        {
            cr[flag] = 255 - (((double)i / N) * 255); // 由浅至深渐变
            line(x - A[i] * u_length, y + width * i, x, y + width * i, RGB(cr[0], cr[1], cr[2]));
        }
    }
    else
    {

        for (i = 0; i < n; i++)
        {
            cr[flag] = 255 - (((double)i / N) * 255); // 由浅至深渐变
            line(x, y + width * i, x + A[i] * u_length, y + width * i, RGB(cr[0], cr[1], cr[2]));
        }
    }
}

/**
 * 定义排序线程函数
*/
// 冒泡排序线程-左
void tsk_left(void *pv)
{
    int weight = g_graphic_dev.XResolution / TASK_NUM; // 水平等分TASK_NUM个部分
    int x = weight;                                    // 起始x坐标
    int y = 0;                                         // 起始y坐标
    int *A = ((struct data *)pv)->array;
    int n = ((struct data *)pv)->length;
    int i, j;
    if (weight < MAX * u_length)
        x = MAX * u_length; // 每一部分weight取MAX * u_length和g_graphic_dev.XResolution / TASK_NUM间的较大值
    // 初始化图形
    for (i = 0; i < n; i++)
    {
        // 由浅至深渐变,Cyan1的RGB码 Cyan1[3] = {0, 255, 255}
        line(x - A[i] * u_length, y + width * i, x, y + width * i, RGB((255 - (((double)i / N) * 255)), 255, 255));
    }
    sleep(3); // 暂停看初始化效果
    // 排序
    for (i = 0; i < n - 1; i++)
        for (j = n - 1; j > i; j--)
            if (prior(A[j], A[j - 1]))
            {
                int temp, a = j, b = j - 1;

                nanosleep((const struct timespec[]){{0, 500000L}}, NULL);

                // 擦除—————黑色完全覆盖
                line(x - MAX * u_length, y + width * a, x, y + width * a, RGB(0, 0, 0));
                line(x - MAX * u_length, y + width * b, x, y + width * b, RGB(0, 0, 0));

                // 交换
                temp = A[a];
                A[a] = A[b];
                A[b] = temp;

                // 重画

                // 根据位置由浅至深渐变
                line(x - A[a] * u_length, y + width * a, x, y + width * a, RGB((255 - (((double)a / N) * 255)), 255, 255));
                // 由浅至深渐变
                line(x - A[b] * u_length, y + width * b, x, y + width * b, RGB((255 - (((double)b / N) * 255)), 255, 255));
            }
    // 测试调度器
    /*
    for (int i = 0; i < 100000;i++)
    {
        nanosleep((const struct timespec[]){{0, 1000000L}}, NULL);
        printf("%d", 1);
    }*/
    task_exit(0);
}

// 冒泡排序线程-右
void tsk_right(void *pv)
{
    int weight = g_graphic_dev.XResolution / TASK_NUM; // 水平等分TASK_NUM个部分
    int x = weight;                                    // 起始x坐标
    int y = 0;                                         // 起始y坐标
    int *A = ((struct data *)pv)->array;
    int n = ((struct data *)pv)->length;
    int i, j;
    // 每一部分weight取MAX * u_length和g_graphic_dev.XResolution / TASK_NUM间的较大值
    if (weight < MAX * u_length)
        x = MAX * u_length;
    // 初始化图形
    for (i = 0; i < n; i++)
    {
        // 由浅至深渐变, Yellow1的RGB码 Yellow1[3] = {255, 255, 0}
        line(x, y + width * i, x + A[i] * u_length, y + width * i, RGB(255, 255, 255 - (((double)i / N) * 255)));
    }
    sleep(3); // 暂停看初始化效果
    // 排序
    for (i = 0; i < n - 1; i++)
        for (j = n - 1; j > i; j--)
            if (prior(A[j], A[j - 1]))
            {
                int temp, a = j, b = j - 1;
                nanosleep((const struct timespec[]){{0, 500000L}}, NULL);

                // 擦除—————黑色完全覆盖
                line(x, y + width * a, x + MAX * u_length, y + width * a, RGB(0, 0, 0));
                line(x, y + width * b, x + MAX * u_length, y + width * b, RGB(0, 0, 0));
                // 交换
                temp = A[a];
                A[a] = A[b];
                A[b] = temp;
                // 重画
                // 根据位置由浅至深渐变
                line(x, y + width * a, x + A[a] * u_length, y + width * a, RGB(255, 255, 255 - (((double)a / N) * 255)));
                // 由浅至深渐变
                line(x, y + width * b, x + A[b] * u_length, y + width * b, RGB(255, 255, 255 - (((double)b / N) * 255)));
            }
    // 测试调度器
    /*
    for (int i = 0; i < 100000;i++)
    {
        nanosleep((const struct timespec[]){{0, 1000000L}}, NULL);
        printf("%d", 2);
    }*/
    task_exit(0);
}

/**
 * 创建控制线程
 */
/**
 * 如果key=0x4800(up)/0x5000(down)
 * 调用setpriority调高/低线程的静态优先级
 * 更新A的优先级进度条
 * 如果key=0x4d00(right)/0x4b00(left)
 * 调用setpriority调高/低B线程的静态优先级
 * 更新B的优先级进度条
*/
void tsk_controller(void *pv)
{

    // 绘制初始进度条
    int weight = g_graphic_dev.XResolution / TASK_NUM; // 水平等分TASK_NUM个部分
    int x = weight;                                    // 起始x坐标
    int y = g_graphic_dev.YResolution - 40;            // 起始y坐标
    int Cyan1[3] = {0, 255, 255};                      // Cyan1的RGB码
    int Yellow1[3] = {255, 255, 0};                    // Yellow1的RGB码
    int i, key;
    struct tsk_prio *controller = (struct tsk_prio *)pv;

    if (weight < MAX * u_length)
        x = MAX * u_length; // 每一部分weight取MAX * u_length和g_graphic_dev.XResolution / TASK_NUM间的较大值

    // 绘制初始化优先级进度条

    for (i = 1; i <= controller->prio[0]; i++)
    {
        for (int j = 0; j < LINE; j++)
            line(x - (width * i + LINE * (i - 1) + j), y, x - (width * i + LINE * (i - 1) + j), y + LEN, RGB(Cyan1[0], Cyan1[1], Cyan1[2]));
    }

    for (i = 1; i <= controller->prio[1]; i++)
    {
        for (int j = 0; j < LINE; j++)
            line(x + (width * i + LINE * (i - 1) + j), y, x + (width * i + LINE * (i - 1) + j), y + LEN, RGB(Yellow1[0], Yellow1[1], Yellow1[2]));
    }

    // 等待键盘输入，控制优先级
    do
    {
        if (controller->wait[0] ==0 && controller->wait[1] ==0 )
            break;
        key = getchar();
        if (key == UP && controller->prio[0] < 20)
        {
            setpriority(controller->tid[0], getpriority(controller->tid[0]) - PRIO_WEIGHT);
            i = ++controller->prio[0];
            for (int j = 0; j < LINE; j++)
                line(x - (width * i + LINE * (i - 1) + j), y, x - (width * i + LINE * (i - 1) + j), y + LEN, RGB(Cyan1[0], Cyan1[1], Cyan1[2]));
        }
        else if (key == DOWN && controller->prio[0] > 1)
        {
            i = controller->prio[0]--;
            for (int j = 0; j < LINE; j++)
                line(x - (width * i + LINE * (i - 1) + j), y, x - (width * i + LINE * (i - 1) + j), y + LEN, RGB(0, 0, 0));
            setpriority(controller->tid[0], getpriority(controller->tid[0]) + PRIO_WEIGHT);
        }
        else if (key == RIGHT && controller->prio[1] < 20)
        {
            setpriority(controller->tid[1], getpriority(controller->tid[1]) - PRIO_WEIGHT);
            i = ++controller->prio[1];
            for (int j = 0; j < LINE; j++)
                line(x + (width * i + LINE * (i - 1) + j), y, x + (width * i + LINE * (i - 1) + j), y + LEN, RGB(Yellow1[0], Yellow1[1], Yellow1[2]));
        }
        else if (key == LEFT && controller->prio[1] > 1)
        {
            i = controller->prio[1]--;
            for (int j = 0; j < LINE; j++)
                line(x + (width * i + LINE * (i - 1) + j), y, x + (width * i + LINE * (i - 1) + j), y + LEN, RGB(0, 0, 0));
            setpriority(controller->tid[1], getpriority(controller->tid[1]) + PRIO_WEIGHT);
        }
        else
            continue;
    } while (1);

    task_exit(0);
}

/**
 * 第一个运行在用户模式的线程所执行的函数
 */
void main(void *pv)
{
    struct data arr[TASK_NUM];  // 定义数据结构体变量数组
    struct tsk_prio controller; // 定义控制器结构体
    int tid_controller;         // 定义控制线程id
    int mode = 0x144;           // 图形界面模式，不同的分辨率
    int temp[N];
    int i, k;

    //printf("task #%d: I'm the first user task(pv=0x%08x)!\r\n",task_getid(), pv);

    //TODO: Your code goes here

    /**
	 *  申请线程栈
	 */
    unsigned char *stack_left, *stack_right, *stack_controller;
    unsigned int stack_size = 1024 * 1024;
    stack_left = (unsigned char *)malloc(stack_size);
    stack_right = (unsigned char *)malloc(stack_size);
    stack_controller = (unsigned char *)malloc(stack_size);

    /**
	 * 数据初始化
	 */
    // 播种
    srand(time(NULL));

    // 产生N个随机数，范围为：1-MAX
    for (i = 0; i < N; i++)
        temp[i] = (rand() % MAX) + 1;
    for (k = 0; k < TASK_NUM; k++)
    {
        arr[k].left = 0;
        arr[k].right = N - 1;
        arr[k].length = N;
        // 对arr赋值
        for (i = 0; i < N; i++)
            arr[k].array[i] = temp[i];
    }

    // 测试键盘输入字符
    /*
    int key;
    while (1)
    {
        key = getchar();
        printf("%x", key);
    }*/

    /**
	 * 进入图形模式
	 */
    init_graphic(mode);

    /**
	 *  创建线程
	 */
    // 将左右线程tid复制给controller结构体
    controller.tid[0] = task_create(stack_left + stack_size, &tsk_left, (void *)&arr[0]);
    controller.tid[1] = task_create(stack_right + stack_size, &tsk_right, (void *)&arr[1]);

    // 初始化左右线程的静态优先级，左右两线程的tid相等，初始均为36，每一次增加2
    controller.prio[0] = 1;
    controller.prio[1] = 1;
    setpriority(controller.tid[0], MAX_PRIO - PRIO_WEIGHT * controller.prio[0]);
    setpriority(controller.tid[1], MAX_PRIO - PRIO_WEIGHT * controller.prio[1]);

    // 设置等待返回值初值为1，线程返回则为0
    controller.wait[0] = 1;
    controller.wait[1] = 1;

    // 创建控制线程
    tid_controller = task_create(stack_controller + stack_size, &tsk_controller, (void *)&controller);

    // 初始化控制线程的优先级最高
    setpriority(tid_controller, MIN_PRIO);

    /**
	 * 等待控制线程结束
	 */
    controller.wait[0] = task_wait(controller.tid[0], NULL);
    controller.wait[1] = task_wait(controller.tid[1], NULL);

    task_wait(tid_controller, NULL);

    // 测试调度器
    //for (i = 0; i < N; i++)
    //    printf("%d", temp[i]);
    //printf("left: %d; right: %d\n", getpriority(controller.tid[0]), getpriority(controller.tid[1]));

    free(stack_left);
    free(stack_right);
    free(stack_controller);

    while (1)
        ;

    /**
	 *  退出图形模式
	 */
    exit_graphic();

    task_exit(0);
}
