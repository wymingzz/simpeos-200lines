/**
 * 功能：16位与32位的启动混合代码
 *
 *创建时间：2022年8月31日
 *作者：李述铜
 *联系邮箱: 527676163@qq.com
 *相关信息：此工程为《从0写x86 Linux操作系统》的前置课程，用于帮助预先建立对32位x86体系结构的理解。整体代码量不到200行（不算注释）
 *课程请见：https://study.163.com/course/introduction.htm?courseId=1212765805&_trace_c_p_k2_=0bdf1e7edda543a8b9a0ad73b5100990
 */
	#include "os.h"

	// 声明本地以下符号是全局的，在其它源文件中可以访问
	.global _start

	// 指定以下的代码生成16位的机器指令，这样才能在启动时的实模式下运行
  	.code16

	// 以下是代码区
 	.text
_start:
	mov $0, %ax
	mov %ax, %ds
	mov %ax, %es
	mov %ax, %ss
	mov %ax, %gs
 	mov %ax, %fs
	mov $_start, %esp

read_self_all:
	mov $_start_32, %bx		// 目标内存地址
	mov $0x2, %cx			// 从哪个扇区开始读
	mov $0x240, %ax			// ah为读取的扇区个数，al表示是读磁盘
	mov $0x80, %dx			// 从哪个磁盘读取数据
	int $0x13

	jc	read_self_all		// 判断flags的cf表示位是否是1，即读取数据是否出错，出错则跳转

	cli						// 关中断

	lgdt [gdt_desc]			// 读取GDT表的段选择子
	// 将寄存器cr0的第一位置1，即进入保护模式
	mov $1, %eax
	lmsw %ax
	jmp $KERNEL_CODE_SEG, $_start_32

	.org 0x1fe
	.byte 0x55, 0xaa

	.code32
	.text
_start_32:
	mov $KERNEL_DATA_SEG, %ax
	mov %ax, %ds
	mov %ax, %es
	mov %ax, %ss
	mov %ax, %gs
 	mov %ax, %fs
	mov $_start, %esp

	call os_init

	// 将页目录表的地址放入cr3寄存器中
	mov $pg_dir, %eax
	mov %eax, %cr3

	// 将寄存器cr4的第四位置1，即开启4MB的分页映射机制
	mov %cr4, %eax
	orl $(1 << 4), %eax
	mov %eax, %cr4

	// 将寄存器cr0的最高位置1，即进入分页模式
	mov %cr0, %eax
	orl $(1 << 31), %eax
	mov %eax, %cr0


	jmp .

gdt_desc:
	.word (256*8) - 1		// 16位界限
	.long gbt_table			// 32位基地址，即gbt_table的内存地址