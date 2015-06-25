/*
 *  linux/kernel/fork.c
 *
 *  (C) 1991  Linus Torvalds
 */

/*
 *  'fork.c' contains the help-routines for the 'fork' system call
 * (see also system_call.s), and some misc functions ('verify_area').
 * Fork is rather simple, once you get the hang of it, but the memory
 * management can be a bitch. See 'mm/mm.c': 'copy_page_tables()'
 */
#include <errno.h>

#include <linux/sched.h>
#include <linux/kernel.h>
#include <asm/segment.h>
#include <asm/system.h>

extern void write_verify(unsigned long address);

long last_pid=0;

void verify_area(void * addr,int size)
{
	unsigned long start;

	start = (unsigned long) addr;
	size += start & 0xfff;
	start &= 0xfffff000;
	start += get_base(current->ldt[2]);
	while (size>0) {
		size -= 4096;
		write_verify(start);
		start += 4096;
	}
}

int copy_mem(int nr,struct task_struct * p)
{
	unsigned long old_data_base,new_data_base,data_limit;
	unsigned long old_code_base,new_code_base,code_limit;

	code_limit=get_limit(0x0f);
	data_limit=get_limit(0x17);
	old_code_base = get_base(current->ldt[1]);
	old_data_base = get_base(current->ldt[2]);
	if (old_data_base != old_code_base)
		panic("We don't support separate I&D");
	if (data_limit < code_limit)
		panic("Bad data_limit");
	new_data_base = new_code_base = nr * 0x4000000;
	p->start_code = new_code_base;
	set_base(p->ldt[1],new_code_base);
	set_base(p->ldt[2],new_data_base);
	if (copy_page_tables(old_data_base,new_data_base,data_limit)) {
		printk("free_page_tables: from copy_mem\n");
		free_page_tables(new_data_base,data_limit);
		return -ENOMEM;
	}
	return 0;
}

/*
 *  Ok, this is the main fork-routine. It copies the system process
 * information (task[nr]) and sets up the necessary registers. It
 * also copies the data segment in it's entirety.
 */
