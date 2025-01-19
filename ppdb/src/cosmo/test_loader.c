#include "cosmopolitan.h"
#include "ape/loader.c"

__attribute__((__noreturn__)) void ApeLoaderCopy(long di, long *sp,
                                                      char dl) {
  int rc, n;
  unsigned i;
  const char *ape;
  int c, fd, os, argc;
  struct ApeLoader *M;
  char arg0, literally;
  unsigned long pagesz;
  union ElfEhdrBuf *ebuf;
  long *auxv, *ap, *endp, *sp2;
  char *p, *pe, *exe, *prog, **argv, **envp;

  (void)Printf;

  /* detect freebsd */
  if (SupportsXnu() && dl == XNU) {
    os = XNU;
  } else if (SupportsFreebsd() && di) {
    os = FREEBSD;
    sp = (long *)di;
  } else {
    os = 0;
  }
  printf("111\n");
  /* extract arguments */
  argc = *sp;
  argv = (char **)(sp + 1);
  envp = (char **)(sp + 1 + argc + 1);
  auxv = sp + 1 + argc + 1;
  for (;;) {
    if (!*auxv++) {
      break;
    }
  }

  printf("222\n");
  /* determine ape loader program name */
  ape = argv[0];
  if (!ape)
    ape = "ape";

  /* detect openbsd */
  if (SupportsOpenbsd() && !os && !auxv[0]) {
    os = OPENBSD;
  }

  /* xnu passes auxv as an array of strings */
  if (os == XNU) {
    *auxv = 0;
  }

  /* detect netbsd and find end of words */
  pagesz = 0;
  arg0 = 0;
  for (ap = auxv; ap[0]; ap += 2) {
    if (ap[0] == AT_PAGESZ) {
      pagesz = ap[1];
    } else if (SupportsNetbsd() && !os && ap[0] == AT_EXECFN_NETBSD) {
      os = NETBSD;
    } else if (SupportsLinux() && ap[0] == AT_FLAGS) {
      // TODO(mrdomino): maybe set/insert this when we are called as "ape -".
      arg0 = !!(ap[1] & AT_FLAGS_PRESERVE_ARGV0);
    }
  }
  if (!pagesz) {
    pagesz = 4096;
  }
  endp = ap + 1;

  /* the default operating system */
  if (!os) {
    os = LINUX;
  }

  printf("DEBUG os=%d\n",os);

  /* we can load via shell, shebang, or binfmt_misc */
  if (arg0) {
    literally = 1;
    prog = (char *)sp[2];
    argc = sp[2] = sp[0] - 2;
    argv = (char **)((sp += 2) + 1);
  } else if ((literally = argc >= 3 && !StrCmp(argv[1], "-"))) {
    /* if the first argument is a hyphen then we give the user the
       power to change argv[0] or omit it entirely. most operating
       systems don't permit the omission of argv[0] but we do, b/c
       it's specified by ANSI X3.159-1988. */
    prog = (char *)sp[3];
    argc = sp[3] = sp[0] - 3;
    argv = (char **)((sp += 3) + 1);
  } else if (argc < 2) {
    ShowUsage(os, 2, 1);
  } else {
    if (argv[1][0] == '-') {
      rc = !((argv[1][1] == 'h' && !argv[1][2]) ||
             !StrCmp(argv[1] + 1, "-help"));
      ShowUsage(os, 1 + rc, rc);
    }
    prog = (char *)sp[2];
    argc = sp[1] = sp[0] - 1;
    argv = (char **)((sp += 1) + 1);
  }

  /* allocate loader memory in program's arg block */
  n = sizeof(struct ApeLoader);
  M = (struct ApeLoader *)__builtin_alloca(n);
  M->ps.literally = literally;

  /* create new bottom of stack for spawned program
     system v abi aligns this on a 16-byte boundary
     grows down the alloc by poking the guard pages */
  n = (endp - sp + 1) * sizeof(long);
  sp2 = (long *)__builtin_alloca(n);
  if ((long)sp2 & 15)
    ++sp2;
  for (; n > 0; n -= pagesz) {
    ((char *)sp2)[n - 1] = 0;
  }
  MemMove(sp2, sp, (endp - sp) * sizeof(long));
  argv = (char **)(sp2 + 1);
  envp = (char **)(sp2 + 1 + argc + 1);
  auxv = sp2 + (auxv - sp);
  sp = sp2;

  /* allocate ephemeral memory for reading file */
  n = sizeof(union ElfEhdrBuf);
  ebuf = (union ElfEhdrBuf *)__builtin_alloca(n);
  for (; n > 0; n -= pagesz) {
    ((char *)ebuf)[n - 1] = 0;
  }

  printf("333,os=%d,di=%d,WINDOWS=%d\n",os,di,WINDOWS);
  //os=LINUX;//TMP TEST
  printf("DEBUG: prog=%s\n", prog);
  exe = Commandv(&M->ps, os, prog, GetEnv(envp, "PATH"));
  printf("DEBUG: exe=%s\n", exe ? exe : "NULL");
  /* resolve path of executable and read its first page */
  if (!exe) {
	  printf("DEBUG: Pexit args: os=%d, prog=%s, code=%d, msg=%s\n", 
			  os, prog, 0, "not found (maybe chmod +x or ./ needed)");
	  Pexit(os, prog, 0, "not found (maybe chmod +x or ./ needed)");
	  printf("336\n");
  } else if ((fd = Open(exe, O_RDONLY, 0, os)) < 0) {
    Pexit(os, exe, fd, "open");
  } else if ((rc = Pread(fd, ebuf->buf, sizeof(ebuf->buf), 0, os)) < 0) {
    Pexit(os, exe, rc, "read");
  } else if ((unsigned long)rc < sizeof(ebuf->ehdr)) {
    Pexit(os, exe, 0, "too small");
  }
  printf("340\n");
  pe = ebuf->buf + rc;

  /* ape intended behavior
     1. if ape, will scan shell script for elf printf statements
     2. shell script may have multiple lines producing elf headers
     3. all elf printf lines must exist in the first 8192 bytes of file
     4. elf program headers may appear anywhere in the binary */
  if (READ64(ebuf->buf) == READ64("MZqFpD='") ||
      READ64(ebuf->buf) == READ64("jartsr='") ||
      READ64(ebuf->buf) == READ64("APEDBG='")) {
    for (p = ebuf->buf; p < pe; ++p) {
      if (READ64(p) != READ64("printf '")) {
        continue;
      }
      for (i = 0, p += 8; p + 3 < pe && (c = *p++) != '\'';) {
        if (c == '\\') {
          if ('0' <= *p && *p <= '7') {
            c = *p++ - '0';
            if ('0' <= *p && *p <= '7') {
              c *= 8;
              c += *p++ - '0';
              if ('0' <= *p && *p <= '7') {
                c *= 8;
                c += *p++ - '0';
              }
            }
          }
        }
        ebuf->buf[i++] = c;
        if (i >= sizeof(ebuf->buf)) {
          break;
        }
      }
      if (i >= sizeof(ebuf->ehdr)) {
	      printf("444=%d\n",i);
        TryElf(M, ebuf, exe, fd, sp, auxv, pagesz, os);
      }
    }
  }
	      printf("555\n");
  Pexit(os, exe, 0, TryElf(M, ebuf, exe, fd, sp, auxv, pagesz, os));
}

