#include "kernel/riscv.h"
#include "kernel/process.h"
#include "spike_interface/spike_utils.h"

static void handle_instruction_access_fault() { panic("Instruction access fault!"); }

static void handle_load_access_fault() { panic("Load access fault!"); }

static void handle_store_access_fault() { panic("Store/AMO access fault!"); }

static void handle_illegal_instruction() { panic("Illegal instruction!"); }

static void handle_misaligned_load() { panic("Misaligned Load!"); }

static void handle_misaligned_store() { panic("Misaligned AMO!"); }

// added @lab1_3
static void handle_timer() {
  int cpuid = 0;
  // setup the timer fired at next time (TIMER_INTERVAL from now)
  *(uint64*)CLINT_MTIMECMP(cpuid) = *(uint64*)CLINT_MTIMECMP(cpuid) + TIMER_INTERVAL;

  // setup a soft interrupt in sip (S-mode Interrupt Pending) to be handled in S-mode
  write_csr(sip, SIP_SSIP);
}
	char path[200], code[10000];
void error_print() {
      uint64 mepc = read_csr(mepc);
      for (int i = 0; i < current->line_ind; i++) {
          if (mepc < current->line[i].addr) { //一行代码可能对应多条指令
    addr_line *line = &current->line[i-1];
    int l = strlen(current->dir[current->file[line->file].dir]); 
    strcpy(path, current->dir[current->file[line->file].dir]);  //利用索引寻找
    path[l] = '/';
    strcpy(path + l + 1, current->file[line->file].file);
    
    spike_file_t *f = spike_file_open(path, O_RDONLY, 0);
    spike_file_read(f, code, 10000);
    spike_file_close(f); int off = 0, cnt = 1;
    while (off < 10000) {
        int x = off; while (x < 10000 && code[x] != '\n') x++;
        if (cnt == line->line) {
            code[x] = '\0';
            sprint("Runtime error at %s:%d\n%s\n", path, line->line, code+off);
            break;
        } 
	else { cnt++; off = x + 1;}
    }
			 break;
          }
      }
}
//
// handle_mtrap calls a handling function according to the type of a machine mode interrupt (trap).
//
void handle_mtrap() {
  uint64 mcause = read_csr(mcause);
  switch (mcause) {
    case CAUSE_MTIMER:
      handle_timer();
      break;
    case CAUSE_FETCH_ACCESS:
      error_print();
      handle_instruction_access_fault();
      break;
    case CAUSE_LOAD_ACCESS:
      error_print();
      handle_load_access_fault();
    case CAUSE_STORE_ACCESS:
      error_print();
      handle_store_access_fault();
      break;
    case CAUSE_ILLEGAL_INSTRUCTION:
      // TODO (lab1_2): call handle_illegal_instruction to implement illegal instruction
      // interception, and finish lab1_2.
      error_print();
      handle_illegal_instruction();

      break;
    case CAUSE_MISALIGNED_LOAD:
      error_print();
      handle_misaligned_load();
      break;
    case CAUSE_MISALIGNED_STORE:
      error_print();
      handle_misaligned_store();
      break;

    default:
      sprint("machine trap(): unexpected mscause %p\n", mcause);
      sprint("            mepc=%p mtval=%p\n", read_csr(mepc), read_csr(mtval));
      panic( "unexpected exception happened in M-mode.\n" );
      break;
  }
}
