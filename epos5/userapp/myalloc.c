/**
 * vim: filetype=c:fenc=utf-8:ts=4:et:sw=4:sts=4
 */
#include <sys/types.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <syscall.h>
#include <stdio.h>

int mutex;

struct chunk
{
  char signature[4];  /* "OSEX" */
  struct chunk *next; /* ptr. to next chunk */
  int state;          /* 0 - free, 1 - used */
#define FREE 0
#define USED 1

  int size; /* size of this chunk */
};

static struct chunk *chunk_head;

void *g_heap;
void *tlsf_create_with_pool(uint8_t *heap_base, size_t heap_size)
{
  chunk_head = (struct chunk *)heap_base;
  strncpy(chunk_head->signature, "OSEX", 4);
  chunk_head->next = NULL;
  chunk_head->state = FREE;
  chunk_head->size = heap_size;

  return NULL;
}

/**
 * 功能：
 * 分配大小为size字节的内存块，并返回块起始地址
 * 如果size是0，返回NULL
 */
void *malloc(size_t size)
{
  //如果size为0，返回NULL
  if (size == 0)
    return NULL;
  // 保护临界区
  sem_wait(mutex);
  //2.根据首次原则找到符合条件的select指针,(select->state == FREE) && ((select->size - sizeof(struct chunk)) >= size)
  struct chunk *select = chunk_head;
  while (select != NULL)
  {
    if ((select->state == FREE) && ((select->size - sizeof(struct chunk)) >= size))
      break;
    select = select->next;
  }
  // 剩余空间不足，返回NULL，待修改
  if (select == NULL)
  {
    sem_signal(mutex);
    return NULL;
  }
  //如果减去参数size+sizeof(struct chunk)，剩余空间大于sizeof(struct chunk),创建新的chunk
  if ((select->size - sizeof(struct chunk) - size) > sizeof(struct chunk))
  {
    struct chunk *new = (struct chunk *)(((uint8_t *)select) + sizeof(struct chunk) + size);
    // 初始化new
    new->next = select->next;
    new->state = FREE;
    new->size = select->size - sizeof(struct chunk) - size;
    strncpy(new->signature, "OSEX", 4);
    // 修改select
    select->size = sizeof(struct chunk) + size;
    select->next = new;
  }
  //修改select->state=USED;
  select->state = USED;
  sem_signal(mutex);
  //返回ptr=((uint8_t *)select)+sizeof(struct chunk)
  uint8_t *ptr = ((uint8_t *)select) + sizeof(struct chunk);
  return (void *)ptr;
}

/**
 * 功能：
 * 释放ptr指向的内存块
 * 如果ptr是NULL，直接返回
 * 提示：
 * 根据ptr得到chunk：
 * struct trunk *achunk=(struct chunk *)(((uint8_t *)ptr)-sizeof(struct chunk));
 * 要求：
 * 必须验证ptr的有效性：判断achunk->signature是否等于“OSEX”
 * 必须合并相邻的空闲块
 */
void free(void *ptr)
{
  //如果ptr是NULL，直接返回
  if (ptr == NULL)
    return;
  // 保护临界区
  sem_wait(mutex);
  //根据ptr得到chunk地址struct chunk* achunk=(struct chunk *)(((uint8_t *)ptr)-sizeof(struct chunk));
  struct chunk *achunk = (struct chunk *)(((uint8_t *)ptr) - sizeof(struct chunk));
  //检验achunk->signature是否等于“OSEX”,strncmp(achunk->signature, "OSEX",4)==0
  if (strncmp(achunk->signature, "OSEX", 4) != 0){
    sem_signal(mutex);
    return;
  }
  //合并相邻的空闲内存块，检查achunk前后是否为FREE：如果为FREE，合并chunk
  //如果achunk不是最后一个chunk，检查achunk后一个内存块
  if (achunk->next != NULL)
  {
    if (achunk->next->state == FREE)
    {
      achunk->size = achunk->size + achunk->next->size;
      achunk->next = achunk->next->next;
    }
  }
  //如果achunk不是chunk_head，检查achunk的前一个内存块
  if (achunk != chunk_head)
  {
    struct chunk *select = chunk_head;
    while (select->next != NULL)
    {
      if (select->next == achunk)
        break;
      select = select->next;
    }
    if (select->state == FREE)
    {
      select->size = select->size + achunk->size;
      select->next = achunk->next;
    }
  }
  //5.修改achunk->state=FREE
  achunk->state = FREE;
  sem_signal(mutex);
  return;
}

