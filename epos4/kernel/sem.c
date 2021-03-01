/**
 * vim: filetype=c:fenc=utf-8:ts=4:et:sw=4:sts=4
 */
#include <stddef.h>
#include "kernel.h"

struct semaphore *g_sem_head=NULL;

static void add_sem(struct semaphore *sem)
{
    if (g_sem_head == NULL)
        g_sem_head = sem;
    else
    {
        struct semaphore *p, *q;
        p = g_sem_head;
        do
        {
            q = p;
            p = p->next;
        } while (p != NULL);
        q->next = sem;
    }
}

static
void remove_sem(struct semaphore *sem)
{
    if(g_sem_head != NULL) {
        if(sem == g_sem_head) {
            g_sem_head = g_sem_head->next;
        } else {
            struct semaphore *p, *q;
            p = g_sem_head;
            do {
                q = p;
                p = p->next;
                if(p == sem)
                    break;
            } while(p != NULL);

            if(p == sem)
                q->next = p->next;
        }
    }
}

static
struct semaphore* get_sem(int semid)
{
    struct semaphore *sem;

    sem = g_sem_head;
    while(sem != NULL) {
        if(sem->semid == semid)
            break;
        sem = sem->next;
    }

    return sem;
}

/**
 * value是信号量的初值
 * 分配内存要用kmalloc，不能用malloc！
 * 成功返回信号量ID，否则返回-1
 */

int sys_sem_create(int value)
{
    static int semid = 0; // 设置semid初值为0
    struct semaphore *new = (struct semaphore *)kmalloc(sizeof(struct semaphore));
    uint32_t flags;

    new->value = value;
    new->semid = semid++;
    new->next = NULL;
    new->wq = NULL;

    save_flags_cli(flags);
    add_sem(new);
    restore_flags(flags);

    if (new == NULL)
        return -1;
    return new->semid;
}

/**
 * 释放内存要用kfree，不能用free！
 * 成功返回0，否则返回-1
 */

int sys_sem_destroy(int semid)
{
    struct semaphore *sem;
    uint32_t flags;
    sem = get_sem(semid);
    if(sem==NULL)
        return -1;
    save_flags_cli(flags);
    remove_sem(sem);
    restore_flags(flags);
    kfree(sem);
    return 0;
}

/**
 * P操作，要用save_flags_cli/restore_flags保证原子性
 * 成功返回0，否则返回-1
 */

int sys_sem_wait(int semid)
{
    struct semaphore *sem;
    uint32_t flags;
    sem = get_sem(semid);
    if(sem==NULL)
        return -1;
    save_flags_cli(flags);
    sem->value--;
    if(sem->value<0)
    {
        sleep_on(&sem->wq);         // sleep_on的参数类型为指向指针的指针
    }
    restore_flags(flags);
    return 0;
}

/**
 * V操作，要用save_flags_cli/restore_flags保证原子性
 * 成功返回0，否则返回-1
 */

int sys_sem_signal(int semid)
{
    struct semaphore *sem;
    uint32_t flags;
    sem = get_sem(semid);
    if(sem==NULL)
        return -1;
    save_flags_cli(flags);
    sem->value++;
    if(sem->value<=0)
    {
        wake_up(&sem->wq, 1);       // wake_up的参数类型为指向指针的指针
    }
    restore_flags(flags);
    return 0;
}