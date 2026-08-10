/*
 * Runtime for P6's interactive built-ins: key(ms), clock and random(n).
 *
 * The compiler lowers each built-in to a call to one of the functions below, converting between
 * P6's balanced-ternary integers and the plain 32-bit integers used here. They are linked next to
 * the CDK runtime (librts), which has no libc, so everything is raw i386 Linux system calls through
 * `int 0x80`.
 *
 * Build: make -C runtime   (gives libp6.a, and libp6-det.a for reproducible test runs)
 */

#define SYS_READ 3
#define SYS_IOCTL 54
#define SYS_POLL 168
#define SYS_CLOCK_GETTIME 265

#define TCGETS 0x5401
#define TCSETS 0x5402
#define ISIG 0x0001
#define ICANON 0x0002
#define ECHO 0x0008
#define VTIME 5
#define VMIN 6

/* What key() returns for keys that are not a single character, and when input has ended. */
#define P6_KEY_NONE 0
#define P6_KEY_UP (-1)
#define P6_KEY_DOWN (-2)
#define P6_KEY_RIGHT (-3)
#define P6_KEY_LEFT (-4)
#define P6_KEY_END 4 /* Ctrl-D: stdin closed */

struct kernel_termios {
  unsigned int c_iflag, c_oflag, c_cflag, c_lflag;
  unsigned char c_line;
  unsigned char c_cc[19];
};

struct pollfd {
  int fd;
  short events, revents;
};

struct timespec32 {
  int tv_sec, tv_nsec;
};

static int syscall3(int n, int a, int b, int c) {
  int r;
  __asm__ volatile("int $0x80" : "=a"(r) : "a"(n), "b"(a), "c"(b), "d"(c) : "memory");
  return r;
}

//---------------------------------------------------------------------------
// The terminal

static struct kernel_termios saved;
static int raw_on;

/* Turns off line buffering, echo and Ctrl-C, so every key reaches the program as it is pressed. */
static void raw(void) {
  struct kernel_termios t;
  if (raw_on) return;
  raw_on = -1; /* tried; stays -1 when stdin is not a terminal */
  if (syscall3(SYS_IOCTL, 0, TCGETS, (int)&saved) < 0) return;
  t = saved;
  t.c_lflag &= ~(ICANON | ECHO | ISIG);
  t.c_cc[VMIN] = 1;
  t.c_cc[VTIME] = 0;
  syscall3(SYS_IOCTL, 0, TCSETS, (int)&t);
  raw_on = 1;
}

/* Puts the terminal back. The compiler calls this wherever a program that uses key() ends. */
void p6_restore(void) {
  if (raw_on == 1) syscall3(SYS_IOCTL, 0, TCSETS, (int)&saved);
  raw_on = 0;
}

//---------------------------------------------------------------------------
// key(ms)

static unsigned char buf[64];
static int pos, len;

/* The next key in the buffer: an arrow's escape sequence becomes one negative code. */
static int decode(void) {
  int c = buf[pos++];
  if (c == 27 && pos + 1 < len && (buf[pos] == '[' || buf[pos] == 'O')) {
    int k = buf[pos + 1];
    if (k >= 'A' && k <= 'D') {
      pos += 2;
      return k == 'A' ? P6_KEY_UP : k == 'B' ? P6_KEY_DOWN : k == 'C' ? P6_KEY_RIGHT : P6_KEY_LEFT;
    }
  }
  return c;
}

/* Waits up to ms milliseconds (forever when ms < 0) for a key and returns its code, or 0. */
int p6_key(int ms) {
  raw();
#ifdef DETERMINISTIC
  /* Test build: one scripted byte per call, '.' meaning no key, so runs are reproducible. */
  (void)ms;
  if (syscall3(SYS_READ, 0, (int)buf, 1) <= 0) return P6_KEY_END;
  pos = 0;
  len = 1;
  return buf[0] == '.' ? P6_KEY_NONE : decode();
#endif
  if (pos >= len) {
    struct pollfd p = {0, 1 /* POLLIN */, 0};
    pos = len = 0;
    if (syscall3(SYS_POLL, (int)&p, 1, ms) <= 0) return P6_KEY_NONE;
    len = syscall3(SYS_READ, 0, (int)buf, sizeof buf);
    if (len <= 0) {
      len = 0;
      return P6_KEY_END;
    }
  }
  return decode();
}

//---------------------------------------------------------------------------
// clock

static int base_sec = -1;

/* Milliseconds since the first call. Counting from boot would pass 2^31 ms after about 25 days. */
int p6_clock(void) {
#ifdef DETERMINISTIC
  static int fake;
  return fake += 7;
#endif
  struct timespec32 ts;
  syscall3(SYS_CLOCK_GETTIME, 1 /* CLOCK_MONOTONIC */, (int)&ts, 0);
  if (base_sec < 0) base_sec = ts.tv_sec;
  return (ts.tv_sec - base_sec) * 1000 + ts.tv_nsec / 1000000;
}

//---------------------------------------------------------------------------
// random(n)

static unsigned int seed;

/* A random integer in [0, n), from xorshift32 seeded by the clock (0 when n <= 0). */
int p6_random(int n) {
#ifdef DETERMINISTIC
  if (seed == 0) seed = 12345;
#endif
  if (seed == 0) {
    struct timespec32 ts;
    syscall3(SYS_CLOCK_GETTIME, 1, (int)&ts, 0);
    seed = (unsigned int)(ts.tv_nsec ^ (ts.tv_sec << 16)) | 1;
  }
  seed ^= seed << 13;
  seed ^= seed >> 17;
  seed ^= seed << 5;
  return n > 0 ? (int)(seed % (unsigned int)n) : 0;
}
