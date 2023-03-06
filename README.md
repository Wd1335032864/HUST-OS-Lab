# lab1_2_exception

- 首先继承lab1_1_syscall的答案

  ```
  git merge lab1_1_syscall -m "continue to work on lab1_2"
  ```

- 观察app_illegal_instruction.c：

  ```
    #include "user_lib.h"
     #include "util/types.h"
    
     int main(void) {
     printu("Going to hack the system by running privilege instructions.\n");
      // we are now in U(user)-mode, but the "csrw" instruction requires M-mode privilege.
      // Attempting to execute such instruction will raise illegal instruction exception.
     asm volatile("csrw sscratch, 0");
     exit(0);
  
  ```

- `csrw sscratch,0 `试图在U模式下修改S模式的栈指针，造成异常。查找RISC-V体系结构的相关文档，我们知道，这类异常属于非法指令异常，即CAUSE_ILLEGAL_INSTRUCTION，它对应的异常码是02（见kernel/riscv.h中的定义），lab1_1_syscall中的系统调用是交给S模式处理，而这里我们需要判断该异常是否交给S模式处理，还是M模式

- 由文档可知，在kernel/machine/minit.c文件中，delegate_traps()函数确实将部分异常代理给了S模式处理，但是里面并没有我们关心的CAUSE_ILLEGAL_INSTRUCTION异常，这说明该异常的处理还是交给M模式来处理。所以，我们需要了解M模式的trap处理入口，以便继续跟踪其后的处理过程。由文档，M模式的trap处理入口在kernel/machine/mtrap_vector.S文件中，而PKE操作系统在系统内核启动时已经将M模式的中断处理入口指向了该函数：

  ```
  mtrapvec:
      # mscratch -> g_itrframe (cf. kernel/machine/minit.c line 94)
      # swap a0 and mscratch, so that a0 points to interrupt frame,
      # i.e., [a0] = &g_itrframe
      csrrw a0, mscratch, a0
  
      # save the registers in g_itrframe
      addi t6, a0, 0
      store_all_registers
      # save the original content of a0 in g_itrframe
      csrr t0, mscratch
      sd t0, 72(a0)
  
      # switch stack (to use stack0) for the rest of machine mode
      # trap handling.
      la sp, stack0
      li a3, 4096
      csrr a4, mhartid
      addi a4, a4, 1
      mul a3, a3, a4
      add sp, sp, a3
  
      # pointing mscratch back to g_itrframe
      csrw mscratch, a0
  
      # call machine mode trap handling function
      call handle_mtrap
  
      # restore all registers, come back to the status before entering
      # machine mode handling.
      csrr t6, mscratch
      restore_all_registers
  
      mret
  ```

- 前半部分与系统调用一样，保存用户态进程上下文和对应的异常码。不同的是这里是用M模式下的寄存器mscratch，并且它指向的是riscv_regs类型的g_itrframe，而不是trapframe结构体了。

- 其实后半部分也是一样，这里是切换到stack0，即PEK内核启动时用过的栈，而lab1_1是切换到用户内核栈，后面同样是调用处理函数，最后回到用户态。这里就是调用handle_mtrap函数：

  ```
  void handle_mtrap() {
    uint64 mcause = read_csr(mcause);
    switch (mcause) {
      case CAUSE_FETCH_ACCESS:
        handle_instruction_access_fault();
        break;
      case CAUSE_LOAD_ACCESS:
        handle_load_access_fault();
      case CAUSE_STORE_ACCESS:
        handle_store_access_fault();
        break;
      case CAUSE_ILLEGAL_INSTRUCTION:
        // TODO (lab1_2): call handle_illegal_instruction to implement illegal instruction
        // interception, and finish lab1_2.
  	  panic( "call handle_illegal_instruction to accomplish illegal instruction interception for lab1_2.\n" );
        
        break;
      case CAUSE_MISALIGNED_LOAD:
        handle_misaligned_load();
        break;
      case CAUSE_MISALIGNED_STORE:
        handle_misaligned_store();
        break;
  
      default:
        sprint("machine trap(): unexpected mscause %p\n", mcause);
        sprint("            mepc=%p mtval=%p\n", read_csr(mepc), read_csr(mtval));
        panic( "unexpected exception happened in M-mode.\n" );
        break;
    }
  }
  ```

- 可见，handle_mtrap函数对在M态的多项异常都进行了处理，而对于CAUSE_ILLEGAL_INSTRUCTION的处理需要我们自己添加处理函数并删除panic：

  ```
  case CAUSE_ILLEGAL_INSTRUCTION:
  handle_illegal_instruction();
  ```

- 而其函数具体定义也已经在mtrap.c中实现好，所以到此实验完成。
