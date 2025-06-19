#define _GNU_SOURCE
#include <fcntl.h>
#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/sendfile.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <time.h>
#include <unistd.h>

struct timespec start, end;

volatile sig_atomic_t run = 1;

void skill(int sig) { run = 0; }

enum tests {
  s_chunked_cpy = 0,
  send_cpy,
  send_nocache_cpy,
  s_chunked_nocache_cpy,
  s_chunked_fsync_cpy,
  s_chunked_stat_cpy
};

int S_CHUNKED_CPY = 1 << s_chunked_cpy;
int SEND_CPY = 1 << send_cpy;
int SEND_NOCACHE_CPY = 1 << send_nocache_cpy;
int S_CHUNKED_NOCACHE_CPY = 1 << s_chunked_nocache_cpy;
int S_CHUNKED_FSYNC_CPY = 1 << s_chunked_fsync_cpy;
int S_CHUNKED_STAT_CPY = 1 << s_chunked_stat_cpy;

void parse_tests(char tests[], int *flags) {
  *flags = 0;
  char *token = NULL;
  token = strtok(tests, ",");
  if (token == NULL) return;
  do {
    if (strcmp(token, "copy") == 0)
      *flags |= S_CHUNKED_CPY;
    if (strcmp(token, "send") == 0)
      *flags |= SEND_CPY;

  } while (token = strtok(NULL, ","));
}

double copy_file_chunk(int r_file, int w_file, int page_size) {
  size_t ret;
  long long elapsed;
  double elapsed_sec;
  void *buffer;

  if (posix_memalign(&buffer, page_size, page_size)) {
    perror("posix_memalign");
    return 1;
  }

  clock_gettime(CLOCK_MONOTONIC, &start);
  while ((ret = read(r_file, buffer, page_size)) > 0) {
    write(w_file, buffer, ret);
  }
  clock_gettime(CLOCK_MONOTONIC, &end);
  elapsed = (end.tv_sec - start.tv_sec) * 1000000000LL +
            (end.tv_nsec - start.tv_nsec);
  free(buffer);
  return elapsed;
}

double copy_file_sendfile(int r_file, int w_file, off_t size) {
  long long elapsed;
  double elapsed_sec;

  clock_gettime(CLOCK_MONOTONIC, &start);
  sendfile(w_file, r_file, NULL, size);
  clock_gettime(CLOCK_MONOTONIC, &end);
  elapsed = (end.tv_sec - start.tv_sec) * 1000000000LL +
            (end.tv_nsec - start.tv_nsec);
  return elapsed;
}

double copy_ap_esync(int r_file, int w_file, int page_size) {
  size_t ret;
  long long elapsed;
  double elapsed_sec;
  void *buffer;

  if (posix_memalign(&buffer, page_size, page_size)) {
    perror("posix_memalign");
    return 1;
  }

  clock_gettime(CLOCK_MONOTONIC, &start);
  while ((ret = read(r_file, buffer, page_size)) > 0) {
    write(w_file, buffer, ret);
    fsync(w_file);
  }
  clock_gettime(CLOCK_MONOTONIC, &end);
  elapsed = (end.tv_sec - start.tv_sec) * 1000000000LL +
            (end.tv_nsec - start.tv_nsec);
  free(buffer);
  return elapsed;
}

double copy_ap_estat(int r_file, int w_file, int page_size, char *w_file_name) {
  size_t ret;
  long long elapsed;
  double elapsed_sec;
  void *buffer;
  struct stat file_stat;

  if (posix_memalign(&buffer, page_size, page_size)) {
    perror("posix_memalign");
    return 1;
  }

  clock_gettime(CLOCK_MONOTONIC, &start);
  while ((ret = read(r_file, buffer, page_size)) > 0) {

    if (stat(w_file_name, &file_stat) != 0) {
      perror("stat");
      return -1;
    }
    write(w_file, buffer, ret);
  }
  clock_gettime(CLOCK_MONOTONIC, &end);
  elapsed = (end.tv_sec - start.tv_sec) * 1000000000LL +
            (end.tv_nsec - start.tv_nsec);
  free(buffer);
  return elapsed;
}

void print_stats(off_t size, long long elapsed) {
  double elapsed_sec = elapsed / 1000000000.0;
  long long total_bytes = (long long)(size);
  double bandwidth = total_bytes / elapsed_sec;
  printf("\n\n\nelapsed: %ld s", elapsed);
  printf("\nbw: %lf KiB/s", bandwidth / 1024);
  printf("\nbw: %lf MiB/s", bandwidth / (1024 * 1024));
  printf("\nbw: %lf GiB/s\n", bandwidth / (1024 * 1024 * 1024));
}

int check_disk_space(const char *path, off_t size) {
  struct statvfs stat;
  if (statvfs(path, &stat) != 0) {
    perror("statvfs");
    return -1;
  }
  off_t available = (off_t)stat.f_bavail * stat.f_frsize;
  if (available > size)
    return 1;
  else
    return 0;
}

