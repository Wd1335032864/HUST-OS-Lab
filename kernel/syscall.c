/*
 * contains the implementation of all syscalls.
 */

#include <stdint.h>
#include <errno.h>

#include "util/types.h"
#include "syscall.h"
#include "string.h"
#include "process.h"
#include "util/functions.h"
#include "elf.h"

#include "spike_interface/spike_utils.h"
extern elf_ctx elfloader;
//
// implement the SYS_user_print syscall
//
ssize_t sys_user_print(const char* buf, size_t n) {
  sprint(buf);
  return 0;
}

//
// implement the SYS_user_exit syscall
//
ssize_t sys_user_exit(uint64 code) {
  sprint("User exit with code:%d.\n", code);
  // in lab1, PKE considers only one app (one process). 
  // therefore, shutdown the system when the app calls exit()
  shutdown(code);
}

int backtrace_symbol(uint64 ad) {
  uint64 t = 0;
  int idx = -1;
  for (int i = 0; i < elfloader.syms_count; ++i) {
    if ((elfloader.syms[i].st_info == STT_FUNC && elfloader.syms[i].st_size != 514 && elfloader.syms[i].st_value < ad && elfloader.syms[i].st_value >t) ) {    //后续解释elfloader.syms[i].st_size != 514
	t = elfloader.syms[i].st_value; 
      idx = i;
    }
  }
  return idx;
}

ssize_t sys_user_backtrace(int64 depth) {
    uint64 user_sp = current->trapframe->regs.sp+24;
   int64 Depth=0;
  for (uint64 p = user_sp; actual_depth<depth; Depth, p += 16) {
   if (*(uint64*)p == 0) continue;
      int symbol_idx = backtrace_symbol(*(uint64*)p);
     if (symbol_idx == -1) {
        sprint("fail to backtrace symbol %lx\n", *(uint64*)p);
        continue;
      }
      sprint("%s\n", &elfloader.strtab[elfloader.syms[symbol_idx].st_name]);


//
// [a0]: the syscall number; [a1] ... [a7]: arguments to the syscalls.
// returns the code of success, (e.g., 0 means success, fail for otherwise)
//
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
}