/**
 * 功能：
 * 为num个元素的数组分配内存，每个元素占size字节
 * 把分配的内存初始化成0
 */
void *calloc(size_t num, size_t size)
{
  //如果size或num为0，返回NULL
  if (num == 0 || size == 0)
    return NULL;
  // 保护临界区
  sem_wait(mutex);
  //根据首次原则找到符合条件的select指针,(select->state == FREE) && ((select->size - sizeof(struct chunk)) >= num*size)
  struct chunk *select = chunk_head;
  while (select != NULL)
  {
    if ((select->state == FREE) && ((select->size - sizeof(struct chunk)) >= num * size))
      break;
    select = select->next;
  }
  // 剩余空间不足，待修改
  if (select == NULL)
  {
    sem_signal(mutex);
    return NULL;
  }
  //如果减去参数size+sizeof(struct chunk)，剩余空间大于sizeof(struct chunk),创建新的chunk
  if ((select->size - sizeof(struct chunk) - num * size) > sizeof(struct chunk))
  {
    struct chunk *new = (struct chunk *)(((uint8_t *)select) + sizeof(struct chunk) + num * size);
    // 初始化new
    new->next = select->next;
    new->state = FREE;
    new->size = select->size - sizeof(struct chunk) - num *size;
    strncpy(new->signature, "OSEX", 4);
    // 修改select
    select->size = sizeof(struct chunk) + num * size;
    select->next = new;
  }
  select->state = USED;
  sem_signal(mutex);
  uint8_t *ptr = ((uint8_t *)select) + sizeof(struct chunk);
  //初始化分配的内存为0
  /*
  size_t i;
  for (i = 0; i < num * size; i++)
  {
    *((char *)ptr + i) = (char)'0';
  }*/
  memset(ptr, 0, num * size);
  //返回ptr
  return (void *)ptr;
}

/**
 * 功能:
 * 重新分配oldptr指向的内存块，新内存块有size字节
 * -如果oldptr是NULL，该函数等价于malloc(size)
 * -如果size是0，该函数等价于free(oldptr)
 * 把旧内存块的内容复制到新内存块
 * -如果新内存块比较小，只复制旧内存块的前面部分
 * -如果新内存块比较大，复制整个旧内存块，而且不用初始化多出来的那部分
 * 如果新内存块还在原来的地址oldptr，返回oldptr；否则返回新地址
 * 要求:
 * 必须验证oldptr的有效性
 * 必须合并相邻的空闲块
 */
