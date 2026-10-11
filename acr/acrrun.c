// acrrun - headless runner for ACR-7000 paper tape images.
//
// Links against the unmodified ACR-7000 emulator core (../acr7000/cpu.c,
// compiled with its interactive monitor's main() renamed) and replaces the
// monitor with a batch driver:
//
//   acrrun [-e <octal entry>] [-m <words>] [-q] <tape>
//
// The tape is read in the RIM format produced by asm2 and deposited
// straight into memory (equivalent to running the relocatable loader).
// Execution starts at the entry address (default 02000 = 1024, where
// chibicc's crt0 lives) at IRQL 0, supervisor key, and runs until the
// program halts. The halt code becomes the process exit status.
//
// Devices:
//   013  line printer stand-in: every character written with "wios r, 0"
//        goes straight to stdout (no line buffering, never busy)
//   012  paper tape reader stand-in reading from stdin: "nios" requests a
//        character, "tiond" reports done once it has arrived, "rio r, 0"
//        returns it. At end of input the reader returns 0777 with done set.

#include <errno.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "cpu.h"

void init_cpu(acr7k_cu_t *cpu, uint64_t mem_size, int max_io);
void wait_for_cpu(acr7k_cu_t *cpu);
void destroy_cpu(acr7k_cu_t *cpu);

// The monitor's main() references these devices; acrrun never calls them.
void init_panel(acr7k_cu_t *cpu, int id) {}
void init_bishop(acr7k_cu_t *cpu, int id) {}
void init_ppt_ex(acr7k_cu_t *cpu, int id, int irq, char *fname) {}
void init_lpt(acr7k_cu_t *cpu, int id, int irq, FILE *fd) {}
void init_tty(acr7k_cu_t *cpu, int id, int irq, int port) {}
void init_msch(acr7k_cu_t *cpu, int id, int irq, int model) {}
void init_7310(acr7k_cu_t *cpu, int id, int sc_id) {}
void start_render(render_loop_ctx_t *ctx) {}
void kill_render(render_loop_ctx_t *ctx) {}

static uint64_t printer_io(void *ctx, uint64_t data, int ctl, int transfer) {
  if (transfer == 1 && ctl == 1)
    putchar((int)(data & 0xFF));
  if (transfer == 14)
    return 2; // done, never busy
  return 0;
}

static int reader_ch = -1;
static int reader_done;

static uint64_t reader_io(void *ctx, uint64_t data, int ctl, int transfer) {
  if (transfer != 14 && ctl == 1) {
    fflush(stdout);
    int c = getchar();
    reader_ch = (c == EOF) ? 0777 : c;
    reader_done = 1;
  }
  if (transfer != 14 && ctl == 2)
    reader_done = 0;
  if (transfer == 14)
    return reader_done << 1;
  if (transfer == 0)
    return reader_ch & 0777;
  return 0;
}

static void usage(void) {
  fprintf(stderr, "usage: acrrun [-e <octal entry>] [-m <words>] [-q] <tape>\n");
  exit(2);
}

// RIM format: per block, a three-character (18-bit) origin, then six
// characters (six bits each, high first) per 36-bit word, then a 0200
// terminator.
static void load_tape(acr7k_cu_t *cpu, char *path) {
  FILE *fp = fopen(path, "rb");
  if (!fp) {
    fprintf(stderr, "acrrun: %s: %s\n", path, strerror(errno));
    exit(2);
  }

  for (;;) {
    int c = fgetc(fp);
    if (c == EOF)
      break;

    uint64_t addr = c & 077;
    for (int i = 0; i < 2; i++) {
      c = fgetc(fp);
      if (c == EOF || c == 0200)
        goto bad;
      addr = (addr << 6) | (c & 077);
    }

    for (;;) {
      c = fgetc(fp);
      if (c == 0200)
        break;
      if (c == EOF)
        goto bad;

      uint64_t word = c & 077;
      for (int i = 0; i < 5; i++) {
        c = fgetc(fp);
        if (c == EOF || c == 0200)
          goto bad;
        word = (word << 6) | (c & 077);
      }

      if (addr >= cpu->mem_size) {
        fprintf(stderr, "acrrun: tape loads beyond memory (%lo)\n", addr);
        exit(2);
      }
      cpu->memory[addr++] = word;
    }
  }
  fclose(fp);
  return;

bad:
  fprintf(stderr, "acrrun: %s: malformed tape\n", path);
  exit(2);
}

int main(int argc, char **argv) {
  uint64_t entry = 02000;
  uint64_t mem_size = 262144;
  int quiet = 0;
  int opt;

  while ((opt = getopt(argc, argv, "e:m:q")) != -1) {
    switch (opt) {
    case 'e': entry = strtoull(optarg, NULL, 8); break;
    case 'm': mem_size = strtoull(optarg, NULL, 0); break;
    case 'q': quiet = 1; break;
    default: usage();
    }
  }
  if (optind != argc - 1)
    usage();

  // The emulator core chats on stderr; keep it quiet unless asked.
  int saved_stderr = dup(2);
  if (quiet)
    freopen("/dev/null", "w", stderr);

  acr7k_cu_t cpu;
  init_cpu(&cpu, mem_size, 512);

  cpu.io[013] = printer_io;
  cpu.io[012] = reader_io;

  load_tape(&cpu, argv[optind]);

  cpu.c[C_PSW] = entry & MASK_ADDR;
  cpu.c[C_CW] = 0;
  start_cpu(&cpu, 0);
  wait_for_cpu(&cpu);
  fflush(stdout);

  uint64_t code = cpu.stop_code;
  destroy_cpu(&cpu);

  if (quiet) {
    fflush(stderr);
    dup2(saved_stderr, 2);
  }
  return (int)(code & 0xFF);
}