int load_and_run_ape(const char* filename) {
    if (IsWindows()) {
        // Open file using cross-platform API
        int fd = Open(filename, O_RDONLY, 0, WINDOWS);
        if (fd < 0) {
            printf("Failed to open file %s\n",filename);
            return 1;
        }

        // Get file size
        struct stat st;
        if (fstat(fd, &st) < 0) {
            Close(fd, WINDOWS);
            return 1;
        }

        // Map file into memory
        long map_result = Mmap(NULL, st.st_size, PROT_READ | PROT_EXEC,
                         MAP_PRIVATE, fd, 0, 0);
        if (map_result == -1) {
            Close(fd, WINDOWS);
            return 1;
        }
        void* base = (void*)map_result;

        // Get PE header offset and entry point
        uint32_t peOffset = READ32((uint8_t*)base + 0x3c);
        uint32_t entryPoint = READ32((uint8_t*)base + peOffset + 0x28);
        void* entry = (uint8_t*)base + entryPoint;

        // Call entry point
        typedef int (*EntryPoint)(void*, void*, char*, int);
        EntryPoint WinMain = (EntryPoint)entry;
        int result = WinMain(base, NULL, "", 0);

        // Cleanup
        munmap(base, st.st_size);
        Close(fd, WINDOWS);
        return result;
    } else {
        // Unix loading via ApeLoaderCopy
        long args_with_count[] = {
            2,                    
            (long)"test_target.exe",         
            (long)"test_target.exe", 
            0,                   
            0                    
        };
        ApeLoaderCopy(0, args_with_count, 0);
        return 0;
    }
}

int main(int argc, char** argv) {
    if (argc != 2) {
        printf("Usage: %s <target>\n", argv[0]);
        return 1;
    }
    return load_and_run_ape(argv[1]);
}