void *realloc(void *oldptr, size_t size)
{
  //如果oldptr是NULL，该函数等价于malloc(size)
  if (oldptr == NULL)
    return malloc(size);
  //如果size是0，该函数等价于free(oldptr)
  if (size == 0)
  {
    free(oldptr);
    return NULL;
  }
  // 保护临界区
  sem_wait(mutex);
  //根据oldptr得到chunk地址struct chunk *achunk=(struct chunk *)(((uint8_t *)oldptr)-sizeof(struct chunk));
  struct chunk *achunk = (struct chunk *)(((uint8_t *)oldptr) - sizeof(struct chunk));
  //检验achunk->signature是否等于“OSEX”,strncmp(achunk->signature, "OSEX",4)==0
  if (strncmp(achunk->signature, "OSEX", 4) != 0)
  {
    sem_signal(mutex);
    return NULL;
  }
  //修改achunk->state=FREE;
  achunk->state = FREE;
  //合并相邻的空闲内存块，检查achunk前后是否为FREE：如果为FREE，合并chunk
  //如果achunk不是最后一个chunk，检查achunk后一个内存块
  if (achunk->next != NULL)
  {
    if (achunk->next->state == FREE)
    {
      achunk->size = achunk->size + achunk->next->size;
      achunk->next = achunk->next->next;
    }
  }
  //如果achunk不是chunk_head，检查achunk的前一个内存块
  if (achunk != chunk_head)
  {
    struct chunk *select = chunk_head;
    while (select->next != NULL)
    {
      if (select->next == achunk)
        break;
      select = select->next;
    }
    if (select->state == FREE)
    {
      select->size = select->size + achunk->size;
      select->next = achunk->next;
    }
  }
  //根据首次原则找到符合条件的select指针,(select->state == FREE) && ((select->size - sizeof(struct chunk)) >= size)
  struct chunk *select = chunk_head;
  while (select != NULL)
  {
    if ((select->state == FREE) && ((select->size - sizeof(struct chunk)) >= size))
      break;
    select = select->next;
  }
  // 剩余空间不足，返回NULL，待修改
  if (select == NULL)
  {
    sem_signal(mutex);
    return NULL;
  }
  //如果减去参数size+sizeof(struct chunk)，剩余空间大于sizeof(struct chunk),创建新的chunk
  if ((select->size - sizeof(struct chunk) - size) > sizeof(struct chunk))
  {
    struct chunk *new = (struct chunk *)(((uint8_t *)select) + sizeof(struct chunk) + size);
    // 初始化new
    new->next = select->next;
    new->state = FREE;
    new->size = select->size - sizeof(struct chunk) - size;
    strncpy(new->signature, "OSEX", 4);
    // 修改select
    select->size = sizeof(struct chunk) + size;
    select->next = new;
  }
  //修改select->state=USED;
  select->state = USED;
  //计算ptr= ((uint8_t *)select) + sizeof(struct chunk);
  uint8_t *ptr = ((uint8_t *)select) + sizeof(struct chunk);
  //比较ptr和oldptr，相等则直接返回
  if ((void *)ptr == oldptr)
  {
    sem_signal(mutex);
    return oldptr;
  }
  //如果新内存块比较小，只复制旧内存块的前面部分,strncpy((char *)ptr, (char *)oldptr, select->size - sizeof(struct chunk));
  //如果新内存块比较大，复制整个旧内存块，而且不用初始化多出来的那部分,strncpy((char *)ptr, (char *)oldptr, achunk->size - sizeof(struct chunk));
  if (select->size < achunk->size)
    strncpy((char *)ptr, (char *)oldptr, select->size - sizeof(struct chunk));
  else
    strncpy((char *)ptr, (char *)oldptr, achunk->size - sizeof(struct chunk));
  sem_signal(mutex);
  return (void *)ptr;
}

/*************D O  N O T  T O U C H  A N Y T H I N G  B E L O W*************/
static void tsk_malloc(void *pv)
{
  int i, c = (int)pv;
  char **a = malloc(c * sizeof(char *));
  for (i = 0; i < c; i++)
  {
    a[i] = malloc(i + 1);
    a[i][i] = 17;
  }
  for (i = 0; i < c; i++)
  {
    free(a[i]);
  }
  free(a);

  task_exit(0);
}

