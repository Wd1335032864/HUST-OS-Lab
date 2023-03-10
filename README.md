# lab1_challenge2_errorline
**文档过程说的比较详细，首先我们找到debug_line段，将其内容保存，然后利用make_addr_line函数去解析，由于该函数已经实现，我们调用即可。调用结束后，process结构体的dir、file、line三个指针会各指向一个数组。读取相关数组内容并打印内容即可。**

## 读取debug_line段中调试信息

```
  elf_status elf_load(elf_ctx *ctx) {
  elf_prog_header ph_addr;
  int i, off; 
  for (i = 0, off = ctx->ehdr.phoff; i < ctx->ehdr.phnum; i++, off += sizeof(ph_addr)) {
    // read segment headers
    if (elf_fpread(ctx, (void *)&ph_addr, sizeof(ph_addr), off) != sizeof(ph_addr)) return EL_EIO;

    if (ph_addr.type != ELF_PROG_LOAD) continue;
    if (ph_addr.memsz < ph_addr.filesz) return EL_ERR;
    if (ph_addr.vaddr + ph_addr.memsz < ph_addr.vaddr) return EL_ERR;

    // allocate memory before loading
    void *dest = elf_alloc_mb(ctx, ph_addr.vaddr, ph_addr.vaddr, ph_addr.memsz);

    // actual loading
    if (elf_fpread(ctx, dest, ph_addr.memsz, ph_addr.off) != ph_addr.memsz)
      return EL_EIO;
  }
	uint64 debug_line;
	debug_line = ph_addr.vaddr + ph_addr.memsz;	
  char name[9999];
  elf_sect_header tmp_seg,name_seg;

  if (elf_fpread(ctx, (void *)&name_seg, sizeof(name_seg),
              ctx->ehdr.shoff + ctx->ehdr.shstrndx * sizeof(name_seg)) != sizeof(name_seg)) return EL_EIO;
  if (elf_fpread(ctx,(void *)name,name_seg.size,name_seg.offset) != name_seg.size) return EL_EIO;

  for (i = 0, off = ctx->ehdr.shoff; i < ctx->ehdr.shnum; i++, off += sizeof(tmp_seg)) {
      if (elf_fpread(ctx, (void *)&tmp_seg, sizeof(tmp_seg), off) != sizeof(tmp_seg)) return EL_EIO;
      if (strcmp(&name[tmp_seg.name], ".debug_line") == 0) {
          if (elf_fpread(ctx, (void *)debug_line, tmp_seg.size, tmp_seg.offset) != tmp_seg.size) return EL_EIO;
          make_addr_line(ctx, (char *)debug_line, tmp_seg.size); break;
      }
  }
  return EL_OK;
}
```

- 之所以在elf_load中实现是为了把debug_line段直接放在程序所有需映射的段数据之后，保证有足够的动态空间来存放dir、file、line三个数组。因为elf_load要装载segment，所以我们直接取用循环后的ph_addr.vaddr+ph_addr.memsz即可。也可以利用section来获取需映射的段数据的最大虚拟地址，但这样后续读取section就会多几次循环。如此就能找到debug_line就break

- 与找symtab和strtab不同的是找debug_line不能通过type来找，观察section header就知道有=还存在许多type为PROGBITS的section，所以这里是采用name的对比来找debug_line

- make_addr_line函数调用结束后，process结构体的dir、file、line三个指针会各指向一个数组，我们观察process的结构体：

  ```
  typedef struct process_t {
    // pointing to the stack used in trap handling.
    uint64 kstack;
    // trapframe storing the context of a (User mode) process.
    trapframe* trapframe;
  
    char *debugline; char **dir; code_file *file; addr_line *line; int line_ind;
  }process;
  ```

- 观察make_addr_line可知，debugline存放debug_line的地址，和line_ind存放代码文件的代码总行数

- 
  dir数组存储所有代码文件的文件夹路径字符串指针

- file数组存储所有代码文件的文件名字符串指针以及其文件夹路径在dir数组中的索引：

  ```
  typedef struct {
      uint64 dir; char *file;
  } code_file;
  ```

- line数组存储所有指令地址，代码行号，文件名在file数组中的索引三者的映射关系

  ```
  typedef struct {
      uint64 addr, line, file;
  } addr_line;
  ```

  

## 中断处理

- 通过对bug_line的解析，我们现在已经有了所有代码文件的文件夹路径和文件名、指令地址和代码行的映射关系。接下来我们还需要找到引出异常的那条指令的地址。通过文档可知，在M态下，mepc寄存器存放着发生异常的那条指令的地址。我们可以通过映射关系找到其具体路径然后进行打印：

  ```
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
  ```

  

## 中断路径完善

- 由于最终测试时内核也应能够对其他会导致panic的异常和其他源文件输出正确的结果，所以在M态所有需要打印异常代码行的case下调用error_print函数：

  ```
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
  ```

  
