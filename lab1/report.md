# 实验1
**吴紫薇 PB16110763**

## hello_linux.sh
命令分析：
>echo -e "It is the time you have wasted for your rose that makes your rose so important.\nAntoine (The Little Prince)" | ./hello_linux.sh

符号`|`表示，以前一句命令的输出创建后一句命令输入的buffer;

参考代码1使用`echo -n > output.txt`新建txt文件，再通过read方法，从buffer中读取出内容存入自定义`line`变量，并通过`echo $line >> output.txt`写入文件。

参考代码2使用`cat > output.txt`在新建txt文件的同时，将buffer中的内容读取并写入文件。

因此，参考代码2更加方便简洁。

## 调试Linux系统的启动过程
1. install qemu
> sudo apt-get install qemu

2. download linux kernel 
> https://launchpad.net/linux/+milestone/3.18.6

3. kernel configuration
> make defconfig

4. kernel compilation 
> make -j10

5. start linux using qemu
> qemu-system-x86_64 -kernel linux-3.18.6/arch/x86/boot/bzImage -initrd ./rootfs.img -gdb tcp::1998 -S

6. debug booting using gdb tool
> gdb 
> 
> (gdb) file linux-3.18.6/vmlinux
> 
>  (gdb) target remote:1998

7. setting breakpoints at key-events
> (gdb) break start_kernel
> 
> (gdb) break trap_init
> 
> (gdb) break mm_init
> 
> (gdb) break sched_init
> 
> (gdb) break rest_init
> 
> (gdb) break run_init_process

8. enter c to continue the booting

```
(gdb) c
Continuing.
Breakpoint 1, start_kernel () at init/main.c:501
warning: Source file is more recent than executable.
501	{
/* 可视为kernel初始化的main函数，初始化过程由其完成 
** 其中set_task_stack_end_magic(&init_task)创建0号进程*/

(gdb) c
Continuing.
Breakpoint 2 trap_init () at arch/x86/kernel/traps.c:804
804		set_intr_gate(X86_TRAP_DE, divide_error);
/* 进行中断相关的初始化 */

(gdb) c
Continuing.
Breakpoint 3, start_kernel () at init/main.c:562
562		mm_init();
/* 内存管理相关的初始化 */

(gdb) c
Continuing.
Breakpoint 4, sched_init () at kernel/sched/core.c:7012
7012		if (alloc_size) {
/* 调度模块相关的初始化 */

(gdb) c
Continuing.
Breakpoint 5, rest_init () at init/main.c:394
394	{
/* kernel_thread(kernel_init, NULL, CLONE_FS)创建init进程，即1号进程， 
** 同时0号进程完成启动相关工作，变为idle进程 
** 内核启动至此结束 */

(gdb) c
Continuing.
Breakpoint 6, run_init_process (init_filename=0xc18cc3ee "/init")
    at init/main.c:911
911	{
/* 执行init进程，进入user-mode */
```

## 使用printk和空循环进行内核追踪

linux启动过程log包含了丰富的信息，但是一些感兴趣的信息未必被输出。在本实验中，使用了这样一个自定义tracer函数：
```
void __init mybreakpoint(const char *msg){
	printk(KERN_NOTICE "MY_MESSAGE : %s\n", msg);
}
```
然后在感兴趣的代码处调用，并使用gdb在mybreakpoint处设置断点，也可以在linux启动后，利用关键字“MY_MESSAGE”查询相关log。