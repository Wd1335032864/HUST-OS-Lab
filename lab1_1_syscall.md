# Lab1_1_syscall

- 观察app_helloworld.c：

```
#include "user_lib.h"

int main(void) {
  printu("Hello world!\n");

  exit(0);
}
```

- 可知就是printu引出的系统调用，所以完善相关系统调用，追踪代码到相关位置补充代码

- printu函数定义在user_lib.h中，并且在user_lib.c中实现，观察user_lib.c：

  ```
  #include "user_lib.h"
  #include "util/types.h"
  #include "util/snprintf.h"
  #include "kernel/syscall.h"
  
  int do_user_call(uint64 sysnum, uint64 a1, uint64 a2, uint64 a3, uint64 a4, uint64 a5, uint64 a6,
                   uint64 a7) {
    int ret;
    asm volatile(
        "ecall\n"
        "sw a0, %0"  // returns a 32-bit value
        : "=m"(ret)
        :
        : "memory");
  
    return ret;
  }
  
  int printu(const char* s, ...) {
    va_list vl;
    va_start(vl, s);
  
    char out[256];  // fixed buffer size.
    int res = vsnprintf(out, sizeof(out), s, vl);
    va_end(vl);
    const char* buf = out;
    size_t n = res < sizeof(out) ? res : sizeof(out);
  
    return do_user_call(SYS_user_print, (uint64)buf, n, 0, 0, 0, 0, 0);
  }
  
  int exit(int code) {
    return do_user_call(SYS_user_exit, code, 0, 0, 0, 0, 0, 0); 
  }
  ```

- 可以发现exit和printu都转化成了对do_user_call的调用，并分别通过参数SYS_user_print和SYS_user_exit来指明系统调用号

- 而do_user_call则是通过ecall指令来完成系统调用的，由文档可知，编译器在ecall 指令前帮助我们将do_user_call的8个参数载入到了模拟机器的a0到a7的8个寄存器中

- ecall指令将根据a0寄存器的值获得系统调用号，并将RISC-V转到S模式的trap处理入口smode_trap_vector执行，这一入口在strp_vector.S定义：

  ```
  smode_trap_vector:
      # swap a0 and sscratch, so that points a0 to the trapframe of current process
      csrrw a0, sscratch, a0
  
      # save the context (user registers) of current process in its trapframe.
      addi t6, a0 , 0
  
      # store_all_registers is a macro defined in util/load_store.S, it stores contents
      # of all general purpose registers into a piece of memory started from [t6].
      store_all_registers
  
      # come back to save a0 register before entering trap handling in trapframe
      # [t0]=[sscratch]
      csrr t0, sscratch
      sd t0, 72(a0)
  
      # use the "user kernel" stack (whose pointer stored in p->trapframe->kernel_sp)
      ld sp, 248(a0)
  
      # load the address of smode_trap_handler() from p->trapframe->kernel_trap
      ld t0, 256(a0)
  
      # jump to smode_trap_handler() that is defined in kernel/trap.c
      jr t0
  ```

- 看第一行注释可知，交换a0和sscratch，让a0指向当前进程的trapframe，此时我们可以先观察process.h中process和trapframe的定义：

  ```
  typedef struct trapframe_t {
    // space to store context (all common registers)
    /* offset:0   */ riscv_regs regs; 
  
    // process's "user kernel" stack
    /* offset:248 */ uint64 kernel_sp;    
    // pointer to smode_trap_handler
    /* offset:256 */ uint64 kernel_trap;
    // saved user process counter
    /* offset:264 */ uint64 epc;
  }trapframe;
  
  // the extremely simple definition of process, used for begining labs of PKE
  typedef struct process_t {
    // pointing to the stack used in trap handling.
    uint64 kstack;
    // trapframe storing the context of a (User mode) process.
    trapframe* trapframe;
  }process;
  ```

- 然后大概能知道process的简易结构包含了进程trap handing时的栈和trapframe；而trapframe包含了进程在用户态时的上下文、用户内核栈、smode_trap_handler()的地址和进程计数器。

- 再回到smode_trap_vector，接下来将t6赋值为a0的值，并将所有通用寄存器保存到t6寄存器所指定首地址的内存区域，该动作由store_all_registers完成，由上可知保存到了trapframe的regs中（可自行查看riscv_regs 的定义）

