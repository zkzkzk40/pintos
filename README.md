pintost: thread代码实现

pintosu: userprog代码实现

# Thread

## 实验目标
![img](https://gitee.com/zkzkzk40/drawing-bed/raw/master/image/pintos/image.png)
简单来说,给出了一个功能最小的线程系统。我们的任务是扩展此系统的功能，以更好地理解同步问题。在此分配的“threads”目录中工作，而在“devices”目录中则有一些工作。编译应该在“threads”目录中进行。

## 前期准备
## 任务1：Alarm Clock
### 任务内容
通过alarm测试集,主要就是将线程的忙等待改为非忙等待,也就是说,一开始的线程在CPU时间到了后并不会转入就绪队列等待,而是直接进入下一次运行,这会导致其他的线程进不去CPU从而导致等待,直到该进程结束,其他进程才可以进入CPU执行.
所以说,为了让线程的忙等待改为非忙等待,我们需要改进当前的数据结构和算法完成以上的任务目标.
因此,在查看pintos.pdf的教程和代码时,它提示我们需要实现timer_sleep函数,所以任务一的目标是正确的实现该函数
### 源代码分析
在pintos.pdf中已经给出了详细的代码信息,其中最重要的是threads/thread.c,其主要的数据结构如下:
```c
struct thread
  {
    /* Owned by thread.c. */
    tid_t tid;                          /* 线程标识. */
    enum thread_status status;          /* 线程状态,包括就绪,等待,运行和结束四个状态. */
    char name[16];                      /* Name (for debugging purposes). */
    uint8_t *stack;                     /* Saved stack pointer. */
    int priority;                       /* Priority. */
    struct list_elem allelem;           /* List element for all threads list. */

    /* Shared between thread.c and synch.c. */
    struct list_elem elem;              /* List element. */

#ifdef USERPROG
    /* Owned by userprog/process.c. */
    uint32_t *pagedir;                  /* Page directory. */
#endif

    /* Owned by thread.c. */
    unsigned magic;                     /* Detects stack overflow. */
  };
```
在devices文件夹中还有个timer.c文件,其主要的函数如下:
```c
void timer_init (void);
void timer_calibrate (void);

int64_t timer_ticks (void);
int64_t timer_elapsed (int64_t);

/* Sleep and yield the CPU to other threads. */
void timer_sleep (int64_t ticks);
void timer_msleep (int64_t milliseconds);
void timer_usleep (int64_t microseconds);
void timer_nsleep (int64_t nanoseconds);

/* Busy waits. */
void timer_mdelay (int64_t milliseconds);
void timer_udelay (int64_t microseconds);
void timer_ndelay (int64_t nanoseconds);

void timer_print_stats (void);
```
在这里我们找到了需要实现的timer_sleep函数,其函数内容如下:
```cpp
void
timer_sleep (int64_t ticks) 
{
  int64_t start = timer_ticks ();

  ASSERT (intr_get_level () == INTR_ON);
  while (timer_elapsed (start) < ticks) 
    thread_yield ();
}
```
其中的thread_yield函数一开始的主要功能是将CPU当前线程立刻抛入执行队列,所以任务一的主要矛盾就出在这个函数中了.
函数代码如下:
```cpp
void
thread_yield (void) 
{
  struct thread *cur = thread_current ();
  enum intr_level old_level;
  
  ASSERT (!intr_context ());

  old_level = intr_disable ();
  if (cur != idle_thread) 
    list_push_back (&ready_list, &cur->elem);
  cur->status = THREAD_READY;
  schedule ();
  intr_set_level (old_level);
}
```
### 问题解决思路
#### 怎么阻塞线程
##### 首先必须要修改thread数据结构,加入控制阻塞的变量,或者说在阻塞队列中停留的时间:
在thread.c文件中有一个比较重要的数据static int64_t ticks,这个变量是用于记录系统启动开始,当前系统执行单位时间的前进计量,它用在如下函数中
```cpp
int64_t
timer_ticks (void) 
{
  enum intr_level old_level = intr_disable ();
  int64_t t = ticks;
  intr_set_level (old_level);
  return t;
}
```
用于当前计数值的返回,然后在下面这个函数中
```cpp
int64_t
timer_elapsed (int64_t then) 
{
  return timer_ticks () - then;
}
```
用于计算和某个值的差
这个函数用在timer_sleep函数中
```cpp
void
timer_sleep (int64_t ticks) 
{
  int64_t start = timer_ticks ();

  ASSERT (intr_get_level () == INTR_ON);
  while (timer_elapsed (start) < ticks) 
    thread_yield ();
}
```
结合timer_sleep函数来看,那么很显然timer_elapsed函数的用处已经很明显了,即返回当前时间距离then的时间差.所以timer_sleep函数的用处首先是获取当前时间距离then的时间差,然后在这个时间差内调用thread_yield函数
简而言之,timer_sleep这个函数的实质就是在ticks的时间内不断执行thread_yield,将CPU当前线程立刻抛入执行队列.所以阻塞队列的实现就需要修改timer_sleep函数即可
##### 第二步,执行阻塞
源代码为我们提供了一个thread_block函数
```cpp
void
thread_block (void) 
{
  ASSERT (!intr_context ());
  ASSERT (intr_get_level () == INTR_OFF);

  thread_current ()->status = THREAD_BLOCKED;
  schedule ();
}
```
解析一下schedule函数
```cpp
// schedule()先把当前线程丢到就绪队列，
// 然后把线程切换如果下一个线程和当前线程不一样的话
static void
schedule (void) 
{
  struct thread *cur = running_thread ();
  struct thread *next = next_thread_to_run ();//获取下一个要run的线程
  struct thread *prev = NULL;

  // 3个断言
  // 确保不能被中断， 当前线程是RUNNING_THREAD等
  ASSERT (intr_get_level () == INTR_OFF);
  ASSERT (cur->status != THREAD_RUNNING);
  ASSERT (is_thread (next));

  if (cur != next)
    prev = switch_threads (cur, next);//调用switch_threads返回给prev。
  // 参数prev是NULL或者在下一个线程的上下文中的当前线程指针。
  thread_schedule_tail (prev);
}
```
#### 怎么将线程从阻塞状态中唤醒
##### 首先思考要决定谁被唤醒
前面我们提到了ticks这个变量,我们可以为每一个线程添加一个新的成员变量ticks,每次CPU时间到了后进程切换的时候,这个ticks变量都是不一样的,所以每个线程的ticks值都是不一样的,当时间到了0后就将该线程唤醒
##### 第二步将如何将这个线程唤醒
源代码中有个时钟中断处理函数timer_interrupt,每次时钟中断时都会运行一次,代码如下
```cpp
static void
timer_interrupt (struct intr_frame *args UNUSED)
{
  ticks++;
  thread_tick ();
}
```
它每次运行都会将ticks值加一,即上文提到的时钟值,所以我们需要修改这一步的代码,将查找和唤醒需要被唤醒的线程代码加入进去
源代码中有个函数thread_foreach.这个函数是用于遍历每一个线程的函数,代码如下
```cpp
void
thread_foreach (thread_action_func *func, void *aux)
{
  struct list_elem *e;

  ASSERT (intr_get_level () == INTR_OFF);

  for (e = list_begin (&all_list); e != list_end (&all_list);
       e = list_next (e))
    {
      struct thread *t = list_entry (e, struct thread, allelem);
      func (t, aux);
    }
}
```
thread_foreach函数的第一个参数thread_action_func *func是自定义的函数名称,传入函数名称,然后在后面执行这个函数,这就给我们修改代码带来了便利,思路如下:
这个函数遍历了所有线程,这里有一个很重要的全局变量all_list,它是所有线程的集合,每次时钟中断时将会遍历一次这个集合
```cpp
/* List of all processes.  Processes are added to this list
   when they are first scheduled and removed when they exit. */
static struct list all_list;
```
所以在这里我们就需要修改它所调用的自定义函数func即可,思路如下

   1. 新加自定义函数.用于检测线程的睡眠时间
   1. 将这个函数作为参数传入thread_foreach函数中
   1. 每次进程切换的中间都会执行timer_interrupt函数,在这个函数调用thread_foreach检查所有线程.当检测到睡眠时间为0的线程就将它唤醒,抛入就绪队列
### 实现过程
#### 新增的数据结构：
在thread结构体中增加一个int64_t 类型的ticks_blocked,记录线程被阻塞的时间
```cpp
struts thread{
	//...
    int64_t ticks_blocked;
    //...
}
```
如图
![img](https://gitee.com/zkzkzk40/drawing-bed/raw/master/image/pintos/image-1641833140455.png)

#### 初始化
线程需要初始化,初始化的函数为thread_create,加入一句赋值语句即可,如下
![img](https://gitee.com/zkzkzk40/drawing-bed/raw/master/image/pintos/image-1641833154190.png)

#### blocked_thread_check函数
然后新加一个函数用于检测线程的睡眠时间,当这个时间为0时将其放回就绪队列中.
函数如下:
```cpp
void
blocked_thread_check (struct thread *t, void *aux UNUSED)
{
  if (t->status == THREAD_BLOCKED && t->ticks_blocked > 0)
  {
      t->ticks_blocked--;
      if (t->ticks_blocked == 0)
      {
          thread_unblock(t);
      }
  }
}
```
第二层if语句就是检查这个时间的,第一层if是用于检查线程是否被阻塞且时间大于零
接着的thread_unblock函数就是将线程放到就绪队列中
```cpp
void
thread_unblock (struct thread *t)
{
  enum intr_level old_level;

  ASSERT (is_thread (t));

  old_level = intr_disable ();
  ASSERT (t->status == THREAD_BLOCKED);
  list_push_back (&ready_list, &t->elem);
  t->status = THREAD_READY;
  intr_set_level (old_level);
}
```
#### timer_sleep函数
接着修改timer_sleep函数
```c
void
timer_sleep (int64_t ticks)
{
  if (ticks <= 0)
  {
    return;
  }
  ASSERT (intr_get_level () == INTR_ON);
  enum intr_level old_level = intr_disable ();
  struct thread *current_thread = thread_current ();//获取当前正在执行的线程
  current_thread->ticks_blocked = ticks;//设置改线程的ticks值
  thread_block ();//阻塞该线程
  intr_set_level (old_level);
}
```
这个函数原来是直接重新调度,将刚刚结束的线程直接放入运行态中,经过前面的分析,这里需要阻塞该线程
#### timer_interrupt函数
修改timer_interrupt函数
```c
static void
timer_interrupt (struct intr_frame *args UNUSED)
{
  ticks++;
  thread_foreach (blocked_thread_check, NULL);
  thread_tick ();
}
```
注意第4行代码调用了thread_foreach函数,参数为blocked_thread_check,它将在找到对应的线程后调用blocked_thread_check函数
## 任务2：Priority Scheduling
### 任务内容
![img](https://gitee.com/zkzkzk40/drawing-bed/raw/master/image/pintos/image-1641833172400.png)

![img](https://gitee.com/zkzkzk40/drawing-bed/raw/master/image/pintos/image-1641833172404.png)从官方文档的资料可知,本次任务的目标是:实现优先级调度
当一个线程被添加到比当前运行的线程优先级更高的就绪列表中时，当前线程应该立即将处理器释放到新线程。类似地，当线程正在等待锁、信号量或条件变量时，应该首先唤醒优先级最高的等待线程。线程可以在任何时候提高或降低自己的优先级，但是降低优先级，使线程不再具有最高优先级，必须使线程立即释放CPU。
线程优先级从PRI_min(0)到PRI_MAX(63)不等，较低的优先级对应于较低的优先级，因此优先级0是最低优先级，优先级63是最高的。初始线程优先级作为一个参数传递给线程_CREATE()。如果没有理由选择另一个优先级，请使用PRI_DEFAULT(31)。PRI_宏是在‘线程/线程.h’中定义的，您不应该更改它们的值。
优先级调度的一个问题是“优先级反转”。请分别考虑高、中和低优先级线程H、M和L。如果H需要等待L(例如，对于L持有的锁)，而M在就绪列表中，则H将永远得不到CPU，因为低优先级线程不会有任何CPU时间。解决这个问题的部分修复方法是在L持有锁时将其优先级“捐赠”给L，然后在释放(从而H获得)锁时回忆捐赠。
实施优先捐赠。您将需要说明所有不同的情况，在这些情况下，优先捐款是需要的。一定要处理多个捐赠，其中多个项目被捐赠给一个线程。您还必须处理嵌套捐赠：如果H在等待M持有的锁，而M正在等待L持有的锁，那么M和L都应该被提升到H的优先级。如果有必要，您可以对嵌套优先级捐赠的深度施加合理的限制，例如8级。
(以上内容来自QQ的OCR和中英文翻译)

### 源代码分析
#### 有关优先级调度的代码
##### thread_unblock函数
任务一中提到的thread_unblock函数,作用是将线程放到就绪队列中
```cpp
void
thread_unblock (struct thread *t)
{
  enum intr_level old_level;

  ASSERT (is_thread (t));

  old_level = intr_disable ();
  ASSERT (t->status == THREAD_BLOCKED);
  list_push_back (&ready_list, &t->elem);
  t->status = THREAD_READY;
  intr_set_level (old_level);
}
```
这个函数用到了两处比较重要的数据结构和函数,一个是ready_list,顾名思义,这个是维护就绪队列的一个链表,其定义如下
```c
/* List of processes in THREAD_READY state, that is, processes
   that are ready to run but not actually running. */
static struct list ready_list;
```
还有一个函数list_push_back,它有两个参数,一个是要插入的链表指针,一个是要插入的线程的节点数据的结构体
```c
void
list_push_back (struct list *list, struct list_elem *elem)
{
  list_insert (list_end (list), elem);
}

void
list_insert (struct list_elem *before, struct list_elem *elem)
{
  ASSERT (is_interior (before) || is_tail (before));
  ASSERT (elem != NULL);

  elem->prev = before->prev;
  elem->next = before;
  before->prev->next = elem;
  before->prev = elem;
}
```
节点数据信息如下
```c
    struct list_elem allelem;           /* List element for all threads list. */

    /* Shared between thread.c and synch.c. */
    struct list_elem elem;              /* List element. */
```
```c
/* List element. */
struct list_elem 
  {
    struct list_elem *prev;     /* Previous list element. */
    struct list_elem *next;     /* Next list element. */
  };
```
所以很明显thread_unblock函数就是简单粗暴地将线程插入到就绪队列的尾部
既然pintos源代码(src\lib\kernel\list.h)给我们实现了一个链表数据结构,那么不妨来看看其他的具体的函数实现
```c
/* List element. */
struct list_elem 
  {
    struct list_elem *prev;     /* Previous list element. */
    struct list_elem *next;     /* Next list element. */
  };

/* List. */
struct list 
  {
    struct list_elem head;      /* List head. */
    struct list_elem tail;      /* List tail. */
  };

/* Converts pointer to list element LIST_ELEM into a pointer to
   the structure that LIST_ELEM is embedded inside.  Supply the
   name of the outer structure STRUCT and the member name MEMBER
   of the list element.  See the big comment at the top of the
   file for an example. */
#define list_entry(LIST_ELEM, STRUCT, MEMBER)           \
        ((STRUCT *) ((uint8_t *) &(LIST_ELEM)->next     \
                     - offsetof (STRUCT, MEMBER.next)))

/* List initialization.

   A list may be initialized by calling list_init():

       struct list my_list;
       list_init (&my_list);

   or with an initializer using LIST_INITIALIZER:

       struct list my_list = LIST_INITIALIZER (my_list); */
#define LIST_INITIALIZER(NAME) { { NULL, &(NAME).tail }, \
                                 { &(NAME).head, NULL } }

void list_init (struct list *);

/* List traversal. */
struct list_elem *list_begin (struct list *);
struct list_elem *list_next (struct list_elem *);
struct list_elem *list_end (struct list *);

struct list_elem *list_rbegin (struct list *);
struct list_elem *list_prev (struct list_elem *);
struct list_elem *list_rend (struct list *);

struct list_elem *list_head (struct list *);
struct list_elem *list_tail (struct list *);

/* List insertion. */
void list_insert (struct list_elem *, struct list_elem *);
void list_splice (struct list_elem *before,
                  struct list_elem *first, struct list_elem *last);
void list_push_front (struct list *, struct list_elem *);
void list_push_back (struct list *, struct list_elem *);

/* List removal. */
struct list_elem *list_remove (struct list_elem *);
struct list_elem *list_pop_front (struct list *);
struct list_elem *list_pop_back (struct list *);

/* List elements. */
struct list_elem *list_front (struct list *);
struct list_elem *list_back (struct list *);

/* List properties. */
size_t list_size (struct list *);
bool list_empty (struct list *);

/* Miscellaneous. */
void list_reverse (struct list *);
/* Compares the value of two list elements A and B, given
   auxiliary data AUX.  Returns true if A is less than B, or
   false if A is greater than or equal to B. */
typedef bool list_less_func (const struct list_elem *a,
                             const struct list_elem *b,
                             void *aux);

/* Operations on lists with ordered elements. */
void list_sort (struct list *,
                list_less_func *, void *aux);
void list_insert_ordered (struct list *, struct list_elem *,
                          list_less_func *, void *aux);
void list_unique (struct list *, struct list *duplicates,
                  list_less_func *, void *aux);

/* Max and min. */
struct list_elem *list_max (struct list *, list_less_func *, void *aux);
struct list_elem *list_min (struct list *, list_less_func *, void *aux);
```
绝大多数的函数都和C语言给出的链表数据结构差不多,上面的函数基本上都可以从名称中看出函数的具体作用.
重点看下list_insert_ordered函数
```c
/* Inserts ELEM in the proper position in LIST, which must be
   sorted according to LESS given auxiliary data AUX.
   Runs in O(n) average case in the number of elements in LIST. */
void
list_insert_ordered (struct list *list, struct list_elem *elem,
                     list_less_func *less, void *aux)
{
  struct list_elem *e;

  ASSERT (list != NULL);
  ASSERT (elem != NULL);
  ASSERT (less != NULL);

  for (e = list_begin (list); e != list_end (list); e = list_next (e))
    if (less (elem, e, aux))
      break;
  return list_insert (e, elem);
}
```
这两个函数的作用是根据使用者传入的比较函数less和参数elem来找到对应的需要插入的节点位置,然后将节点插入
在编译器中寻找list_push_back可以发现不止是thread_unblock函数用到了,还有另外几处需要修改的地方
##### init_thread函数
![img](https://gitee.com/zkzkzk40/drawing-bed/raw/master/image/pintos/image-1641833195094.png)
##### thread_yield函数

![img](https://gitee.com/zkzkzk40/drawing-bed/raw/master/image/pintos/image-1641833202935.png)

src\threads\synch.h中给出了lock结构体,它被thread结构体所调用,这个结构体有许多PV相关的函数,如下
```c
struct semaphore 
  {
    unsigned value;             /* Current value. */
    struct list waiters;        /* List of waiting threads. */
  };

void sema_init (struct semaphore *, unsigned value);
void sema_down (struct semaphore *);
bool sema_try_down (struct semaphore *);
void sema_up (struct semaphore *);
void sema_self_test (void);

/* Lock. */
struct lock 
  {
    struct thread *holder;      /* Thread holding lock (for debugging). */
    struct semaphore semaphore; /* Binary semaphore controlling access. */
  };

void lock_init (struct lock *);
void lock_acquire (struct lock *);
bool lock_try_acquire (struct lock *);
void lock_release (struct lock *);
bool lock_held_by_current_thread (const struct lock *);

/* Condition variable. */
struct condition 
  {
    struct list waiters;        /* List of waiting threads. */
  };

void cond_init (struct condition *);
void cond_wait (struct condition *, struct lock *);
void cond_signal (struct condition *, struct lock *);
void cond_broadcast (struct condition *, struct lock *);
```
### 问题解决思路
#### 如何实现优先级队列
按照源代码的思路,就绪队列实际上是一个普通的先进先出的队列而已,但我们要实现一个优先级队列,就必须要修改队列数据的插入方式,不能是之前那样简单粗暴的插入到队列末尾,而是根据优先级插入.	
所以要将原队列改为优先级队列.就需要改变他的插入方式,改成优先级地插入即可,所以需要修改源代码中的三处插入队列的函数,这三处分别是thread_yield,init_thread和thread_unblock函数
前面在分析源代码的时候已经发现,pintos给我们实现了list的数据结构,其中的list_insert_ordered函数是实现优先级队列的关键,为了满足这个函数的参数需求,我们还必须自定义一个队列节点类list_elem的数据结构的比较函数
#### 如何实现优先级调度
和上面实现优先级队列不同的是,实现优先级调度不能单单只改变线程的插入方式,因为一个线程的生命周期包括了创建,运行,就绪,阻塞,死亡五个状态.上面的思路只是单单实现了线程转为就绪态时插入优先队列的方法,要正确的实现优先级调度,必须完善其他状态,比如线程刚创建时或者重新设置优先级时进入就绪态,需要重新调整其就绪队列的顺序
那么线程在创建时调用了哪些函数呢?
一个是thread_create函数,这个函数在新的线程被创建时执行,用于初始化这个线程,所以我们需要修改这个函数:当新的线程的优先级大于当前运行态线程的优先级时,需要在代码中加入thread_yield函数的调用
thread_create源代码如下
```c
tid_t
thread_create (const char *name, int priority,
               thread_func *function, void *aux) 
{
  struct thread *t;
  struct kernel_thread_frame *kf;
  struct switch_entry_frame *ef;
  struct switch_threads_frame *sf;
  tid_t tid;

  ASSERT (function != NULL);

  /* Allocate thread. */
  t = palloc_get_page (PAL_ZERO);
  if (t == NULL)
    return TID_ERROR;

  /* Initialize thread. */
  init_thread (t, name, priority);
  tid = t->tid = allocate_tid ();

  /* Stack frame for kernel_thread(). */
  kf = alloc_frame (t, sizeof *kf);
  kf->eip = NULL;
  kf->function = function;
  kf->aux = aux;

  /* Stack frame for switch_entry(). */
  ef = alloc_frame (t, sizeof *ef);
  ef->eip = (void (*) (void)) kernel_thread;

  /* Stack frame for switch_threads(). */
  sf = alloc_frame (t, sizeof *sf);
  sf->eip = switch_entry;
  sf->ebp = 0;

  /* Add to run queue. */
  thread_unblock (t);

  return tid;
}
```
#### 如何实现优先级捐赠
什么是优先级捐赠?pintos官方文档给出的文档翻译如下
优先级调度的一个问题是“优先级反转”。请分别考虑高、中和低优先级线程H、M和L。如果H需要等待L(例如，对于L持有的锁)，而M在就绪列表中，则H将永远得不到CPU，因为低优先级线程不会有任何CPU时间。解决这个问题的部分修复方法是在L持有锁时将其优先级“捐赠”给L，然后在释放(从而H获得)锁时回忆捐赠。
实施优先捐赠。您将需要说明所有不同的情况，在这些情况下，优先捐款是需要的。一定要处理多个捐赠，其中多个项目被捐赠给一个线程。您还必须处理嵌套捐赠：如果H在等待M持有的锁，而M正在等待L持有的锁，那么M和L都应该被提升到H的优先级。如果有必要，您可以对嵌套优先级捐赠的深度施加合理的限制，例如8级。
为了理解,先来看下面这个例子
![img](https://gitee.com/zkzkzk40/drawing-bed/raw/master/image/pintos/image-1641833238865.png)
就绪队列中有线程AB,阻塞队列C,其优先级分别为1,32,64,优先级依次增大.因此,当前就绪队列的优先级应该是A<B
C进入就绪队列的时候,其优先级是最高的,所以进入就绪队列后是排在B前面,所以优先级依次是A<B<C.此外C还需要对某个资源进行加锁,但是在此之前A已经给这个资源加了锁,所以C就必须要等待A释放锁之后才可以继续,所以C就不能转入就绪队列了.因此前面分析过的优先级调度在这种情况下就失效了.所以就有了任务给出的优先级捐赠的问题
在上面这个例子中,线程C获取一个锁的时候， 拥有这个锁的线程A优先级比C低优先级,所以要提高A的优先级，然后在线程A释放掉这个锁之后改回原来的优先级,这就是优先级捐赠的思路
所以我们就必须考虑下面几个问题:
##### Q1:优先级谁捐赠给谁
在上面这个例子中是C捐赠给A的,很显然捐赠者是高优先级的对象,被捐赠者是低优先级的对象,但是操作系统运行时的情况往往比这更复杂.比如现在有多个线程C,D,E...N给某个资源加锁,但是这个资源已经被线程A加锁了,那么线程A应该接受谁的捐赠呢?回答是C,D,E...N中优先级最高的线程.A将获取他们中最高的优先级.当然,这些高优先级在捐赠的时候是挨个来的,并非一起捐赠
##### Q2:多个线程捐赠的时候,优先级在中间段的线程怎么变化
考虑如下情况:
就绪队列中有A,优先级为1,将要依次进入B和C,其优先级依次为32,64,ABC三个线程均需要占用同一个资源,给这个资源加锁
那么B在C进入队列后的优先级应该是和C一样了
在pintos官方文档中把这个叫做递归式捐赠,当然实际上这个问题如果仔细考虑的话也是能够思考出来的
##### Q3:如何将被捐献的线程的优先级还原
当某个线程被修改后,如果不用其他的变量保存这个数据的话,那它原来的值就会永远消失,所以在程序实现的时候,应该新加一个变量用于保存原有的优先级.
##### Q4:当多个线程给某个资源加锁的时候,这些线程被唤醒时该唤醒谁
考虑下面这个例子:
	ABC按照顺序给一个已经被占用的资源加锁,其优先级依次为1,64,32,那么这三个线程的排序顺序应该也是需要按照优先队列来排序,所以我们需要新加入一个变量来保存这个队列,用于记录捐赠过优先级的线程
##### Q5:线程进行优先级捐赠后,转入哪个队列?何时唤醒?
线程A对某资源加锁,线程B试图对该资源加锁,失败,所以需要进行优先级捐赠.线程B进行优先级捐赠后,应该将它转入阻塞队列.但是问题在于这次转入阻塞队列是因为线程加了锁导致的,而不是其他原因比如I/O操作(举例,和pintos无关)等,那么当线程释放线程锁后,就需要一个操作来将它唤醒
所以这里需要考虑用操作系统最为经典的PV操作进行,而pintos已经为我们实现了PV操作的代码,当然考虑到Q4的分析,我们必须要将这个PV操作的等待唤醒队列改成优先级队列,每次唤醒都需要唤醒优先级最高的也就是队列首部的线程.
同样,因为要唤醒的线程需要考虑优先级,所以我们要修改PV的函数代码
### 实现过程
#### 优先队列的插入实现
前面提到的list_insert_ordered函数,它需要两个参数,一个是使用者传入的比较函数less,另一个是参数elem.源代码中并没有给出less函数的实现,所以我们需要自己实现一个比较函数,代码如下
```c
/*比较线程优先级 Priority compare function. */
bool
thread_cmp_priority (const struct list_elem *a, const struct list_elem *b, void *aux UNUSED)
{
  return list_entry(a, struct thread, elem)->priority > list_entry(b, struct thread, elem)->priority;
}

#define list_entry(LIST_ELEM, STRUCT, MEMBER)           \
        ((STRUCT *) ((uint8_t *) &(LIST_ELEM)->next     \
                     - offsetof (STRUCT, MEMBER.next)))
```
通过比较线程结构体中的priority实现大小的比较
![img](https://gitee.com/zkzkzk40/drawing-bed/raw/master/image/pintos/image-1641833257179.png)
接着修改thread_unblock函数

```c
void
thread_unblock (struct thread *t) 
{
//...
//   list_push_back (&all_list, &t->allelem);
  list_insert_ordered (&ready_list, &t->elem, (list_less_func *) &thread_cmp_priority, NULL);
//...
}
```
然后是修改init_thread函数
```c
static void
init_thread (struct thread *t, const char *name, int priority)
{
//...
//   list_push_back (&all_list, &t->allelem);
  list_insert_ordered (&all_list, &t->allelem, (list_less_func *) &thread_cmp_priority, NULL);
//...
}
```
最后是修改thread_yield函数
```c
void
thread_yield (void) 
{
//...
	list_insert_ordered (&ready_list, &cur->elem, (list_less_func *) &thread_cmp_priority, NULL);
    // list_push_back (&ready_list, &cur->elem);
//...
}
```
#### 优先级调度的实现
因为上文已经完成了优先级队列,实际上已经完成了大部分优先级调度的工作,所以需要修改的地方不多,修改thread_create函数如下:
```c
tid_t
thread_create (const char *name, int priority,
               thread_func *function, void *aux) 
{
	//上面的代码是初始化代码,不进行修改,省略
  /* Add to run queue. */
  thread_unblock (t);
    if (thread_current ()->priority < priority)
    {
        thread_yield ();
    }
  return tid;
}
```
#### 优先级捐赠的实现
修改thread结构体,加入如下数据结构
```c
    int base_priority;                  /* 最开始的优先级. */
    struct list locks;                  /* 准备给资源加线程锁的线程列表 */
    struct lock *lock_waiting;          /* 线程锁结构体 */
```
修改lock结构体,加入如下
```c
    struct list_elem elem;      /* 记录了为了获取该资源而进行优先级捐赠的进程的队列 */
    int max_priority;          /* 优先级捐赠的最大值 */
```
在thread中加入新的函数
![img](https://gitee.com/zkzkzk40/drawing-bed/raw/master/image/pintos/image-1641833272963.png)

```c
void
thread_donate_priority (struct thread *t)
{
  enum intr_level old_level = intr_disable ();
  thread_update_priority (t);

  if (t->status == THREAD_READY)
  {
    list_remove (&t->elem);
    list_insert_ordered (&ready_list, &t->elem, thread_cmp_priority, NULL);
  }
  intr_set_level (old_level);
}

void
thread_hold_the_lock(struct lock *lock)
{
  enum intr_level old_level = intr_disable ();
  list_insert_ordered (&thread_current ()->locks, &lock->elem, lock_cmp_priority, NULL);

  if (lock->max_priority > thread_current ()->priority)
  {
    thread_current ()->priority = lock->max_priority;
    thread_yield ();
  }
  intr_set_level (old_level);
}

void
thread_remove_lock (struct lock *lock)
{
  enum intr_level old_level = intr_disable ();
  list_remove (&lock->elem);
  thread_update_priority (thread_current ());
  intr_set_level (old_level);
}

void
thread_update_priority (struct thread *t)
{
  enum intr_level old_level = intr_disable ();
  int max_priority = t->base_priority;
  int lock_priority;

  if (!list_empty (&t->locks))
  {
    list_sort (&t->locks, lock_cmp_priority, NULL);
    lock_priority = list_entry (list_front (&t->locks), struct lock, elem)->max_priority;
    if (lock_priority > max_priority)
      max_priority = lock_priority;
  }

  t->priority = max_priority;
  intr_set_level (old_level);
}
```


给lock结构体加入新的函数lock_cmp_priority
```c
bool
lock_cmp_priority (const struct list_elem *a, const struct list_elem *b, void *aux UNUSED)
{
  return list_entry (a, struct lock, elem)->max_priority > list_entry (b, struct lock, elem)->max_priority;
}
```
修改原有函数sema_down,这个函数是用于加锁的,同时他需要保证等待队列是优先队列.然后将线程转入阻塞队列
```c
void
sema_down (struct semaphore *sema)
{
  enum intr_level old_level;

  ASSERT (sema != NULL);
  ASSERT (!intr_context ());

  old_level = intr_disable ();
  while (sema->value == 0)
    {
      list_insert_ordered (&sema->waiters, &thread_current ()->elem, thread_cmp_priority, NULL);
      thread_block ();
    }
  sema->value--;
  intr_set_level (old_level);
}
```
修改了sema_up函数,释放线程锁的时候需要将原来的等待队列按照线程原有的优先级进行排序​
```c
void
sema_up (struct semaphore *sema)
{
  enum intr_level old_level;

  ASSERT (sema != NULL);

  old_level = intr_disable ();
  if (!list_empty (&sema->waiters))
  {
    list_sort (&sema->waiters, thread_cmp_priority, NULL);
    thread_unblock (list_entry (list_pop_front (&sema->waiters), struct thread, elem));
  }

  sema->value++;
  thread_yield ();
  intr_set_level (old_level);
}
```
修改了lock_acquire函数,当线程调用该函数时,如果这个线程请求的资源已经被其他线程加锁了,则需要实现递归式的优先级捐赠,另外,需要用PV操作中的P操作阻塞线程,等待其他线程的V操作来唤醒.被唤醒后则说明这个资源的线程锁已被解开,这时需要更新当前线程的优先级,然后给资源加锁
```c
void
lock_acquire (struct lock *lock)
{
  struct thread *current_thread = thread_current ();
  struct lock *l;
  enum intr_level old_level;

  ASSERT (lock != NULL);
  ASSERT (!intr_context ());
  ASSERT (!lock_held_by_current_thread (lock));
  //如果资源已被其他线程加锁
  if (lock->holder != NULL && !thread_mlfqs)
  {
    current_thread->lock_waiting = lock;
    l = lock;
    //完成递归式捐赠优先级
    while (l && current_thread->priority > l->max_priority)
    {
      l->max_priority = current_thread->priority;
      thread_donate_priority (l->holder);
      l = l->holder->lock_waiting;
    }
  }
  //进程阻塞,等待进程释放锁来唤醒本进程
  sema_down (&lock->semaphore);

  old_level = intr_disable ();
  //需要修改当前线程
  current_thread = thread_current ();
  if (!thread_mlfqs)
  {
    //置空当前锁指针
    current_thread->lock_waiting = NULL;
    //修改线程锁指向的线程的优先级
    lock->max_priority = current_thread->priority;
    //加锁
    thread_hold_the_lock (lock);
  }
  lock->holder = current_thread;

  intr_set_level (old_level);
}
```
修改了lock_release函数,调用thread_remove_lock函数来更新当前线程占用的资源的线程锁的列表,同时使用PV操作中的V操作进行线程锁的释放,唤醒其他线程
```c
void
lock_release (struct lock *lock) 
{
  ASSERT (lock != NULL);
  ASSERT (lock_held_by_current_thread (lock));

    if (!thread_mlfqs){
        //解除锁
        thread_remove_lock (lock);  
    }
    
  lock->holder = NULL;
  //释放锁,V操作唤醒其他线程
  sema_up (&lock->semaphore);
}
```
修改了cond_signal函数,使用排序算法,将当前加锁的队列进行一个排序,以此来实现优先级队列,同时执行线程锁等待队列的V操作,释放锁
```c
void
cond_signal (struct condition *cond, struct lock *lock UNUSED) 
{
  ASSERT (cond != NULL);
  ASSERT (lock != NULL);
  ASSERT (!intr_context ());
  ASSERT (lock_held_by_current_thread (lock));

  if (!list_empty (&cond->waiters)) {
      list_sort (&cond->waiters, cond_sema_cmp_priority, NULL);
      sema_up (&list_entry (list_pop_front (&cond->waiters),
                          struct semaphore_elem, elem)->semaphore);
  }
    
}
```
## 任务3：Advanced Scheduler
### 任务内容
Advanced Scheduler要求实现一个替代优先级捐赠的另一种方案.该方案将线程划分成了不同的类别,比如有些线程需要占用IO操作,有些需要大量CPU计算,当然也有其他.但是本质上的区别是对CPU占用的时长不同,因此,依靠这个特性,我们可以改进算法,实现对CPU的更高效利用.
### 源代码分析
源代码给出了几个比较简单的接口

1. thread_get_nice函数,需要重新实现
```c
int
thread_get_nice (void) 
{
  /* Not yet implemented. */
  return 0;
}
```

2. thread_set_nice 函数,需要重新实现
```c
thread_set_nice (int nice UNUSED) 
{
  /* Not yet implemented. */
}
```

3. thread_get_recent_cpu函数,文档还提示我们要看这个函数并且需要实现它
```c
int
thread_get_recent_cpu (void) 
{
  /* Not yet implemented. */
  return 0;
}
```

4. thread_get_load_avg 函数,同3,过于简单了,同样需要我们实现它
```c
int
thread_get_load_avg (void) 
{
  /* Not yet implemented. */
  return 0;
}
```
### 问题解决思路
#### 核心算法及数学公式
既然不考虑优先级,那么需要用其他的变量来决定线程的调用,pintos官方文档给出了一种该变量的计算思路,如下

![](https://gitee.com/zkzkzk40/drawing-bed/raw/master/image/pintos/LnhpRsQ7gVX6uwD.png)

![](https://gitee.com/zkzkzk40/drawing-bed/raw/master/image/pintos/Qx23jqMS8KlJXeB.png)

![](https://gitee.com/zkzkzk40/drawing-bed/raw/master/image/pintos/UTRL7kei8FXmW9K.png)

![img](https://gitee.com/zkzkzk40/drawing-bed/raw/master/image/pintos/image-1641833301714.png)
最核心的数学公式是
$$priority = PRI\_MAX - (recent\_cpu / 4) - (nice * 2)$$
解释下其中变量意义:

1. recent_cpu是线程最近平均使用cpu的时间,根据文档的解释,这个变量可以随着后续CPU占用时间的更新而更新,其公式如下:

$$recent\_cpu = (2*load\_avg)/(2*load\_avg + 1) * recent\_cpu + nice$$

2. load_avg,系统平均装载时间,用于计算线程平均运行时间同样会随着后续运行时间变化而变化,其公式如下:

$$load\_avg = (59/60)*load\_avg + (1/60)*ready\_threads$$

3. nice值是一个系数,初始值为0,在-20到20之间波动,当它为正数时将会降低线程的权值并减少CPU时间并将减少的时间转交给其他线程.值得注意的是,文档还指出一开始的线程nice值为0,而其他线程的nice值将继承自其父线程的nice值
#### 浮点数计算的实现
pintos官方文档给出了浮点数计算的方法
![img](https://gitee.com/zkzkzk40/drawing-bed/raw/master/image/pintos/image-1641833333094.png)
因为pintos操作系统内核不支持浮点运算,所以文档给出了用整数代替浮点数的计算方法
因此我们只需要按照文档所给的思路实现浮点数计算方法即可

#### 系统最终实现
文档提示我们需要重新实现的函数有thread_get_nice,thread_set_nice ,thread_get_load_avg 和thread_get_recent_cpu函数,简单解释下这几个函数的作用:

   1. thread_get_nice:返回当前线程的nice值
   1. thread_set_nice:将当前线程的nice值设置为new_nice并根据新值重新计算线程的优先级.如果正在运行的线程不再具有最高优先级，则让步
   1. thread_get_load_avg :返回当前系统负载平均值的 100 倍，四舍五入到最接近的整数
   1. thread_get_recent_cpu :返回当前线程的recent_cpu值的100 倍，四舍五入到最接近的整数

除此之外还必须新加其他的函数,这些函数用于计算priority,load_avg和recent_cpu的数值,因为文档已经给出了公式,所以我们只需要按照文档给出的公式实现即可
最后,我们需要使用一个函数来调用上面计算的priority去更新线程的优先级,这一步实际上是在timer_interrupt函数中调用即可,因为前面分析过了timer_interrupt函数是在CPU时间片用完后就立刻执行的操作,所以我们需要修改这个函数,让它每隔一定时间间隔就执行priority的更新
### 实现过程
#### 新增数据结构
为thread结构体添加新的变量nice和recent_cpu
```c
    int nice;                           /* Niceness. */
    fixed_t recent_cpu;                 /* Recent CPU. */
```
#### 浮点数算法实现
新增src\threads\fixed_point.h,用于实现浮点数计算和提供fixed_t结构体
```c
#ifndef __THREAD_FIXED_POINT_H
#define __THREAD_FIXED_POINT_H

/* Basic definitions of fixed point. */
typedef int fixed_t;
/* 16 LSB used for fractional part. */
#define FP_SHIFT_AMOUNT 16
/* Convert a value to fixed-point value. */
#define FP_CONST(A) ((fixed_t)(A << FP_SHIFT_AMOUNT))
/* Add two fixed-point value. */
#define FP_ADD(A,B) (A + B)
/* Add a fixed-point value A and an int value B. */
#define FP_ADD_MIX(A,B) (A + (B << FP_SHIFT_AMOUNT))
/* Substract two fixed-point value. */
#define FP_SUB(A,B) (A - B)
/* Substract an int value B from a fixed-point value A */
#define FP_SUB_MIX(A,B) (A - (B << FP_SHIFT_AMOUNT))
/* Multiply a fixed-point value A by an int value B. */
#define FP_MULT_MIX(A,B) (A * B)
/* Divide a fixed-point value A by an int value B. */
#define FP_DIV_MIX(A,B) (A / B)
/* Multiply two fixed-point value. */
#define FP_MULT(A,B) ((fixed_t)(((int64_t) A) * B >> FP_SHIFT_AMOUNT))
/* Divide two fixed-point value. */
#define FP_DIV(A,B) ((fixed_t)((((int64_t) A) << FP_SHIFT_AMOUNT) / B))
/* Get integer part of a fixed-point value. */
#define FP_INT_PART(A) (A >> FP_SHIFT_AMOUNT)
/* Get rounded integer of a fixed-point value. */
#define FP_ROUND(A) (A >= 0 ? ((A + (1 << (FP_SHIFT_AMOUNT - 1))) >> FP_SHIFT_AMOUNT) \
        : ((A - (1 << (FP_SHIFT_AMOUNT - 1))) >> FP_SHIFT_AMOUNT))

#endif /* thread/fixed_point.h */
```
#### 修改文档要求我们实现的代码
修改thread_get_nice
```c
int
thread_get_nice (void)
{
  return thread_current ()->nice;
}
```
修改thread_set_nice
```c
void
thread_set_nice (int nice)
{
  thread_current ()->nice = nice;
  thread_mlfqs_update_priority (thread_current ());
  thread_yield ();
}
```
修改thread_get_load_avg 
```c
int
thread_get_load_avg (void)
{
  return FP_ROUND (FP_MULT_MIX (load_avg, 100));
}

```
修改thread_get_recent_cpu
```c
int
thread_get_recent_cpu (void)
{
  return FP_ROUND (FP_MULT_MIX (thread_current ()->recent_cpu, 100));
}
```
#### 计算priority,load_avg和recent_cpu
计算priority
```c
void
thread_mlfqs_update_priority (struct thread *t)
{
  if (t == idle_thread)
    return;

  ASSERT (thread_mlfqs);
  ASSERT (t != idle_thread);
  //公式计算
  t->priority = FP_INT_PART (FP_SUB_MIX (FP_SUB (FP_CONST (PRI_MAX), FP_DIV_MIX (t->recent_cpu, 4)), 2 * t->nice));
  //确保priority在-20到20之间
  if(t->priority<PRI_MIN)
    t->priority=PRI_MIN;
  if(t->priority>PRI_MAX)
    t->priority=PRI_MAX;
}
```
	令recent_cpu加1的函数实现
```c
void
thread_mlfqs_increase_recent_cpu_by_one (void)
{
  ASSERT (thread_mlfqs);
  ASSERT (intr_context ());

  struct thread *current_thread = thread_current ();
  if (current_thread == idle_thread)
    return;
  current_thread->recent_cpu = FP_ADD_MIX (current_thread->recent_cpu, 1);
}
```
	更新load_avg和recent_cpu
```c
/* Every per second to refresh load_avg and recent_cpu of all threads. */
void
thread_mlfqs_update_load_avg_and_recent_cpu (void)
{
  ASSERT (thread_mlfqs);
  ASSERT (intr_context ());

  size_t ready_threads = list_size (&ready_list);
  if (thread_current () != idle_thread)
    ready_threads++;
  load_avg = FP_ADD (FP_DIV_MIX (FP_MULT_MIX (load_avg, 59), 60), FP_DIV_MIX (FP_CONST (ready_threads), 60));

  struct thread *t;
  struct list_elem *e = list_begin (&all_list);
  for (; e != list_end (&all_list); e = list_next (e))
  {
    t = list_entry(e, struct thread, allelem);
    if (t != idle_thread)
    {
      t->recent_cpu = FP_ADD_MIX (FP_MULT (FP_DIV (FP_MULT_MIX (load_avg, 2), FP_ADD_MIX (FP_MULT_MIX (load_avg, 2), 1)), t->recent_cpu), t->nice);
      thread_mlfqs_update_priority (t);
    }
  }
}
```
#### 更新线程优先级
修改timer_interrupt函数如下:
```c
static void
timer_interrupt (struct intr_frame *args UNUSED)
{
    ticks++;
    enum intr_level old_level = intr_disable ();
    thread_foreach (blocked_thread_check, NULL);
    if (thread_mlfqs)
  {
    thread_mlfqs_increase_recent_cpu_by_one ();
    if (ticks % TIMER_FREQ == 0)
      thread_mlfqs_update_load_avg_and_recent_cpu ();
    else if (ticks % 4 == 0)
      thread_mlfqs_update_priority (thread_current ());
  }
    intr_set_level (old_level);
  thread_tick ();
}
```
前面分析了为什么要修改这个函数,这里解释下第10行的if的作用:每隔100个时间片,更新load_avg和recent_cpu,每隔4个时间片( 不包括100的倍数)更新priority.
pintos官方文档建议我们将时间片的间隔调整为100,所以就有了如下全局变量
```c
/* Number of timer interrupts per second. */
#define TIMER_FREQ 100
```

# userprog
## 实验目标
来看看官方文档的要求
![img](https://gitee.com/zkzkzk40/drawing-bed/raw/master/image/pintos/image-111.png)
简单来说,用户程序是在用户拥有整台机器的错觉下编写的。这意味着当您一次加载和运行多个进程时，您必须正确管理内存、调度和其他状态以维持这种错觉。
所以我们要实现一个用于简单交互的用户系统,既然是一个用于交互的用户系统,就必须考虑和实现一些必要的东西,比如参数的传递,一些基本的系统调用的接口,比如exec,wait等.当然我们必须搞懂一些基本的概念,这里必须要明白我们在干什么:

   1. 用户在和一个完整的操作系统交互时所进行的操作时,有哪些数据在流通,从哪里流通到哪里?
   1. 用户是否可以直接拿到最底层的操作权限,如果是的话拿到权限后出错了怎么办,不是的话怎么才能用其他方式拿到权限
   1. 用户在新建一个程序的时候,操作系统需要将这个程序初始化,该如何初始化?

总之需要考虑很多东西,在userprog中的三个任务具体如下:
## 前期准备
需要把代码回滚到一开始阶段,官方文档提示前面thread修改了线程捐赠的代码会影响userprog的实现,所以用户管理代码的实现是建立在原来一开始纯净代码的基础上的
## 任务一:Argument Passing
### 任务目标
任务一的目标是实现参数传递,在process.c文件中已经存在一个函数process_execute,这个函数的作用是创建新的用户级进程,但是它并没有被完全实现,
### 源代码分析
#### 各个文件作用
不谋全局者,不足谋一域.首先来看看官方文档给我们讲解的userprog文件夹下各个文件的作用
![img](https://gitee.com/zkzkzk40/drawing-bed/raw/master/image/pintos/image-1641989619800.png)![img](https://gitee.com/zkzkzk40/drawing-bed/raw/master/image/pintos/image-1641989677470.png)

大意如下:

```c
process.c
process.h
		加载 ELF 二进制文件并启动进程。

pagedir.c
pagedir.h
        一个简单的 80 x 86 硬件页表管理器。
        尽管您可能不想为此项目修改此代码，
        但您可能想调用它的一些函数

syscall.c
syscall.h
        每当用户进程想要访问某些内核功能时，它都会调用系统调用。
        这是一个骨架系统调用处理程序。
        目前，它只是打印一条消息并终止用户进程。
        在本项目的第 2 部分中，
        您将添加代码来完成系统调用所需的所有其他操作。

exception.c
exception.h
		当用户进程执行特权或禁止操作时，它会作为“异常”或“故障”进入内核。
        这些文件处理异常。目前，所有异常都只是打印一条消息并终止该过程。
        项目 2 的一些（但不是全部）解决方案需要page_fault()在此文件中进行修改。

gdt.c
gdt.h
		80 x 86 是一种分段架构。
        全局描述符表 (GDT) 是描述正在使用的段的表。
        这些文件设置了 GDT。您不需要为任何项目修改这些文件
tss.c
tss.h
		任务状态段 (TSS) 用于 80 x 86 架构任务切换。
        Pintos 仅在用户进程进入中断处理程序时使用 TSS 来切换堆栈，Linux 也是如此。
        您不需要为任何项目修改这些文件
```
#### pintos内存分配方式
代码中有一些palloc_get_page函数等等,需要根据官方文档来解释
![img](https://gitee.com/zkzkzk40/drawing-bed/raw/master/image/pintos/image-1641989706747.png)![img](https://gitee.com/zkzkzk40/drawing-bed/raw/master/image/pintos/image-1641989723054.png)
在pintos中,内存调用是采用了80 x 86 调用约定,翻译如下

1. 调用者将函数的每个参数一个一个地压入堆栈，通常使用PUSH汇编语言指令。参数按从右到左的顺序推送。堆栈向下增长：每次推送都会减少堆栈指针，然后存储到它现在指向的位置，就像 C 表达式 *--sp =值.
1. 调用者将其下一条指令的地址（返回地址）压入堆栈并跳转到被调用者的第一条指令。一条 80 x 86 指令，CALL,两者兼而有之。
1. 被调用者执行。当它获得控制权时，堆栈指针指向返回地址，第一个参数就在它上面，第二个参数就在第一个参数的上面，依此类推。
1. 如果被调用者有返回值，则将其存储到寄存器中EAX。
1. 被调用者通过使用 80 x 86RET 指令从堆栈中弹出返回地址并跳转到它指定的位置来返回。
1. 调用者将参数从堆栈中弹出。

当然上面还给出了一个例子,f(1,2,3)的参数压栈如上图所示,可以看到地址是从下往上增大的,而数据也是从下往上的,栈顶指针在下方也就是0xbffffe70这个位置
文档还额外给了一个例子来解析参数在内存里的情况,以命令/bin/ls -l foo bar为例
![img](https://gitee.com/zkzkzk40/drawing-bed/raw/master/image/pintos/image-1641989755848.png)![img](https://gitee.com/zkzkzk40/drawing-bed/raw/master/image/pintos/image-1641989762955.png)
	所以说整个压栈的过程是很清晰的,在上图中地址自下而上增大,首先压入返回地址,然后根据参数顺序一个一个压入栈中.所以我们在调用这些参数指针的时候,为了正确的按照顺序读取参数,指针应该是从下往上读取
另外文档还指出内核和用户所占用的地址分配情况:
![img](https://gitee.com/zkzkzk40/drawing-bed/raw/master/image/pintos/image-1641989773694.png)
0-0x08048000(如果没算错的话,0x08048000=134512640,大约为128MB)这一段和0x08048000到PHYS_BASE合起来是用户占用的空间,PHYS_BASE到其他的则是内核占用的空间.我们知道操作系统在运行时是分为用户态和内核态,用户态是不可以访问内核态的内存的.而上面这个图中,用户栈从高地址到低地址,数据栈从低地址到高地址,与前面的例子相符合

#### 需要实现的函数
来看看我们要实现的函数process_execute
```c
tid_t
process_execute (const char *file_name) 
{
  char *fn_copy;
  tid_t tid;

  /* Make a copy of FILE_NAME.
     Otherwise there's a race between the caller and load(). */
  fn_copy = palloc_get_page (0);
  if (fn_copy == NULL)
    return TID_ERROR;
  strlcpy (fn_copy, file_name, PGSIZE);

  /* Create a new thread to execute FILE_NAME. */
  tid = thread_create (file_name, PRI_DEFAULT, start_process, fn_copy);
  if (tid == TID_ERROR)
    palloc_free_page (fn_copy); 
  return tid;
}

```
首先这个函数的参数是一个指针,指向字符串,在调用这个函数的过程中,会传入一个指令,如 "rm -rf" ,然后是我们熟悉的创建进程,检测进程是否创建成功,然后执行start_process函数.很显然这部分的代码并没有实现参数分割的功能,直接把参数抛入进程中,这就是问题所在
再来看看start_process函数
```c
static void
start_process (void *file_name_)
{
  char *file_name = file_name_;
  struct intr_frame if_;
  bool success;

  /* Initialize interrupt frame and load executable. */
  memset (&if_, 0, sizeof if_);
  if_.gs = if_.fs = if_.es = if_.ds = if_.ss = SEL_UDSEG;
  if_.cs = SEL_UCSEG;
  if_.eflags = FLAG_IF | FLAG_MBS;
  success = load (file_name, &if_.eip, &if_.esp);

  /* If load failed, quit. */
  palloc_free_page (file_name);
  if (!success) 
    thread_exit ();

  /* Start the user process by simulating a return from an
     interrupt, implemented by intr_exit (in
     threads/intr-stubs.S).  Because intr_exit takes all of its
     arguments on the stack in the form of a `struct intr_frame',
     we just point the stack pointer (%esp) to our stack frame
     and jump to it. */
  asm volatile ("movl %0, %%esp; jmp intr_exit" : : "g" (&if_) : "memory");
  NOT_REACHED ();
}
```
这个函数的作用是装载一个用户程序(13行),然后把调用了load函数,最后检查一下是否load函数是否成功,并调用了一个汇编代码
来看看load函数做了什么
```c
/* Loads an ELF executable from FILE_NAME into the current thread.
   Stores the executable's entry point into *EIP
   and its initial stack pointer into *ESP.
   Returns true if successful, false otherwise. */
bool
load (const char *file_name, void (**eip) (void), void **esp) 
{
  struct thread *t = thread_current ();
  struct Elf32_Ehdr ehdr;
  struct file *file = NULL;
  off_t file_ofs;
  bool success = false;
  int i;

  /* Allocate and activate page directory. */
  t->pagedir = pagedir_create ();
  if (t->pagedir == NULL) 
    goto done;
  process_activate ();

  /* Open executable file. */
  file = filesys_open (file_name);
  if (file == NULL) 
    {
      printf ("load: %s: open failed\n", file_name);
      goto done; 
    }

  /* Read and verify executable header. */
  if (file_read (file, &ehdr, sizeof ehdr) != sizeof ehdr
      || memcmp (ehdr.e_ident, "\177ELF\1\1\1", 7)
      || ehdr.e_type != 2
      || ehdr.e_machine != 3
      || ehdr.e_version != 1
      || ehdr.e_phentsize != sizeof (struct Elf32_Phdr)
      || ehdr.e_phnum > 1024) 
    {
      printf ("load: %s: error loading executable\n", file_name);
      goto done; 
    }

  /* Read program headers. */
  file_ofs = ehdr.e_phoff;
  for (i = 0; i < ehdr.e_phnum; i++) 
    {
      struct Elf32_Phdr phdr;

      if (file_ofs < 0 || file_ofs > file_length (file))
        goto done;
      file_seek (file, file_ofs);

      if (file_read (file, &phdr, sizeof phdr) != sizeof phdr)
        goto done;
      file_ofs += sizeof phdr;
      switch (phdr.p_type) 
        {
        case PT_NULL:
        case PT_NOTE:
        case PT_PHDR:
        case PT_STACK:
        default:
          /* Ignore this segment. */
          break;
        case PT_DYNAMIC:
        case PT_INTERP:
        case PT_SHLIB:
          goto done;
        case PT_LOAD:
          if (validate_segment (&phdr, file)) 
            {
              bool writable = (phdr.p_flags & PF_W) != 0;
              uint32_t file_page = phdr.p_offset & ~PGMASK;
              uint32_t mem_page = phdr.p_vaddr & ~PGMASK;
              uint32_t page_offset = phdr.p_vaddr & PGMASK;
              uint32_t read_bytes, zero_bytes;
              if (phdr.p_filesz > 0)
                {
                  /* Normal segment.
                     Read initial part from disk and zero the rest. */
                  read_bytes = page_offset + phdr.p_filesz;
                  zero_bytes = (ROUND_UP (page_offset + phdr.p_memsz, PGSIZE)
                                - read_bytes);
                }
              else 
                {
                  /* Entirely zero.
                     Don't read anything from disk. */
                  read_bytes = 0;
                  zero_bytes = ROUND_UP (page_offset + phdr.p_memsz, PGSIZE);
                }
              if (!load_segment (file, file_page, (void *) mem_page,
                                 read_bytes, zero_bytes, writable))
                goto done;
            }
          else
            goto done;
          break;
        }
    }

  /* Set up stack. */
  if (!setup_stack (esp))
    goto done;

  /* Start address. */
  *eip = (void (*) (void)) ehdr.e_entry;

  success = true;

 done:
  /* We arrive here whether the load is successful or not. */
  file_close (file);
  return success;
}
```
函数很长,首先是分配内存和页目录,然后调用setup_stack函数初始化运行栈
	setup_stack函数代码如下,主要就是调用pintos官网给我们写好的palloc_get_page等函数初始化栈就行
```c
static bool
setup_stack (void **esp) 
{
  uint8_t *kpage;
  bool success = false;

  kpage = palloc_get_page (PAL_USER | PAL_ZERO);
  if (kpage != NULL) 
    {
      success = install_page (((uint8_t *) PHYS_BASE) - PGSIZE, kpage, true);
      if (success)
        *esp = PHYS_BASE;
      else
        palloc_free_page (kpage);
    }
  return success;
}
```
### 实验思路
#### 分离参数
前面源代码简单分析过了问题的原因所在,所以第一步我们需要分离参数,步骤如下:
##### 定义两个变量并且分配内存
###### 为什么要两个变量?
因为我们需要把输入的命令串给分隔
###### 怎么分配内存?
其实是根据源代码照猫画虎的写
```c
char *fn_copy; 
fn_copy = palloc_get_page (0);
if (fn_copy == NULL)
    return TID_ERROR;
```
源代码的分配方式如上,所以这里也就和它一样就行
###### 为什么需要分配内存?
我们的文件是存放在硬盘中的,要将这些内容转到内存中,就必须要给读取硬盘并分配到内存相应的位置中去
##### 给这两个变量赋值
pintos官方已经为我们写好了用于拆解字符串的类src/lib/string.h.代码如下
```c

/* Standard. */
void *memcpy (void *, const void *, size_t);
void *memmove (void *, const void *, size_t);
char *strncat (char *, const char *, size_t);
int memcmp (const void *, const void *, size_t);
int strcmp (const char *, const char *);
void *memchr (const void *, int, size_t);
char *strchr (const char *, int);
size_t strcspn (const char *, const char *);
char *strpbrk (const char *, const char *);
char *strrchr (const char *, int);
size_t strspn (const char *, const char *);
char *strstr (const char *, const char *);
void *memset (void *, int, size_t);
size_t strlen (const char *);

/* Extensions. */
size_t strlcpy (char *, const char *, size_t);
size_t strlcat (char *, const char *, size_t);
char *strtok_r (char *, const char *, char **);
size_t strnlen (const char *, size_t);

/* Try to be helpful. */
#define strcpy dont_use_strcpy_use_strlcpy
#define strncpy dont_use_strncpy_use_strlcpy
#define strcat dont_use_strcat_use_strlcat
#define strncat dont_use_strncat_use_strlcat
#define strtok dont_use_strtok_use_strtok_r
```
其中我们可以用到的复制字符串的函数是strlcpy,第一个参数是要被复制的字符串指针,第二个是字符串复制来源,第三个是长度
```c
size_t
strlcpy (char *dst, const char *src, size_t size) 
{
  size_t src_len;

  ASSERT (dst != NULL);
  ASSERT (src != NULL);

  src_len = strlen (src);
  if (size > 0) 
    {
      size_t dst_len = size - 1;
      if (src_len < dst_len)
        dst_len = src_len;
      memcpy (dst, src, dst_len);
      dst[dst_len] = '\0';
    }
  return src_len;
}
```
memcpy的作用很明显了,字符挨个复制
```c
void *
memcpy (void *dst_, const void *src_, size_t size) 
{
  unsigned char *dst = dst_;
  const unsigned char *src = src_;

  ASSERT (dst != NULL || size == 0);
  ASSERT (src != NULL || size == 0);

  while (size-- > 0)
    *dst++ = *src++;

  return dst_;
}
```
##### 分隔字符串
依然是用到src/lib/string.h下的函数,为strtok_r函数,这个函数比较复杂,第一个参数是要分割的字符串，第二个参数是分割的依据，第三个参数是存储分割后剩余的右边部分字符串，返回值是分割左部的字符串
```c
char *
strtok_r (char *s, const char *delimiters, char **save_ptr) 
{
  char *token;
  
  ASSERT (delimiters != NULL);
  ASSERT (save_ptr != NULL);

  /* If S is nonnull, start from it.
     If S is null, start from saved position. */
  if (s == NULL)
    s = *save_ptr;
  ASSERT (s != NULL);

  /* Skip any DELIMITERS at our current position. */
  while (strchr (delimiters, *s) != NULL) 
    {
      /* strchr() will always return nonnull if we're searching
         for a null byte, because every string contains a null
         byte (at the end). */
      if (*s == '\0')
        {
          *save_ptr = s;
          return NULL;
        }

      s++;
    }

  /* Skip any non-DELIMITERS up to the end of the string. */
  token = s;
  while (strchr (delimiters, *s) == NULL)
    s++;
  if (*s != '\0') 
    {
      *s = '\0';
      *save_ptr = s + 1;
    }
  else 
    *save_ptr = s;
  return token;
}
```
借用这个函数我们就可以把前面创建的两个变量最终分配完成
#### 压栈
这一步就是对应于前面分析的内存分配前面分析过的start_process函数就是在创建进程的时候调用的函数,我们的压栈操作就需要修改这个函数来进行
### 代码实现
新增push_argument函数用于压栈操作
```c
/* Our implementation for Task 1:
  Push argument into stack, this method is used in Task 1 Argument Pushing */
void
push_argument (void **esp, int argc, int argv[]){
  *esp = (int)*esp & 0xfffffffc;
  *esp -= 4;//四位对齐
  *(int *) *esp = 0;
    /*循环压入参数栈*/
  for (int i = argc - 1; i >= 0; i--)
  {
    *esp -= 4;
    *(int *) *esp = argv[i];
  }
  *esp -= 4;
  *(int *) *esp = (int) *esp + 4;//压入第一个参数的地址
  *esp -= 4;
  *(int *) *esp = argc;
  *esp -= 4;
  *(int *) *esp = 0;
}
```
这里涉及到一级指针和二级指针,因为内存中的数据存放方式是通过指针的
修改start_process函数
```c
static void
start_process (void *file_name_)
{
  char *file_name = file_name_;
  struct intr_frame if_;
  bool success;

char *fn_copy=malloc(strlen(file_name)+1);
strlcpy(fn_copy,file_name,strlen(file_name)+1);

  /* Initialize interrupt frame */
  memset (&if_, 0, sizeof if_);
  if_.gs = if_.fs = if_.es = if_.ds = if_.ss = SEL_UDSEG;
  if_.cs = SEL_UCSEG;
  if_.eflags = FLAG_IF | FLAG_MBS;

  /*load executable. */
  //此处发生改变，需要传入文件名
  char *token, *save_ptr;
  file_name = strtok_r (file_name, " ", &save_ptr);
  success = load (file_name, &if_.eip, &if_.esp);
  
  if (success)
    {

    /* Our implementation for Task 1:
      Calculate the number of parameters and the specification of parameters */
    int argc = 0;
    /* The number of parameters can't be more than 50 in the test case */
    int argv[50];
    for (token = strtok_r (fn_copy, " ", &save_ptr); token != NULL; token = strtok_r (NULL, " ", &save_ptr)){
      if_.esp -= (strlen(token)+1);//栈指针向下移动，留出token+'\0'的大小
      memcpy (if_.esp, token, strlen(token)+1);//token+'\0'复制进去
      argv[argc++] = (int) if_.esp;//存储 参数的地址
    }
    push_argument (&if_.esp, argc, argv);//将参数的地址压入栈
     /* Record the exec_status of the parent thread's success and sema up parent's semaphore */
    thread_current ()->parent->success = true;
    sema_up (&thread_current ()->parent->sema);
    }
  /* Free file_name whether successed or failed. */
  palloc_free_page (file_name);
  free(fn_copy);
  if (!success) 
  {
    thread_current ()->parent->success = false;
    sema_up (&thread_current ()->parent->sema);
    thread_exit ();
  }
    

  /* Start the user process by simulating a return from an
     interrupt, implemented by intr_exit (in
     threads/intr-stubs.S).  Because intr_exit takes all of its
     arguments on the stack in the form of a `struct intr_frame',
     we just point the stack pointer (%esp) to our stack frame
     and jump to it. */
  asm volatile ("movl %0, %%esp; jmp intr_exit" : : "g" (&if_) : "memory");
  NOT_REACHED ();
}

```
修改process_execute函数
```c
tid_t
process_execute (const char *file_name) 
{
  char *fn_copy0, *fn_copy1;
  tid_t tid;


    /* Make a copy of FILE_NAME.
       Otherwise strtok_r will modify the const char *file_name. */
    fn_copy0 = palloc_get_page(0);//palloc_get_page(0)动态分配了一个内存页
    if (fn_copy0 == NULL)//分配失败
        return TID_ERROR;

  /* Make a copy of FILE_NAME.
     Otherwise there's a race between the caller and load(). */
  fn_copy1 = palloc_get_page (0);
  if (fn_copy1 == NULL)
  {
    palloc_free_page(fn_copy0);
    return TID_ERROR;
  }
  //把file_name 复制2份，PGSIZE为页大小
  strlcpy (fn_copy0, file_name, PGSIZE);
  strlcpy (fn_copy1, file_name, PGSIZE);


  /* Create a new thread to execute FILE_NAME. */
  char *save_ptr;
  char *cmd = strtok_r(fn_copy0, " ", &save_ptr);
  
  tid = thread_create(cmd, PRI_DEFAULT, start_process, fn_copy1);
  palloc_free_page(fn_copy0);
  if (tid == TID_ERROR)
  {
    palloc_free_page (fn_copy1); 
    return tid;
  }
    //后续exec系统调用要求,懒得删了...
    /* Sema down the parent process, waiting for child */
  sema_down(&thread_current()->sema);
  if (!thread_current()->success) return TID_ERROR;//can't create new process thread,return error

  return tid;
}
```
## 任务二:Process Control Syscalls
### 任务目标
我们需要实现如下四个内核态的系统调用接口

   1. exit:终止当前用户程序，将状态返回给内核
   1. halt:终止 Pintos​
   1. exec:运行名称在cmd_line 中给出的可执行文件，传递任何给定的参数，并返回新进程的程序 id (pid)
   1. wait:等待子进程pid并检索子进程的退出状态

在实现过程中比如保证系统调用的过程中是安全的,可能出现的问题是:

   1. 传入空指针
   1. 传入无效指针
   1. 内存地址不属于用户内存范围内,也就是前面分析过的0到PHYS_BASE,可能属于内核地址空间

当出现上述情况时,我们必须要关闭对应的用户进程
### 源代码分析
来看下src/lib/syscall-nr.h文件
```c
/* System call numbers. */
enum 
  {
    /* Projects 2 and later. */
    SYS_HALT,                   /* Halt the operating system. */
    SYS_EXIT,                   /* Terminate this process. */
    SYS_EXEC,                   /* Start another process. */
    SYS_WAIT,                   /* Wait for a child process to die. */
    SYS_CREATE,                 /* Create a file. */
    SYS_REMOVE,                 /* Delete a file. */
    SYS_OPEN,                   /* Open a file. */
    SYS_FILESIZE,               /* Obtain a file's size. */
    SYS_READ,                   /* Read from a file. */
    SYS_WRITE,                  /* Write to a file. */
    SYS_SEEK,                   /* Change position in a file. */
    SYS_TELL,                   /* Report current position in a file. */
    SYS_CLOSE,                  /* Close a file. */

    /* Project 3 and optionally project 4. */
    SYS_MMAP,                   /* Map a file into memory. */
    SYS_MUNMAP,                 /* Remove a memory mapping. */

    /* Project 4 only. */
    SYS_CHDIR,                  /* Change the current directory. */
    SYS_MKDIR,                  /* Create a directory. */
    SYS_READDIR,                /* Reads a directory entry. */
    SYS_ISDIR,                  /* Tests if a fd represents a directory. */
    SYS_INUMBER                 /* Returns the inode number for a fd. */
  };
```
这个枚举类给出了所有的系统内核的接口,本任务中要实现的就是前四个
回过来看src/userprog/syscall.h
```c
#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H

void syscall_init (void);

#endif /* userprog/syscall.h */
```
syscall.c
```c
static void syscall_handler (struct intr_frame *);

void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

static void
syscall_handler (struct intr_frame *f UNUSED) 
{
  printf ("system call!\n");
  thread_exit ();
}
```
简简单单朴实无华,代码量低到我一时间没反应过来
syscall_init函数用于初始化,syscall_handler用于调用对应的指令
### 实验思路
#### 合法性判断
合法性判断是必须要在最前面执行的,pintos官方文档给了我们很多的提示
![img](https://gitee.com/zkzkzk40/drawing-bed/raw/master/image/pintos/image-1641989815320.png)
两种思路:

1. 验证用户提供的指针的有效性，然后取消引用它
1. 检查用户指针是否指向下方PHYS_BASE，然后取消引用它

官方文档给出了第二种思路的实现代码,我们实现即可
#### 四种指令的实现
##### halt
源代码给了shutdown_power_off函数供我们直接调用
```c
void
shutdown_power_off (void)
{
  const char s[] = "Shutdown";
  const char *p;

#ifdef FILESYS
  filesys_done ();
#endif

  print_stats ();

  printf ("Powering off...\n");
  serial_flush ();

  /* ACPI power-off */
  outw (0xB004, 0x2000);

  /* This is a special power-off sequence supported by Bochs and
     QEMU, but not by physical hardware. */
  for (p = s; *p != '\0'; p++)
    outb (0x8900, *p);

  /* For newer versions of qemu, you must run with -device
   * isa-debug-exit, which exits on any write to an IO port (by
   * default 0x501).  Qemu's exit code is double the value plus one,
   * so there is no way to exit cleanly.  We use 0x31 which should
   * result in a qemu exit code of 0x63.  */
  outb (0x501, 0x31);

  /* This will power off a VMware VM if "gui.exitOnCLIHLT = TRUE"
     is set in its configuration file.  (The "pintos" script does
     that automatically.)  */
  asm volatile ("cli; hlt" : : : "memory");

  /* None of those worked. */
  printf ("still running...\n");
  for (;;);
}
```
用了一些汇编代码,看不太懂,但是这里只需调用即可
##### exit
终止当前用户程序，将状态返回给内核

![img](https://gitee.com/zkzkzk40/drawing-bed/raw/master/image/pintos/image-1641989835430.png)根据文档的描述,需要返回一个状态码,0表示成功,其余表示失败
首先第一步就是进行合法性判断
第二步,保存退出的状态码
第三步,既然是要终止当前用户程序,那么就涉及到了进程和线程的问题,所以这里也需要操作下线程,把线程关闭了

##### exec
查看文档，运行其名称在 cmd_line 中给出的可执行文件，并传递任何给定的参数，并返回新进程的程序ID（pid）。
这里就涉及到PV操作了,既然是新的进程产生,那么久必须要阻塞原来的进程直到新进程创建成功,换句话说,父进程进行P操作,子进程创建完毕后进行V操作
还记得process_execute吗,这个函数用于创建新的进程,也可以用于创建新线程,所以这里我们需要调用这个函数,同时需要改进这个函数,在这里进行PV操作
##### wait
等待子进程pid并检索子进程的退出状态
官方文档对退出状态的解释如下
![img](https://gitee.com/zkzkzk40/drawing-bed/raw/master/image/pintos/image-1641989852860.png)
如果以下任一条件为真，则必须失败并立即返回 -1

- pid不是指调用进程的直接子进程。 当且仅当调用进程收到pid作为成功调用的返回值时，pid才是调用进程的直接子进程。 请注意，子进程不是继承的：如果A产生子B 并且B产生子进程C，那么A不能等待 C，即使B已经死了。要在通话wait(C)的过程中 一个必须失败。类似地，如果它们的父进程在它们之前退出，则不会将孤立进程分配给新的父进程。
- 调用的进程wait已经调用wait了 pid。也就是说，一个进程最多可以等待任何给定的孩子一次

进程可以产生任意数量的子进程，以任意顺序等待它们，甚至可以在没有等待其部分或全部子进程的情况下退出。您的设计应考虑可能发生等待的所有方式。一个进程的所有资源，包括它的struct thread，无论它的父进程是否等待它，也不管子进程是在它的父进程之前还是之后退出，都必须被释放。
综上所述,要实现wait和exec必须修改原有的thread结构体,因为涉及到了子线程数据的保存,分析一下需要加入哪些数据:

   1. 退出状态
   1. 当前进程创建的子线程列表
   1. 当前线程的子线程指针
   1. PV操作用的semaphore信号量
   1. 父进程指针
   1. 判断子线程是否成功执行的返回值
   1. 子线程的id
   1. 子线程的运行状态
   1. 子线程的PV操作用的semaphore信号量
   1. 子线程的退出状态
   1. 子线程的子线程列表

把最后五个状态组合在一个结构体中
#### 谁来调用指令?
源代码一开始只提供了如下两个函数
```c
void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

static void
syscall_handler (struct intr_frame *f UNUSED) 
{
  printf ("system call!\n");
  thread_exit ();
}
```
显然光有这些代码是不足以完成的,从用户输入命令到命令执行,结束,大致流程如下:
syscall_init存储了系统调用的类型。当中断发生，参数（包含了系统调用的类型）入栈,执行syscall_handler
syscall_handler弹出栈顶元素，也就是系统调用的类型，并去syscall_init里寻找有无定义该系统调用，找到了的话就转而执行该系统调用。
### 代码实现
#### 地址检查
首先复现一下文档给出的地址安全性检查代码,判断该地址是否写入内核
```c
static int
get_user (const uint8_t *uaddr)
{
  int result;
  asm ("movl $1f, %0; movzbl %1, %0; 1:"
       : "=&a" (result) : "m" (*uaddr));
  return result;
}
```
然后新建一个函数check_ptr2,用于判断
```c
void * 
check_ptr2(const void *vaddr)
{ 
  /* Judge address */
  if (!is_user_vaddr(vaddr))//检查地址是否为用户地址
  {
    exit_special ();
  }
  /* Judge the page */
  void *ptr = pagedir_get_page (thread_current()->pagedir, vaddr);//检查page是否为用户地址
  if (!ptr)
  {
    exit_special ();
  }
  /* Judge the content of page */
  //检查用户地址指向内容是否合法
  uint8_t *check_byteptr = (uint8_t *) vaddr;
  uint8_t i=0;
  while ( i < 4) 
  {
    if (get_user(check_byteptr + i) == -1)
    {
      exit_special ();
    }
    i++;
  }

  return ptr;
}
```
#### 新建的数据结构
修改thread结构体,加入如下
```c
struct list childs;                 /* The list of childs 创建的所有子线程*/
struct child * thread_child;        /* Store the child of this thread 存储线程的子进程,新建线程时用来存自己*/
int st_exit;                        /* Exit status */
struct semaphore sema;              /* Control the child process's logic, finish parent waiting for child */
bool success;                       /* Judge whehter the child's thread execute successfully */
struct thread* parent;              /* Parent thread of the thread 当前进程的父进程*/

struct child
  {
    tid_t tid;                           /* tid of the thread */
    bool isrun;                          /* whether the child's thread is run successfully */
    struct list_elem child_elem;         /* list of children */
    struct semaphore sema;               /* semaphore to control waiting */
    int store_exit;                      /* the exit status of child thread */
  };
```
修改init_thread进行初始化
```c
  if (t==initial_thread) t->parent=NULL;
  /* Record the parent's thread */
  else t->parent = thread_current ();
  /* List initialization for lists */
  list_init (&t->childs);
  /* Semaphore initialization for lists */
  sema_init (&t->sema, 0);
  t->success = true;
  /* Initialize exit status to MAX */
  t->st_exit = UINT32_MAX;
```


#### halt
新加sys_halt函数
```c
void sys_halt (struct intr_frame* f)
{
  shutdown_power_off();
}
```
#### exit
新加sys_exit函数
```c
void 
sys_exit (struct intr_frame* f)
{
  uint32_t *user_ptr = f->esp;
  check_ptr2 (user_ptr + 1);//检验第一个参数
  *user_ptr++;//指针指向第一个参数
  /* record the exit status of the process */
  thread_current()->st_exit = *user_ptr;//保存exit_code
  thread_exit ();
}

```
#### exec
新加sys_exec函数
```c
/* Do sytem exec */
void 
sys_exec (struct intr_frame* f)
{
  uint32_t *user_ptr = f->esp;
  check_ptr2 (user_ptr + 1);//检查第一个参数的地址
  check_ptr2 (*(user_ptr + 1));//检查第一个参数的值，即const char *file指向的地址
  *user_ptr++;
  f->eax = process_execute((char*)* user_ptr);//使用process_execute完成pid的返回
}
```
#### wait
新加sys_wait函数
```c
void 
sys_wait (struct intr_frame* f)
{
  uint32_t *user_ptr = f->esp;
  check_ptr2 (user_ptr + 1);
  *user_ptr++;
  f->eax = process_wait(*user_ptr);
}
```
修改process_wait函数
```c
int process_wait(tid_t child_tid UNUSED)
{
  /* Find the child's ID that the current thread waits for and sema down the child's semaphore */
  struct list *l = &thread_current()->childs;
  struct list_elem *child_elem_ptr;
  child_elem_ptr = list_begin(l);
  struct child *child_ptr = NULL;
  while (child_elem_ptr != list_end(l)) //遍历当前线程的所有子线程
  {
    /* list_entry:Converts pointer to list element LIST_ELEM into a pointer to
   the structure that LIST_ELEM is embedded inside.  Supply the
   name of the outer structure STRUCT and the member name MEMBER
   of the list element. */
    child_ptr = list_entry(child_elem_ptr, struct child, child_elem); //把child_elem的指针变成child的指针
    if (child_ptr->tid == child_tid)                                  //找到child_tid
    {
      if (!child_ptr->isrun) //检查子线程之前是否已经等待过
      {
        child_ptr->isrun = true;
        sema_down(&child_ptr->sema); //线程阻塞，等待子进程结束
        break;
      }
        return -1;//等待过了
    }
    child_elem_ptr = list_next(child_elem_ptr);
  }
  if (child_elem_ptr == list_end(l))
  { //找不到child_tid
    return -1;
  }
  //执行到这里说明子进程正常退出
  list_remove(child_elem_ptr);  //从子进程列表中删除该子进程，因为它已经没有在运行了，也就是说父进程重新抢占回了资源
  return child_ptr->store_exit; //返回子线程exit值
}
```
#### 如何调用


修改syscall_init函数
```c
void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
    /* Our implementation for Task2: initialize halt,exit,exec */
  syscalls[SYS_EXEC] = &sys_exec;
  syscalls[SYS_HALT] = &sys_halt;
  syscalls[SYS_EXIT] = &sys_exit;
 
  // /* Our implementation for Task3: initialize create, remove, open, filesize, read, write, seek, tell, and close */
  syscalls[SYS_WAIT] = &sys_wait;
  syscalls[SYS_CREATE] = &sys_create;
  syscalls[SYS_REMOVE] = &sys_remove;
  syscalls[SYS_OPEN] = &sys_open;
  syscalls[SYS_WRITE] = &sys_write;
  syscalls[SYS_SEEK] = &sys_seek;
  syscalls[SYS_TELL] = &sys_tell;
  syscalls[SYS_CLOSE] =&sys_close;
  syscalls[SYS_READ] = &sys_read;
  syscalls[SYS_FILESIZE] = &sys_filesize;
}
```
修改syscall_handler函数
```c
static void
syscall_handler (struct intr_frame *f UNUSED) 
{
  /* For Task2 practice, just add 1 to its first argument, and print its result */
  int * p = f->esp;
  check_ptr2 (p + 1);//检验第一个参数
  int type = * (int *)f->esp;//检验系统调用号sys_code是否合法
  if(type <= 0 || type >= max_syscall){
    exit_special ();
  }
  syscalls[type](f);//无误则执行对应系统调用函数
}
```
## 任务三:File Operation Syscalls
### 任务目标
实现9个内核态的文件系统调用接口

![img](https://gitee.com/zkzkzk40/drawing-bed/raw/master/image/pintos/image-1641989886003.png)![img](https://gitee.com/zkzkzk40/drawing-bed/raw/master/image/pintos/image-1641989890432.png)![img](https://gitee.com/zkzkzk40/drawing-bed/raw/master/image/pintos/image-1641989895239.png)

对应的解析如下

```c
    SYS_CREATE,                 /* 创建文件 */
    SYS_REMOVE,                 /* 删除文件 */
    SYS_OPEN,                   /* 打开文件 */
    SYS_FILESIZE,               /* 返回文件大小. */
    SYS_READ,                   /* 读取文件内容到buffer缓冲区中,返回实际读取的字节数. */
    SYS_WRITE,                  /* 从buffer缓冲区写入打开的文件,返回实际写入的字节数 */
    SYS_SEEK,                   /* 将打开文件fd 中要读取或写入的下一个字节更改为 position，以文件开头的字节表示. */
    SYS_TELL,                   /* 返回文件中要读取或写入的下一个字节的位置. */
    SYS_CLOSE,                  /* 关闭文件. */
```
#### fd是什么
官方文档中反复出现了一个单词 : fd,这个词的意义是一个索引值，指向内核为每一个进程所维护的该进程打开文件的记录表,一个非负整数,标准输入（standard input）的文件描述符是 0，标准输出（standard output）是 1，标准错误（standard error）是 2
### 源码分析
src/filesys/file.h给我们提供了一些API接口以供我们实现任务,代码如下
```c
struct inode;

/* Opening and closing files. */
struct file *file_open (struct inode *);
struct file *file_reopen (struct file *);
void file_close (struct file *);
struct inode *file_get_inode (struct file *);

/* Reading and writing. */
off_t file_read (struct file *, void *, off_t);
off_t file_read_at (struct file *, void *, off_t size, off_t start);
off_t file_write (struct file *, const void *, off_t);
off_t file_write_at (struct file *, const void *, off_t size, off_t start);

/* Preventing writes. */
void file_deny_write (struct file *);
void file_allow_write (struct file *);

/* File position. */
void file_seek (struct file *, off_t);
off_t file_tell (struct file *);
off_t file_length (struct file *);
```
### 实验思路
#### 读者写者问题
既然涉及到文件的操作,就必须要考虑到一个经典的PV问题--读者写者问题,当然实际上在pintos中并没有那么复杂,只需要考虑同时只有一个进程/线程进行读写操作即可.所以为了保证同一时刻只有一个用户对同一文件进行操作,需要给文件加文件锁
#### 怎么加文件锁
首先应该是在线程对一个文件进行操作的时候进行加锁的操作,而在线程死亡时进行解锁的操作.所以我们需要为线程加什么样的数据结构已经呼之欲出了:

   1. 线程的文件锁集合
   1. 最大的fd值
   1. 新的结构体用于存储文件信息
### 代码实现
thread结构体中加入如下
```c
struct thread_file
  {
    int fd;
    struct file* file;
    struct list_elem file_elem;
  };
struct list files; 
int max_file_fd;
```
thread.c中加入如下:
```c
static struct lock lock_f;
void 
acquire_lock_f ()
{
  lock_acquire(&lock_f);
}

void 
release_lock_f ()
{
  lock_release(&lock_f);
}

void
lock_acquire (struct lock *lock)
{
  ASSERT (lock != NULL);
  ASSERT (!intr_context ());
  ASSERT (!lock_held_by_current_thread (lock));

  sema_down (&lock->semaphore);
  lock->holder = thread_current ();
}
void
lock_release (struct lock *lock) 
{
  ASSERT (lock != NULL);
  ASSERT (lock_held_by_current_thread (lock));

  lock->holder = NULL;
  sema_up (&lock->semaphore);
}
```
#### write
syscall.c中加入sys_write函数
```c
/* Do system write, Do writing in stdout and write in files */
void 
sys_write (struct intr_frame* f)
{
  uint32_t *user_ptr = f->esp;
  check_ptr2 (user_ptr + 7);//for tests maybe?
  check_ptr2 (*(user_ptr + 6));
  *user_ptr++;
  int fd = *user_ptr;
  const char * buffer = (const char *)*(user_ptr+1);
  off_t size = *(user_ptr+2);
  if (fd == 1) {//writes to the console
    /* Use putbuf to do testing */
    putbuf(buffer,size);
    f->eax = size;//return number written
  }
  else
  {
    /* Write to Files */
    struct thread_file * thread_file_temp = find_file_id (*user_ptr);
    if (thread_file_temp)
    {
      acquire_lock_f ();//file operating needs lock
      f->eax = file_write (thread_file_temp->file, buffer, size);
      release_lock_f ();
    } 
    else
    {
      f->eax = 0;//can't write,return 0
    }
  }
}
```
简单解释下过程:首先需要获取中断的页,通过esp获取栈顶地址并检验地址合法性(后面的代码思路其实差不多,不赘述了)
当fd值为1是则是标准输出,需要输出到控制台上,如果不是则写入到文件中去
file_write三个参数分别是fd值,buffer来源,写入字数大小,将这三个参数传入file_write函数中调用
其中的find_file_id是在thread结构体中的文件集合中查找对应的文件id值,代码如下如下
```c
/* Find file by the file's ID */
struct thread_file * 
find_file_id (int file_id)
{
  struct list_elem *e;
  struct thread_file * thread_file_temp = NULL;
  struct list *files = &thread_current ()->files;
  for (e = list_begin (files); e != list_end (files); e = list_next (e)){
    thread_file_temp = list_entry (e, struct thread_file, file_elem);
    if (file_id == thread_file_temp->fd)
      return thread_file_temp;
  }
  return false;
}
```
#### create
直接调用filesys_create 函数即可,syscall.c中加入sys_create函数
```c
void 
sys_create(struct intr_frame* f)
{
  uint32_t *user_ptr = f->esp;
  check_ptr2 (user_ptr + 5);
  check_ptr2 (*(user_ptr + 4));
  *user_ptr++;
  acquire_lock_f ();
  f->eax = filesys_create ((const char *)*user_ptr, *(user_ptr+1));
  release_lock_f ();
}
```
#### remove
直接调用filesys_remove函数即可,syscall.c中加入sys_remove函数
```c
/* Do system remove, by calling the method filesys_remove */
void 
sys_remove(struct intr_frame* f)
{
  uint32_t *user_ptr = f->esp;
  check_ptr2 (user_ptr + 1);//arg address
  check_ptr2 (*(user_ptr + 1));//file address 
  *user_ptr++;
  acquire_lock_f ();
  f->eax = filesys_remove ((const char *)*user_ptr);
  release_lock_f ();
}
```
#### open
直接调用filesys_open 函数即可,syscall.c中加入sys_open函数,但是需要注意的是,每次打开文件都会返回一个fd值,这时候就需要吧把这个值记录下来,方便后续调用
```c
void 
sys_open (struct intr_frame* f)
{
  uint32_t *user_ptr = f->esp;
  check_ptr2 (user_ptr + 1);
  check_ptr2 (*(user_ptr + 1));
  *user_ptr++;
  acquire_lock_f ();
  struct file * file_opened = filesys_open((const char *)*user_ptr);
  release_lock_f ();
  struct thread * t = thread_current();
  if (file_opened)
  {
    struct thread_file *thread_file_temp = malloc(sizeof(struct thread_file));
    thread_file_temp->fd = t->max_file_fd++;
    thread_file_temp->file = file_opened;
    list_push_back (&t->files, &thread_file_temp->file_elem);//维护files列表
    f->eax = thread_file_temp->fd;
  } 
  else// the file could not be opened
  {
    f->eax = -1;
  }
}
```
#### filesize
直接调用file_length函数即可,syscall.c中加入sys_filesize函数
```c
void 
sys_filesize (struct intr_frame* f){
  uint32_t *user_ptr = f->esp;
  check_ptr2 (user_ptr + 1);
  *user_ptr++;//fd
  struct thread_file * thread_file_temp = find_file_id (*user_ptr);
  if (thread_file_temp)
  {
    acquire_lock_f ();
    f->eax = file_length (thread_file_temp->file);//return the size in bytes
    release_lock_f ();
  } 
  else
  {
    f->eax = -1;
  }
}
```
#### read
直接调用file_tell函数即可,syscall.c中加入sys_read函数
```c
void 
sys_read (struct intr_frame* f)
{
  uint32_t *user_ptr = f->esp;
  /* PASS the test bad read */
  *user_ptr++;
  /* We don't konw how to fix the bug, just check the pointer */
  int fd = *user_ptr;
  uint8_t * buffer = (uint8_t*)*(user_ptr+1);
  off_t size = *(user_ptr+2);
  if (!is_valid_pointer (buffer, 1) || !is_valid_pointer (buffer + size,1)){
    exit_special ();
  }
  /* get the files buffer */
  if (fd == 0) //stdin
  {
    for (int i = 0; i < size; i++)
      buffer[i] = input_getc();
    f->eax = size;
  }
  else
  {
    struct thread_file * thread_file_temp = find_file_id (*user_ptr);
    if (thread_file_temp)
    {
      acquire_lock_f ();
      f->eax = file_read (thread_file_temp->file, buffer, size);
      release_lock_f ();
    } 
    else//can't read
    {
      f->eax = -1;
    }
  }
}
```
其中is_valid_pointer如下
```c
bool 
is_valid_pointer (void* esp,uint8_t argc){
  for (uint8_t i = 0; i < argc; ++i)
  {
    if((!is_user_vaddr (esp)) || 
      (pagedir_get_page (thread_current()->pagedir, esp)==NULL)){
      return false;
    }
  }
  return true;
}
```
#### seek
直接调用file_seek函数即可,syscall.c中加入sys_seek函数
```c
void 
sys_seek(struct intr_frame* f)
{
  uint32_t *user_ptr = f->esp;
  check_ptr2 (user_ptr + 5);
  *user_ptr++;//fd
  struct thread_file *file_temp = find_file_id (*user_ptr);
  if (file_temp)
  {
    acquire_lock_f ();
    file_seek (file_temp->file, *(user_ptr+1));
    release_lock_f ();
  }
}
```
#### tell
直接调用file_tell函数即可,syscall.c中加入sys_tell函数
```c
void 
sys_tell (struct intr_frame* f)
{
  uint32_t *user_ptr = f->esp;
  check_ptr2 (user_ptr + 1);
  *user_ptr++;
  struct thread_file *thread_file_temp = find_file_id (*user_ptr);
  if (thread_file_temp)
  {
    acquire_lock_f ();
    f->eax = file_tell (thread_file_temp->file);
    release_lock_f ();
  }else{
    f->eax = -1;
  }
}
```
#### close
直接调用file_close函数即可,syscall.c中加入sys_close函数,注意需要将文件集合中对应的给删除了,否则线程最后消亡的时候会重复解除文件锁,导致出问题
```c
void 
sys_close (struct intr_frame* f)
{
  uint32_t *user_ptr = f->esp;
  check_ptr2 (user_ptr + 1);
  *user_ptr++;
  struct thread_file * opened_file = find_file_id (*user_ptr);
  if (opened_file)
  {
    acquire_lock_f ();
    file_close (opened_file->file);
    release_lock_f ();
    /* Remove the opened file from the list */
    list_remove (&opened_file->file_elem);
    /* Free opened files */
    free (opened_file);
  }
}
```

# 实验总结

## 对专业知识基本概念、基本理论和典型方法的理解。

​		pintos设计了好多东西,比如shell,线程,内存等等,这些都是一个成熟的操作系统所具有的东西.一开始我是抱着悲观的态度学习pintos的,因为相关资料都是英文的,其学习难度可以说是非常大,而网上的资料良莠不齐,后来逐渐通过对pintos的学习,越来越发现pintos的精妙之处,从操作系统最基本的线程到进程,再到内存分配,页分配,再到用户管理等内容的学习,我渐渐学习到了很多课本上提到的概念,算法但是没有实现的内容,还有对PV操作的学习,C语言的学习,学到了好多代码编程方式,比方说宏定义,全局变量,结构,枚举等等,这些都会对我以后的工作和学习带来很大的启发和影响.

## 怎么建立模型

​		要对一个问题建立模型,首先必须要总览全局,从全局入手.古人云,不谋全局者不足谋一隅,在全局的角度思考我们要做什么,要完成一个什么样的模型,然后去再自底向上分析每一个细节,完善模型的每一部分

## 如何利用基本原理解决复杂工程问题。

​		先从全局入手,统筹思考需要的知识,然后局部分析,逐步求精,仔细思考每一个地方需要用到的基本原理

## 具有实验方案设计的能力。

​		pintos的源代码本身就是非常出色的,虽然没有学过C语言,但是之前学过C++,可以说这两个语言是非常相似的.从源代码中可以学到以前学过的但是没有用过的东西,比如宏定义,全局变量,结构体,枚举,还有二级指针在内存结构中的运用.这将为我以后的方案设计提供很大的思考

## 如何对环境和社会的可持续发展

​		大概是指代码可持续吧,其实代码规范蛮重要的,一个好的项目必然遵循良好的代码规范,这样才能有利于可持续的发展

