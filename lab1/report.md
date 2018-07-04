# 实验1



## hello_linux.sh
命令分析：
>echo -e "It is the time you have wasted for your rose that makes your rose so important.\nAntoine (The Little Prince)" | ./hello_linux.sh

符号`|`表示，以前一句命令的输出创建后一句命令输入的buffer;

参考代码1使用`echo -n > output.txt`新建txt文件，再通过read方法，从buffer中读取出内容存入自定义`line`变量，并通过`echo $line >> output.txt`写入文件。

参考代码2使用`cat > output.txt`在新建txt文件的同时，将buffer中的内容读取并写入文件。

因此，参考代码2更加方便简洁。





## 调试Linux系统的启动过程


### 实验环境

- Ubuntu Linux16.04
- 宿主内核版本：4.13.0-45-generic
- qemu版本：2.5.0
- 内存：16GB
- 测试内核版本：3.18.114 



### 实验过程

- 在[此处](https://www.kernel.org/)下载linux kernel 3.18.114
- 将下载好的 linux-3.18.114.tar.xz 解压到 linux-3.18.114 目录下
- 设置内核编译选项

```shell
linux-3.18.114$ make menuconfig
```

​	为了能够使用断点调试，我们在弹出的内核配置页面需要设置：

 	1. 选中 `Kernel hacking` ->`Compile-time checks and compiler options`-> `Compile the kernel with debug info`
	2. 关闭 `Processor type and features` ->`Build a relocatable kernel`, 这样就不用在后续使用qemu时加上`- append "nokaslr"`

- 编译内核

```shell
linux-3.18.114$ make -j10 bzImage vmlinux
```

​	编译完成后，bzImage 出现在./arch/x86/boot/目录下，vmlinux出现在当前目录下。

- 制作initrd.img

```shell
linux-3.18.114$ cd arch/x86/boot
boot$ mkinitramfs -o initrd.img-3.18.114
```

- 使用qemu运行内核

```shell
boot$ qemu-system-x86_64 -kernel bzImage -initrd initrd.img-3.18.114 -append root=/dev/sda -m 2048 -S -gdb tcp::1998
```

​	注意这里加上了 `-m 2048`， 这是为了分配内存，防止开机过程错误。

-  启动gdb调试内核

  在linux-3.18.114目录下启动终端运行gdb

```shell
gdb ./vmlinux	#加载符号表，建立gdb与gdb server的连接
(gdb) target remote localhost:1234	#连接端口，远程控制qemu
(gdb) break start_kernel	#设置断点
(gdb) continue
```

​	这时遇到了`Remote 'g' packet reply is too long:` 的错误，参考[stackoverflow](https://stackoverflow.com/questions/48620622/how-to-solve-qemu-gdb-debug-error-remote-g-packet-reply-is-too-long)得知使用如下方法解决：

```shell
(gdb) disconnect
(gdb) set architecture i386:x86-64:intel
(gdb) target remote localhost: 1234
```

为了方便，我将这一步的命令写成了run.sh 。

接下来就开始正式调试分析关键事件了！



### 关键事件分析

在`start_kernel` , `set_task_stack_end_magic`, `mm_init`, `setup_arch`, `boot_cpu_init`, `rest_init`处设置断点，使用list, n, b, info 等指令进行跟踪调试。具体操作可见我的录屏视频。

第一次continue时可以看到qemu停在了内核引导的位置：

![set_magic2](https://github.com/OSH-2018/1-hhhiddleston/blob/master/pics/set_magic2.png)



**Overview：**

在完成进入内核之前所有的准备工作后，`x86_64_start_reservations` 的最后一步是调用 [init/main.c](https://github.com/torvalds/linux/blob/master/init/main.c) 中的`start_kernel()`.  `start_kernel`函数的主要目的是完成内核初始化并启动祖先进程(1号进程)。在祖先进程启动之前`start_kernel`函数做了很多事情，如锁验证器,根据处理器标识ID初始化处理器，开启cgroups子系统，设置每CPU区域环境，初始化VFS Cache机制，初始化内存管理，rcu,vmalloc,scheduler(调度器),IRQs(中断向量表),ACPI(中断可编程控制器)以及其它很多子系统。

![startkernel](https://github.com/OSH-2018/1-hhhiddleston/blob/master/pics/startkernel.png)

参考文献[1], 结合自己的理解，start_kernel()的主要流程为：

1. `lockdep_init` 锁定依赖验证器
2. `set_task_stack_end_magic(&init_task)` 在init_task堆栈的末尾设置magic number以进行溢出检测
3. `boot_init_stack_canary` 初始化 canary, 防止缓冲区溢出
4. `cgroup_init_early()` 初始化 control group 机制
5. `local_irq_disable` 关闭中断操作，因为尚未设置中断向量表
6. `boot_cpu_init()` 激活第一个处理器
7. `page_address_init()` 初始化页地址
8. `setup_arch(&command_line)` 初始化内核架构相关
9. `mm_init_cpumask(&init_mm)` 初始化内存
10. `smp_prepare_boot_cpu()` 为SMP系统里引导CPU作准备
11. `page_alloc_init()` 内存页分配
12. `trap_init` 初始化中断向量表，处理硬件中断与异常
13. `mm_init` 初始化内存管理
14. `sched_init` 初始化进程调度管理
15. `preempt_disabled()` 关闭优先权
16. `rcu_init` 初始化直接读、拷贝更新的锁机制
17. `trace_init` 初始化跟踪信息
18. `init_IRQ,softirq_init` 初始化硬件中断和软件中断
19. `time_init()` 初始化系统时钟
20. `local_irq_enablr()` 启动中断
21. `console_init()` 初始化控制台
22. `vfs_cache_init()` 初始化页表
23. `check_bugs()` 检查体系结构相关的错误
24. `rest_init()` 完成剩余部分



- **set_task_stack_end_magic()**

  我们在该处用b设置断点，用list我们可以看到这部分的代码，用 `info stack` 我们可以查看调用栈信息。

![set_magic](https://github.com/OSH-2018/1-hhhiddleston/blob/master/pics/set_magic.png)

​	由栈信息我们能看出来，`set_task_stack_end_magic`这个函数被定义在[kernel/fork.c](https://github.com/torvalds/linux/blob/master/kernel/fork.c#L297)中，通过list的代码可以看出，它的功能为设置[canary](http://en.wikipedia.org/wiki/Stack_buffer_overflow) init 进程堆栈以检测堆栈溢出。

```c
set_task_stack_end_magic(struct task_struct *tsk)
{
	unsigned long *stackend;
  stackend = end_of_stack(tsk);
  *stackend = STACK_END_MAGIC;	/* for overflow detection */
}
```

​	参考[博客](https://www.cnblogs.com/yjf512/p/5999532.html)，我们对该函数进一步分析：

`set_task_stack_end_magic()` 有两个参数：`init_task` 和 `STACK_END_MAGIC`(`0x57AC6E9D`) . 调用这个函数会先通过`end_of_stack`函数获取堆栈并赋给 `task_struct`。`task_struct` 存储了进程的所有相关信息，它在调度相关数据结构定义头文件 [include/linux/sched.h](https://github.com/torvalds/linux/blob/master/include/linux/sched.h#L1278)中定义。`init_task`通过`INIT_TASK`宏初始化，可以进行如下设置：

- 初始化进程状态为 zero，即 一个可运行进程等待CPU去运行;
- 初始化仅存的标志位 - `PF_KTHREAD` (内核线程)
- 设置一个可运行的任务列表;
- 设置进程地址空间;
- 初始化进程堆栈 `&init_thread_info` - `init_thread_union.thread_info` 和 `initthread_union`使用共用体 - `thread_union` 包含了 `thread_info`进程信息以及进程栈.

```c
union thread_union {
    struct thread_info thread_info;
    unsigned long stack[THREAD_SIZE/sizeof(long)];
};
```

接着函数将栈底地址设置为`STACK_END_MAGIC`，作为溢出标记。如果这个检查码的值被改变了，表示有人写到stack之外的区域，即表示发生了stack overflow. 

```c
if (*end_of_stack(task) != STACK_END_MAGIC) {
        // handle stack overflow here
}
```

注意这里我们用的是x86架构的初始化，堆栈是逆生成，所以堆栈底部为：`(unsigned long *）(task_thread_info(p) + 1);`



- **mm_init()**

![mem_init](https://github.com/OSH-2018/1-hhhiddleston/blob/master/pics/mem_init.png)

该函数定义在init/main.c 中

```C
static void __init mm_init(void)

{
    /*
     * page_cgroup requires contiguous pages,
     * bigger than MAX_ORDER unless SPARSEMEM.
     */
    page_cgroup_init_flatmem(); 
    mem_init(); 
    kmem_cache_init(); 
    percpu_init_late(); 
    pgtable_cache_init(); 
    vmalloc_init();
}
```

该函数执行完之后，我们**无法再用**alloc_bootmem(), alloc_bootmem_low()等申请低端内存的函数申请内存了，也不能申请大块连续物理内存了。

其中，`page_cgroup_init_flatmem()`函数由于mem_cgroup_disabled为true，直接返回了。

 `mem_init()`函数释放node_bootmem_map, 将memboot分配器转化为伙伴系统分配器。

 `kmem_cache_init()`初始化kmem_cache和kmalloc_caches，使能slab内存分配器。

`percpu_init_late()`将静态数组换成动态分配的空间。

`vmalloc_init()`初始化vmalloc。vmalloc分配的内存虚拟地址是连续的，而物理地址则无需连续。



- **setup_arch(&command_line)**

![setup_arch](https://github.com/OSH-2018/1-hhhiddleston/blob/master/pics/setup_arch.png)

该函数内容较长，没有在终端完全打印, 此函数在arch/x86/kernel/setup.c中定义。

通过`info stack`(上图)和`info register`查看该函数的调用栈和寄存器修改情况。

![setup_arch_reg](https://github.com/OSH-2018/1-hhhiddleston/blob/master/pics/setup_arch_reg.png)

rip是setup_arch, rax, rsi等寄存器都有变化。

每个体系都有自己的setup_arch()函数，是体系结构相关的，具体编译哪个体系的setup_arch()函数，由顶层Makefile中的ARCH变量决定。重要的函数调用如下：

`start_kernel ()` --> `setup_arch ()` --> `paging_init ()` --> `bootmem_init ()` --> `alloc_bootmem_low_pages ()`

它首先通过检测出来的处理器类型进行处理器内核的初始化，然后通过 `bootmem_init()`函数根据系统定义的 meminfo 结构进行内存结构的初始化,完成位图分配器的建立，最后调用`paging_init()`开启 MMU，创建内核页表，映射所有的物理内存和 IO空间。



- **boot_cpu_init()**

该函数的功能是通过掩码初始化每一个CPU。

```C
void __init boot_cpu_init(void) { 
    int cpu = smp_processor_id(); 
    /* Mark the boot cpu "present", "online" etc for SMP and UP case */ 		  
    set_cpu_online(cpu, true); 
    set_cpu_active(cpu, true); 
    set_cpu_present(cpu, true); 
    set_cpu_possible(cpu, true); 
}
```

`int cpu = smp_processor_id()` 先获取cpu的id，在SMP下，获取第一个处理器ip，非SMP，第一个cpu的id为0。后面就是设置cpu的四个标志位。

在 Linux 内核中主要有 4 个 cpu mask array 记录 CPU 的使用情形：

- cpu_possible_mask: 硬件上实际可用的 CPU， boot time 时决定
- cpu_present_mask: 目前指派使用的 CPU
- cpu_online_mask: 可以被排程的 CPU (boot CPU 以外的 CPU 由 smp_init() 完成 CPU 初始化后设定为 online)
- cpu_active_mask: 可以依据 domain/group 进行排程的 CPU (负载平衡)

如果 CONFIG_HOTPLUG_CPU 有设定的话，Linux 内核启动支援 CPU hotplug 的机制。 这边的 hotplug 不是指硬件上的热插拔，而是指系統可以动态决定 CPU 的使用，系统可以经由设定 cpu_present_mask 决定要使用的 CPU，举例来说，系统可以在低负载时将一些 CPU 关掉，节省电源的消耗。 除了 cpu_present_mask 可以由外部设定，其它的 3个 mask 都是只读的，由核心维护。



- **rest_init()**

![rest_init](https://github.com/OSH-2018/1-hhhiddleston/blob/master/pics/rest_init.png)

这是start_kernel最后调用的函数。通过 `info register` 我们能看出寄存器的值发生了很多变化(在demo里能更清楚的看出前后对比)。

![rest_init_reg](https://github.com/OSH-2018/1-hhhiddleston/blob/master/pics/rest_init_reg.png)

同时qemu也多了很多内容：

![rest_init2](https://github.com/OSH-2018/1-hhhiddleston/blob/master/pics/rest_init2.png)

在网站上找到该函数的完整源码为：

```c
static noinline void __ref rest_init(void)
{
	struct task_struct *tsk;
	int pid;

	rcu_scheduler_starting();
	/*
	 * We need to spawn init first so that it obtains pid 1, however
	 * the init task will end up wanting to create kthreads, which, if
	 * we schedule it before we create kthreadd, will OOPS.
	 */
	pid = kernel_thread(kernel_init, NULL, CLONE_FS);
	/*
	 * Pin init on the boot CPU. Task migration is not properly working
	 * until sched_init_smp() has been run. It will set the allowed
	 * CPUs for init to the non isolated CPUs.
	 */
	rcu_read_lock();
	tsk = find_task_by_pid_ns(pid, &init_pid_ns);
	set_cpus_allowed_ptr(tsk, cpumask_of(smp_processor_id()));
	rcu_read_unlock();

	numa_default_policy();
	pid = kernel_thread(kthreadd, NULL, CLONE_FS | CLONE_FILES);
	rcu_read_lock();
	kthreadd_task = find_task_by_pid_ns(pid, &init_pid_ns);
	rcu_read_unlock();

	/*
	 * Enable might_sleep() and smp_processor_id() checks.
	 * They cannot be enabled earlier because with CONFIG_PRREMPT=y
	 * kernel_thread() would trigger might_sleep() splats. With
	 * CONFIG_PREEMPT_VOLUNTARY=y the init task might have scheduled
	 * already, but it's stuck on the kthreadd_done completion.
	 */
	system_state = SYSTEM_SCHEDULING;

	complete(&kthreadd_done);

	/*
	 * The boot idle thread must execute schedule()
	 * at least once to get things moving:
	 */
	schedule_preempt_disabled();
	/* Call into cpu_idle with preempt disabled */
	cpu_startup_entry(CPUHP_ONLINE);
}
```

在 rest_init() 中主要进行 4 件工作:

- 创建核心线程 kernel_init
- 创建核心线程 kthreadd
- 至少执行一次 schedule() 进行排程调度，让刚刚创建的核心线程能够开始执行。
- 进入 cpu_idle_loop() 变成 idle process (pid=0) 处理 idle task

idle进程是进程0，即空闲进程，也就是死循环。 kernel_init是进程1，被称为init进程，是所有用户态进程的父进程。kthreadd是进程2，这是linux内核的守护进程，它用来管理调度其他内核进程且用来保证linux内核自己本身能正常工作。

简单来说，linux内核最终的状态是：有事干的时候去执行有意义的工作（执行各个进程任务），实在没活干的时候就去死循环（实际上死循环也可以看成是一个任务）。之前已经启动了内核调度系统，调度系统会负责考评系统中所有的进程，这些进程里面只要有哪个需要被运行，调度系统就会终止cpu_idle死循环进程（空闲进程）转而去执行有意义的进程。



## 使用printk和空循环进行内核追踪

linux启动过程log包含了丰富的信息，但是一些感兴趣的信息未必被输出。在本实验中，使用了这样一个自定义tracer函数：
```
void __init mybreakpoint(const char *msg){
	printk(KERN_NOTICE "MY_MESSAGE : %s\n", msg);
}
```
然后在感兴趣的代码处调用，并使用gdb在mybreakpoint处设置断点，也可以在linux启动后，利用关键字“MY_MESSAGE”查询相关log。



参考资料：

(1)  [深入浅出start_kernel](https://danielmaker.github.io/blog/linux/inside_start_kernel.html)

(2) [g-packet-reply-is-too-long](https://stackoverflow.com/questions/48620622/how-to-solve-qemu-gdb-debug-error-remote-g-packet-reply-is-too-long)

(3) [伙伴算法](https://blog.csdn.net/wh8_2011/article/details/51360177)

(4) [linux内核启动分析](https://blog.csdn.net/boarmy/article/details/8652343)

(5) [linux内核移植](http://www.bubuko.com/infodetail-1720220.html)