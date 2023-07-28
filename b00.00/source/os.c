/**
 * 功能：32位代码，完成多任务的运行
 *
 *创建时间：2022年8月31日
 *作者：李述铜
 *联系邮箱: 527676163@qq.com
 *相关信息：此工程为《从0写x86 Linux操作系统》的前置课程，用于帮助预先建立对32位x86体系结构的理解。整体代码量不到200行（不算注释）
 *课程请见：https://study.163.com/course/introduction.htm?courseId=1212765805&_trace_c_p_k2_=0bdf1e7edda543a8b9a0ad73b5100990
 */
#include "os.h"

//  类型定义
typedef unsigned char uint8_t;
typedef unsigned short uint16_t;
typedef unsigned int uint32_t;

/// @brief 系统调用函数(目前只有显示字符串)
/// @param func 功能号
/// @param str  需要显示的字符串
/// @param color    字符串颜色
void do_syscall(int func, char *str, char color){
    static int row = 0;                 // 行号

    if(func == 2){                      // 显示字符串
        uint16_t *dest = (uint16_t*) 0xb8000 + 80 * row;
        while (*str)
        {
            *dest++ = *str++ | (color << 8);
        }

        row = (row >= 25) ? 0 : (row + 1);
    }
    for(int i = 0; i < 0xfffff; i++);
}

/// @brief 使用系统调用门调用do_syscall函数
/// @param str  需要显示的字符串
/// @param color    字符串的颜色
void sys_show(char *str, char color){
    uint32_t addr[] = {0, SYSCALL_SEG};
    __asm__ __volatile("push %[color]; push %[str]; push %[id]; lcall *(%[a])"::[a]"r"(addr), [color]"m"(color), [str]"m"(str), [id]"r"(2));
}

void task_0(void){
    char* str = "task a: 1234";
    uint8_t color = 0;

    for(;;){
        sys_show(str, color++);
    }
}

void task_1(void){
    char* str = "task b: 5678";
    uint8_t color = 0;

    for(;;){
        sys_show(str, color--);
    }
}

void task_sched(void){
    static int task_tss = TASK0_TSS_SEG;

    task_tss = (task_tss == TASK0_TSS_SEG? TASK1_TSS_SEG: TASK0_TSS_SEG);

    uint32_t addr[] = {0, task_tss};
    __asm__ __volatile("ljmpl *(%[a])"::[a]"r"(addr));
}

#define PDE_P   (1 << 0)  // PDE页表项的存在位
#define PDE_W   (1 << 1)  // PDE页表项的读写位
#define PDE_U   (1 << 2)  // PDE页表项的权限位——是否会被低内存级的段读写
#define PDE_PS  (1 << 7)  // PDE页表项的大小是4KB还是4MB——1为4MB

#define MAP_ADDR    0x80000000      // map_phy_buffer映射到的虚拟内存地址
uint8_t map_phy_buffer[4096] __attribute__((aligned(4096))) = {0x36};                                           // 映射一个4KB大小的空间

static uint32_t page_table[1024] __attribute__((aligned(4096))) = {PDE_U};

uint32_t pg_dir[1024] __attribute__((aligned(4096))) = {                                                        // 页目录表     以4KB对齐
    [0] = (0) | PDE_P | PDE_W |PDE_U | PDE_PS,
};

/// @brief 栈段
uint32_t task0_dpl3_stack[1024], task0_dpl0_stack[1024], task1_dpl3_stack[1024], task1_dpl0_stack[1024];

/**
 * @brief 任务0的任务状态段
 */
uint32_t task0_tss[] = {
    // prelink, esp0, ss0, esp1, ss1, esp2, ss2
    0,  (uint32_t)task0_dpl0_stack + 4*1024, KERNEL_DATA_SEG , /* 后边不用使用 */ 0x0, 0x0, 0x0, 0x0,
    // cr3, eip, eflags, eax, ecx, edx, ebx, esp, ebp, esi, edi,
    (uint32_t)pg_dir,  (uint32_t)task_0/*入口地址*/, 0x202, 0xa, 0xc, 0xd, 0xb, (uint32_t)task0_dpl3_stack + 4*1024/* 栈 */, 0x1, 0x2, 0x3,
    // es, cs, ss, ds, fs, gs, ldt, iomap
    APP_DATA_SEG, APP_CODE_SEG, APP_DATA_SEG, APP_DATA_SEG, APP_DATA_SEG, APP_DATA_SEG, 0x0, 0x0,
};