#define MESSAGE(foo) printf("%s, line %d: %s", __FILE__, __LINE__, foo)
void test_allocator()
{
  char *p, *q, *t;
  mutex = sem_create(1);
  
  MESSAGE("[1] Test malloc/free for unusual situations\r\n");

  MESSAGE("  [1.1]  Allocate small block ... ");
  p = malloc(17);
  if (p == NULL)
  {
    printf("FAILED\r\n");
    return;
  }
  p[0] = p[16] = 17;
  printf("PASSED\r\n");

  MESSAGE("  [1.2]  Allocate big block ... ");
  q = malloc(4711);
  if (q == NULL)
  {
    printf("FAILED\r\n");
    return;
  }
  q[4710] = 47;
  printf("PASSED\r\n");

  MESSAGE("  [1.3]  Free big block ... ");
  free(q);
  printf("PASSED\r\n");

  MESSAGE("  [1.4]  Free small block ... ");
  free(p);
  printf("PASSED\r\n");

  MESSAGE("  [1.5]  Allocate huge block ... ");
  q = malloc(32 * 1024 * 1024 - sizeof(struct chunk));
  if (q == NULL)
  {
    printf("FAILED\r\n");
    return;
  }
  q[32 * 1024 * 1024 - sizeof(struct chunk) - 1] = 17;
  free(q);
  printf("PASSED\r\n");

  MESSAGE("  [1.6]  Allocate zero bytes ... ");
  if ((p = malloc(0)) != NULL)
  {
    printf("FAILED\r\n");
    return;
  }
  printf("PASSED\r\n");

  MESSAGE("  [1.7]  Free NULL ... ");
  free(p);
  printf("PASSED\r\n");

  MESSAGE("  [1.8]  Free non-allocated-via-malloc block ... ");
  int arr[5] = {0x55aa4711, 0x5a5a1147, 0xa5a51471, 0xaa551741, 0x5aa54171};
  free(&arr[4]);
  if (arr[0] == 0x55aa4711 &&
      arr[1] == 0x5a5a1147 &&
      arr[2] == 0xa5a51471 &&
      arr[3] == 0xaa551741 &&
      arr[4] == 0x5aa54171)
  {
    printf("PASSED\r\n");
  }
  else
  {
    printf("FAILED\r\n");
    return;
  }

  MESSAGE("  [1.9]  Various allocation pattern ... ");
  int i;
  size_t pagesize = sysconf(_SC_PAGESIZE);
  for (i = 0; i < 7411; i++)
  {
    p = malloc(pagesize);
    p[pagesize - 1] = 17;
    q = malloc(pagesize * 2 + 1);
    q[pagesize * 2] = 17;
    t = malloc(1);
    t[0] = 17;
    free(p);
    free(q);
    free(t);
  }

  char **a = malloc(2741 * sizeof(char *));
  for (i = 0; i < 2741; i++)
  {
    a[i] = malloc(i + 1);
    a[i][i] = 17;
  }
  for (i = 0; i < 2741; i++)
  {
    free(a[i]);
  }
  free(a);

  if (chunk_head->next != NULL || chunk_head->size != 32 * 1024 * 1024)
  {
    printf("FAILED\r\n");
    return;
  }
  printf("PASSED\r\n");

  MESSAGE("  [1.10] Allocate using calloc ... ");
  int *x = calloc(17, 4);
  for (i = 0; i < 17; i++)
    if (x[i] != 0)
    {
      printf("FAILED\r\n");
      return;
    }
    else
      x[i] = i;
  free(x);
  printf("PASSED\r\n");

  MESSAGE("[2] Test realloc() for unusual situations\r\n");

  MESSAGE("  [2.1]  Allocate 17 bytes by realloc(NULL, 17) ... ");
  p = realloc(NULL, 17);
  if (p == NULL)
  {
    printf("FAILED\r\n");
    return;
  }
  p[0] = p[16] = 17;
  printf("PASSED\r\n");
  MESSAGE("  [2.2]  Increase size by realloc(., 4711) ... ");
  p = realloc(p, 4711);
  if (p == NULL)
  {
    printf("FAILED\r\n");
    return;
  }
  if (p[0] != 17 || p[16] != 17)
  {
    printf("FAILED\r\n");
    return;
  }
  p[4710] = 47;
  printf("PASSED\r\n");

  MESSAGE("  [2.3]  Decrease size by realloc(., 17) ... ");
  p = realloc(p, 17);
  if (p == NULL)
  {
    printf("FAILED\r\n");
    return;
  }
  if (p[0] != 17 || p[16] != 17)
  {
    printf("FAILED\r\n");
    return;
  }
  printf("PASSED\r\n");

  MESSAGE("  [2.4]  Free block by realloc(., 0) ... ");
  p = realloc(p, 0);
  if (p != NULL)
  {
    printf("FAILED\r\n");
    return;
  }
  else
    printf("PASSED\r\n");

  MESSAGE("  [2.5]  Free block by realloc(NULL, 0) ... ");
  p = realloc(realloc(NULL, 0), 0);
  if (p != NULL)
  {
    printf("FAILED\r\n");
    return;
  }
  else
    printf("PASSED\r\n");

  MESSAGE("[3] Test malloc/free for thread-safe ... ");

  int t1, t2;
  char *s1 = malloc(1024 * 1024),
       *s2 = malloc(1024 * 1024);
  t1 = task_create(s1 + 1024 * 1024, tsk_malloc, (void *)5000);
  t2 = task_create(s2 + 1024 * 1024, tsk_malloc, (void *)5000);
  task_wait(t1, NULL);
  task_wait(t2, NULL);
  free(s1);
  free(s2);

  if (chunk_head->next != NULL || chunk_head->size != 32 * 1024 * 1024)
  {
    printf("FAILED\r\n");
    return;
  }
  printf("PASSED\r\n");
}
/*************D O  N O T  T O U C H  A N Y T H I N G  A B O V E*************/