void drop_caches() {
  sync();
  int dc_file = open("/proc/sys/vm/drop_caches", O_WRONLY);
  if (dc_file >= 0) {
    write(dc_file, "3", 1);
  } else {
    perror("open");
  }
  close(dc_file);
}

void run_simple_chunked_copy(int r_file, int w_file, off_t size,
                             int page_size) {
  long long elapsed;
  printf("\nuser space stats\n", size);
  elapsed = copy_file_chunk(r_file, w_file, page_size);
  print_stats(size, elapsed);
}

void run_send(int r_file, int w_file, off_t size) {
  long long elapsed;
  lseek(r_file, 0, SEEK_SET);
  lseek(w_file, 0, SEEK_SET);
  printf("\nsend stats\n", size);
  elapsed = copy_file_sendfile(r_file, w_file, size);
  print_stats(size, elapsed);
}

void run_send_nocache(int r_file, int w_file, off_t size) {
  long long elapsed;
  lseek(r_file, 0, SEEK_SET);
  lseek(w_file, 0, SEEK_SET);
  drop_caches();
  printf("\nno cache send stats\n", size);
  elapsed = copy_file_sendfile(r_file, w_file, size);
  print_stats(size, elapsed);
}

void run_simple_chunked_nocache(int r_file, int w_file, off_t size,
                                int page_size) {
  long long elapsed;
  lseek(r_file, 0, SEEK_SET);
  lseek(w_file, 0, SEEK_SET);
  drop_caches();
  printf("\nno cache user space stats\n", size);
  elapsed = copy_file_chunk(r_file, w_file, page_size);
  print_stats(size, elapsed);
}

void run_ap_excessive_fsync(int r_file, int w_file, off_t size, int page_size) {
  long long elapsed;
  lseek(r_file, 0, SEEK_SET);
  lseek(w_file, 0, SEEK_SET);
  printf("\nantipattern: excessive fsync\n", size);
  elapsed = copy_ap_esync(r_file, w_file, page_size);
  print_stats(size, elapsed);
}

void run_ap_excessive_stat(int r_file, int w_file, off_t size, int page_size) {
  long long elapsed;
  lseek(r_file, 0, SEEK_SET);
  lseek(w_file, 0, SEEK_SET);
  printf("\nantipattern: excessive stat\n", size);
  elapsed = copy_ap_estat(r_file, w_file, page_size, "copy1.1");
  print_stats(size, elapsed);
}

void run_tests(int r_file, int w_file, off_t size, int page_size,
               int runflags) {
  if (runflags & S_CHUNKED_CPY)
    run_simple_chunked_copy(r_file, w_file, size, page_size);
  if (runflags & SEND_CPY)
    run_send(r_file, w_file, size);
  if (runflags & SEND_NOCACHE_CPY)
    run_send_nocache(r_file, w_file, size);
  if (runflags & S_CHUNKED_NOCACHE_CPY)
    run_simple_chunked_nocache(r_file, w_file, size, page_size);
  if (runflags & S_CHUNKED_FSYNC_CPY)
    run_ap_excessive_fsync(r_file, w_file, size, page_size);
  if (runflags & S_CHUNKED_STAT_CPY)
    run_ap_excessive_stat(r_file, w_file, size, page_size);
}

int main(int argc, char *argv[]) {
  signal(SIGINT, skill);
  signal(SIGTERM, skill);

  int open_flags = O_RDONLY;
  int write_flags = O_WRONLY | O_CREAT;
  int page_size = getpagesize();
  int r_file;
  int w_file;
  off_t size;
  long long elapsed;
  size_t ret;
  int opt;
  double elapsed_sec;
  char filename[PATH_MAX] = {0};
  char tests[PATH_MAX] = {0};
  int runflags = 0;

  if (getuid() != 0) {
    printf("\nMust be ran as root.\n");
    return 1;
  }

  while ((opt = getopt(argc, argv, "f:t:")) != -1) {
    switch (opt) {
    case 'f':
      strncpy(filename, optarg, PATH_MAX - 1);
      break;
    case 't':
      strncpy(tests, optarg, PATH_MAX - 1);
      break;
    }
  }

  parse_tests(tests, &runflags);
  r_file = open(filename, open_flags);
  if (r_file < 0) {
    perror("open");
    return 1;
  }

  size = lseek(r_file, 0, SEEK_END);
  lseek(r_file, 0, SEEK_SET);
  printf("\nfile size:%ld bytes\n", size);
  printf("\npage size:%d bytes\n\n", page_size);

  if (check_disk_space(".", size) <= 0) {
    printf("\ncould not allocate space \n");
    close(r_file);
    return -1;
  }

  w_file = open("copy1.1", write_flags);
  if (w_file < 0) {
    perror("open");
    return 1;
  }

  run_tests(r_file, w_file, size, page_size, runflags);

  if (unlink("copy1.1") != 0) {
    perror("unlink");
  }

  close(r_file);
  close(w_file);
}