uint32_t task1_tss[] = {
    // prelink, esp0, ss0, esp1, ss1, esp2, ss2
    0,  (uint32_t)task1_dpl0_stack + 4*1024, KERNEL_DATA_SEG , /* 后边不用使用 */ 0x0, 0x0, 0x0, 0x0,
    // cr3, eip, eflags, eax, ecx, edx, ebx, esp, ebp, esi, edi,
    (uint32_t)pg_dir,  (uint32_t)task_1/*入口地址*/, 0x202, 0xa, 0xc, 0xd, 0xb, (uint32_t)task1_dpl3_stack + 4*1024/* 栈 */, 0x1, 0x2, 0x3,
    // es, cs, ss, ds, fs, gs, ldt, iomap
    APP_DATA_SEG, APP_CODE_SEG, APP_DATA_SEG, APP_DATA_SEG, APP_DATA_SEG, APP_DATA_SEG, 0x0, 0x0,
};

struct{uint16_t offset_l, selector, attr, offset_h;}idt_table[256] __attribute__((aligned(8)));

struct{uint16_t limit_l,base_l,basehl_attr,base_limit;}gbt_table[256] __attribute__((aligned(8))) = {           // 全局描述符表  __attribute__((aligned(8)))以8字节对齐
    // 0x00cf9a000000ffff - 从0地址开始，P存在，DPL=0，Type=非系统段，32位代码段（非一致代码段），界限4G，
    // 00000000 11001111 10011010 00000000      0x00 0xcf 0x9a 0x00
    // 00000000 00000000 11111111 11111111      0x00 0x00 0xff 0xff
    [KERNEL_CODE_SEG / 8] = {0xffff, 0x0000, 0x9a00, 0x00cf},
    // 0x00cf92000000ffff - 从0地址开始，P存在，DPL=0，Type=非系统段，数据段，界限4G，可读写
    // 00000000 11001111 10010010 00000000      0x00 0xcf 0x92 0x00
    // 00000000 00000000 11111111 11111111      0x00 0x00 0xff 0xff
    [KERNEL_DATA_SEG / 8] = {0xffff, 0x0000, 0x9200, 0x00cf},

    [APP_CODE_SEG / 8] = {0xffff, 0x0000, 0xfa00, 0x00cf},
    [APP_DATA_SEG / 8] = {0xffff, 0x0000, 0xf200, 0x00cf},

    [TASK0_TSS_SEG / 8] = {0x68, 0, 0xe900, 0},
    [TASK1_TSS_SEG / 8] = {0x68, 0, 0xe900, 0},

    [SYSCALL_SEG / 8] = {0, KERNEL_CODE_SEG, 0xec03, 0},
};

/// @brief 调用汇编函数outb
/// @param data 传入al寄存器的值
/// @param port 传入dx寄存器的值
void outb(uint8_t data, uint16_t port){
    __asm__ __volatile__("outb %[v], %[p]"::[p]"d"(port), [v]"a"(data));
}

// 声明汇编代码中的自定义函数
void timer_init(void);
void syscall_handler(void);

/// @brief 对os进行初始化
/// @param
void os_init(void){
    // 对8259芯片进行初始化
    outb(0x11, 0x20);
    outb(0x11, 0xa0);
    // 中断产生时，定义对应引脚查找IDT表中的位置
    outb(0x20, 0x21);
    outb(0x28, 0xa1);
    // 配置从片的引脚2连接到主片的引脚2(引脚从0开始算起)
    outb(1 << 2, 0x21);
    outb(1 << 2, 0xa1);

    outb(0x1, 0x21);
    outb(0x1, 0xa1);
    // 屏蔽除了主片的0号引脚以外的所有中断信号
    outb(0xfe, 0x21);
    outb(0xff, 0xa1);

    // 对8253定时器进行初始化，设定成每100ms产生一次中断
    uint16_t tmo = 1193180 / 100;
    outb(0x36, 0x43);
    outb((uint8_t)tmo, 0x40);
    outb(tmo >> 8, 0x40);

    // 对IDT表的0x20位置进行设置
    idt_table[0x20].offset_l = (uint32_t)timer_init & 0xffff;
    idt_table[0x20].offset_h = (uint32_t)timer_init >> 16;
    idt_table[0x20].selector = KERNEL_CODE_SEG;
    idt_table[0x20].attr = 0x8e00;                              // 0x8e00 1000 1110 0000 0000  32位，中断门，存在

    // 设置两个任务段选择子的基地址
    gbt_table[TASK0_TSS_SEG / 8].base_l = (uint16_t)(uint32_t)task0_tss;
    gbt_table[TASK1_TSS_SEG / 8].base_l = (uint16_t)(uint32_t)task1_tss;

    // 设置系统调用门的段偏移量
    gbt_table[SYSCALL_SEG / 8].limit_l = (uint16_t)(uint32_t)syscall_handler;

    // 在虚拟内存地址为0x80000000的地方映射一块4KB的空间
    pg_dir[MAP_ADDR >> 22]= (uint32_t)page_table | PDE_P | PDE_W | PDE_U;
    page_table[(MAP_ADDR >> 12) & 0x3ff] = (uint32_t)map_phy_buffer | PDE_P | PDE_W;
}