- 再看后面代码和trapframe定义：

  ```
   csrr t0, sscratch
      sd t0, 72(a0)
  
      # use the "user kernel" stack (whose pointer stored in p->trapframe->kernel_sp)
      ld sp, 248(a0)
  
      # load the address of smode_trap_handler() from p->trapframe->kernel_trap
      ld t0, 256(a0)
  
      # jump to smode_trap_handler() that is defined in kernel/trap.c
      jr t0
  ```

- 此时便将a0寄存器中的系统调用号保存到内核堆栈中，也就是regs中某个寄存器，如果查看了riscv_regs 的定义可以发现72(a0)处就是定义了一个`uint64 a0`，要注意riscv_resgs是为了保存上下文定义的，与a0寄存器区分开来。再就是让sp指向用户内核栈，最后跳到smode_trap_handler()处，该函数定义在strap.c中：

  ```
  void smode_trap_handler(void) {
    // make sure we are in User mode before entering the trap handling.
    // we will consider other previous case in lab1_3 (interrupt).
    if ((read_csr(sstatus) & SSTATUS_SPP) != 0) panic("usertrap: not from user mode");
  
    assert(current);
    // save user process counter.
    current->trapframe->epc = read_csr(sepc);
  
    // if the cause of trap is syscall from user application.
    // read_csr() and CAUSE_USER_ECALL are macros defined in kernel/riscv.h
    if (read_csr(scause) == CAUSE_USER_ECALL) {
      handle_syscall(current->trapframe);
    } else {
      sprint("smode_trap_handler(): unexpected scause %p\n", read_csr(scause));
      sprint("            sepc=%p stval=%p\n", read_csr(sepc), read_csr(stval));
      panic( "unexpected exception happened.\n" );
    }
  
    // continue (come back to) the execution of current process.
    switch_to(current);
  }
  ```

- 第一行对进入当前特权级模式（S模式）之前的模式进行判断，确保进入前是用户模式，相关具体定义在riscv.h中；然后再保存发生系统调用的指令地址；if语句进一步判断导致进入当前模式的原因。观察可知如果为系统调用则执行handle_syscall()函数，而其他原因就打印错误信息，因为这里还未做完善。所以继续追溯handle_syscall()函数，他也定义在该文件中：

  ```
  static void handle_syscall(trapframe *tf) {
    // tf->epc points to the address that our computer will jump to after the trap handling.
    // for a syscall, we should return to the NEXT instruction after its handling.
    // in RV64G, each instruction occupies exactly 32 bits (i.e., 4 Bytes)
    tf->epc += 4;
  
    // TODO (lab1_1): remove the panic call below, and call do_syscall (defined in
    // kernel/syscall.c) to conduct real operations of the kernel side for a syscall.
    // IMPORTANT: return value should be returned to user app, or else, you will encounter
    // problems in later experiments!
    panic( "call do_syscall to accomplish the syscall and lab1_1 here.\n" );
  
  }
  ```

- 根据提示，在此处调用do_syscall函数，并删除该处的panic即可。观察定义在syscall.c中的do_syscall函数：

  ```
  long do_syscall(long a0, long a1, long a2, long a3, long a4, long a5, long a6, long a7) {
    switch (a0) {
      case SYS_user_print:
        return sys_user_print((const char*)a1, a2);
      case SYS_user_exit:
        return sys_user_exit(a1);
      default:
        panic("Unknown syscall %ld \n", a0);
    }
  ```

- 观察可知，我们需要传入a0中的系统调用号来确定是何种系统调用，再至少传入sys_user_print函数需要的a1和a2，从存储上下文的regs中取出即可。

- 文档还说明do_syscall的返回值是要通知应用程序它发出的系统调用是否成功的，所以我们将返回值赋给regs中的a0，回到用户态时，a0中值变回0，即系统调用成功。所以handle_syscall具体实现为：

  ```
  static void handle_syscall(trapframe *tf) {
    // tf->epc points to the address that our computer will jump to after the trap handling.
    // for a syscall, we should return to the NEXT instruction after its handling.
    // in RV64G, each instruction occupies exactly 32 bits (i.e., 4 Bytes)
    tf->epc += 4;
  
    // TODO (lab1_1): remove the panic call below, and call do_syscall (defined in
    // kernel/syscall.c) to conduct real operations of the kernel side for a syscall.
    // IMPORTANT: return value should be returned to user app, or else, you will encounter
    // problems in later experiments!
    long t=do_syscall(tf->regs.a0,tf->regs.a1,tf->regs.a2,0,0,0,0,0);
    tf->regs.a0=t;
  }
  ```

  