int copy_process(int nr,long ebp,long edi,long esi,long gs,long none,
		long ebx,long ecx,long edx,
		long fs,long es,long ds,
		long eip,long cs,long eflags,long esp,long ss)
{
	/*melon - 添加用来取得内核栈指针*/
	long * krnstack;
	/*melon added End*/
	struct task_struct *p;
	int i;
	struct file *f;

	p = (struct task_struct *) get_free_page();
	if (!p)
		return -EAGAIN;

	/*melon  -取得当前子进程的内核栈指针*/
	krnstack=(long)(PAGE_SIZE+(long)p);
	/*melon added End*/
	task[nr] = p;
	*p = *current;	/* NOTE! this doesn't copy the supervisor stack */
	p->state = TASK_UNINTERRUPTIBLE;
	p->pid = last_pid;
	p->father = current->pid;
	p->counter = p->priority;
	//初始化内核栈内容，由于系统不再使用tss进行切换，所以内核栈内容要自已安排好
	*(--krnstack) = ss & 0xffff; //保存用户栈段寄存器,这些参数均来自于此次的函数调用，
				      //即父进程压栈内容，看下面关于tss的设置此处和那里一样。
	*(--krnstack) = esp; //保存用户栈顶指针
	*(--krnstack) = eflags; //保存标识寄存器
	*(--krnstack) = cs & 0xffff; //保存用户代码段寄存器
	*(--krnstack) = eip; //保存ＩＰ指针数据,iret时会出栈使用
	//下面是iret时要使用的栈内容，由于switch_to返回后first_return_fromkernel运行时会模拟system_call的返回，所以此处要再保存寄存器现场。
	*(--krnstack) = ds & 0xffff;
	*(--krnstack) = es & 0xffff;
	*(--krnstack) = fs & 0xffff;
	*(--krnstack) = gs & 0xffff;
	*(--krnstack) = esi;
	*(--krnstack) = edi;
	*(--krnstack) = edx;
	//*(--krnstack) = ecx;
	//*(--krnstack) = ebx;
	//*(--krnstack) = 0; //此处应是返回的子进程pid//eax;
			   //其意义等同于p->tss.eax=0;因为tss不再被使用，
			   //所以返回值在这里被写入栈内，在switch_to返回前被弹出给eax;
	
	//switch_to的ret语句将会用以下地址做为弹出进址进行运行
	*(--krnstack) = (long)first_return_from_kernel;
	//*(--krnstack) = &first_return_from_kernel;
	//这是在switch_to一起定义的一段用来返回用户态的汇编标号,也就是
	//以下是子进程开始运行时要使用出栈数据，即eax,ebx,ecx ebp,这是在switch_to中使用到的，
	//在switch_to结束后会出栈的内容。也就是说如果子进程得到机会运行，一定也是先
	//到switch_to的结束部分去运行，因为PCB是在那里被切换的，栈也是在那里被切换的，
	//所以下面的数据一定要事先压到一个要运行的进程中才可以平衡。
	*(--krnstack) = ebp;
	*(--krnstack) = eflags; //新添加
	*(--krnstack) = ecx;
	*(--krnstack) = ebx;
	*(--krnstack) = 0; //这里的eax=0是switch_to返回时弹出的，而且在后面没有被修改过。
		  	    

	//将内核栈的栈顶保存到内核指针处
	p->kernelstack=krnstack; //保存当前栈顶
	p->eip=(long)first_switch_from;//此处为第一次被调度时使用的地址
	/*melon added End*/
	p->signal = 0;
	p->alarm = 0;
	p->leader = 0;		/* process leadership doesn't inherit */
	p->utime = p->stime = 0;
	p->cutime = p->cstime = 0;
	p->start_time = jiffies;
	p->tss.back_link = 0;
	p->tss.esp0 = PAGE_SIZE + (long) p;
	p->tss.ss0 = 0x10;
	p->tss.eip = eip;
	p->tss.eflags = eflags;
	p->tss.eax = 0;
	p->tss.ecx = ecx;
	p->tss.edx = edx;
	p->tss.ebx = ebx;
	p->tss.esp = esp;
	p->tss.ebp = ebp;
	p->tss.esi = esi;
	p->tss.edi = edi;
	p->tss.es = es & 0xffff;
	p->tss.cs = cs & 0xffff;
	p->tss.ss = ss & 0xffff;
	p->tss.ds = ds & 0xffff;
	p->tss.fs = fs & 0xffff;
	p->tss.gs = gs & 0xffff;
	p->tss.ldt = _LDT(nr);
	p->tss.trace_bitmap = 0x80000000;
	if (last_task_used_math == current)
		__asm__("clts ; fnsave %0"::"m" (p->tss.i387));
	if (copy_mem(nr,p)) {
		task[nr] = NULL;
		free_page((long) p);
		return -EAGAIN;
	}
	for (i=0; i<NR_OPEN;i++)
		if ((f=p->filp[i]))
			f->f_count++;
	if (current->pwd)
		current->pwd->i_count++;
	if (current->root)
		current->root->i_count++;
	if (current->executable)
		current->executable->i_count++;
	set_tss_desc(gdt+(nr<<1)+FIRST_TSS_ENTRY,&(p->tss));
	set_ldt_desc(gdt+(nr<<1)+FIRST_LDT_ENTRY,&(p->ldt));
	/*添加代码进行进程信息记录，新建的进程均要记录两次N，J*/
	fprintk(3,"%ld\t%c\t%ld\n",last_pid,'N',jiffies);
	/*添加完毕*/
	/*添加就绪状态*/
	fprintk(3,"%ld\t%c\t%ld\n",last_pid,'J',jiffies);
	/*添加完毕*/
	p->state = TASK_RUNNING;	/* do this last, just in case */

	return last_pid;
}

int find_empty_process(void)
{
	int i;

	repeat:
		if ((++last_pid)<0) last_pid=1;
		for(i=0 ; i<NR_TASKS ; i++)
			if (task[i] && task[i]->pid == last_pid) goto repeat;
	for(i=1 ; i<NR_TASKS ; i++)
		if (!task[i])
			return i;
	return -EAGAIN;
}
