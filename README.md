# lab1_challenge1_backtrace

**首先明确我们需要做什么，由于没有此系统调用，所以我们肯定要添加系统调用，并完善相关路径。其次在调用函数中，我们需要获取用户程序的栈，才能追溯到函数的返回地址，要注意的是，系统调用时会切换到S模式的用户内核栈，但我们需要在用户栈上寻找。最后找到返回地址后，需要将虚拟地址转换成源程序中的符号，由文档可知该内容与ELF文件的symtab section和strtab seciton有关。**



## 添加系统调用

- 首先在user_lib.h文件中添加print_backtrace()函数原型，完成函数声明

  ```
  int printu(const char *s, ...);
  int exit(int code);
  //added
  int print_backtrace(int depth);
  ```

- 然后在user_lib.c文件中完成该函数的实现，仿照printu和exit，此处转化为对do_user_call()的调用即可

  ```
   int print_backtrace(int depth) {
      return do_user_call(SYS_user_backtrace, depth, 0, 0, 0, 0, 0, 0);
    }
  ```

- 因为需要告诉do_user_call系统调用号，所以需要添加宏，找到syscall.h往里添加：

  ```
  #define SYS_user_backtrace (SYS_user_base + 2)  //与前两个不同即可
  ```

- 然后就是do_user_call中ecall指令使机器转到trap处理入口smode_trap_vecotr，再调用smode_trap_handler()函数，判断为系统调用后，转而调用handle_syscall()函数，进一步调用do_syscall()函数，其实调用过程与lab1_1_syscall一致，所以我们在do_syscall中添加sys_user_backtrace()函数到switch分支即可：

  ```
  long do_syscall(long a0, long a1, long a2, long a3, long a4, long a5, long a6, long a7) {
    switch (a0) {
      case SYS_user_print:
        return sys_user_print((const char*)a1, a2);
      case SYS_user_exit:
        return sys_user_exit(a1);
      case SYS_user_backtrace:
        return sys_user_backtrace(a1);  //a1就是depth
      default:
        panic("Unknown syscall %ld \n", a0);
    }
  ```

- 到此系统调用添加完成，接来下要完善系统调用具体内容

  

## 获取用户栈和函数返回地址

- f1到f8和print_backtrace的被调用时系统处于用户态，所以他们会在用户栈上操作。print_backtrace调用的do_user_call()会通过ecall指令使系统陷入S态，所以此时转到用户内核栈上操作。我们可以通过`riscv64-unknown-elf-objdump -d obj/app_print_backtrace`来查看该程序的汇编代码，可以发现f1到f8函数代码基本一致：

  ```
  000000008100008e <f1>:
      8100008e:	1141                	addi	sp,sp,-16
      81000090:	e406                	sd	ra,8(sp)
      81000092:	e022                	sd	s0,0(sp)
      81000094:	0800                	addi	s0,sp,16
      81000096:	fe5ff0ef          	jal	ra,8100007a <f2>
      8100009a:	60a2                	ld	ra,8(sp)
      8100009c:	6402                	ld	s0,0(sp)
      8100009e:	0141                	addi	sp,sp,16
      810000a0:	8082                	ret
  ```

- f1到f8在栈中所占都是16字节，高地8字节存ra，即该函数的返回地址，低字节存s0，指向调用该函数的函数返回地址。print_backtrace同样占16字节。特别注意用户态函数do_user_call，他开辟了32字节：

  ```
  00000000810000ca <do_user_call>:
      810000ca:	1101                	addi	sp,sp,-32
      810000cc:	ec22                	sd	s0,24(sp)
      810000ce:	1000                	addi	s0,sp,32
      810000d0:	00000073          	ecall
  ```

- 随后调用ecall进入s态

- 所以我们首先令sp+24指向第一个fp，然后循环+16即可得到之前所有函数的返回地址，**注意这个返回地址是调用该函数的指令的下一条指令的地址，而我们后面需要解析的是函数的首地址**

  ```
   uint64 user_sp = current->trapframe->regs.sp+24;
     int64 actual_depth=0;
    for (uint64 p = user_sp; actual_depth<depth; ++actual_depth, p += 16)
  \
  将虚拟地址转化为源符号部分
  \
  ```

  

## symtab section and strtab section 处理
