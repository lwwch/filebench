#define _POSIX_C_SOURCE 199309L

#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <threads.h>
#include <time.h>
#include <unistd.h>

#define ERROR(fmt, ...)                                               \
  fprintf(stderr, "\x1b[31mERROR\x1b[0m (%s:%i) " fmt "\n", __FILE__, \
          __LINE__, ##__VA_ARGS__)
#define INFO(fmt, ...)                                                         \
  fprintf(stderr, "\x1b[34mINFO\x1b[0m (%s:%i) " fmt "\n", __FILE__, __LINE__, \
          ##__VA_ARGS__)
#define FATAL(fmt, ...)                                                    \
  do {                                                                     \
    fprintf(stderr, "\x1b[31;1mFATAL:\x1b[0m (%s:%i) " fmt "\n", __FILE__, \
            __LINE__, ##__VA_ARGS__);                                      \
    abort();                                                               \
  } while (0)
#define ASSERT(expr, fmt, ...)                                      \
  do {                                                              \
    if (!(expr)) {                                                  \
      FATAL("Expression failed: '" #expr "' " #fmt, ##__VA_ARGS__); \
    }                                                               \
  } while (0)
#define ASSERT_ERRNO(expr, fmt, ...) \
  ASSERT(expr, " [errno: %d, %s] " #fmt, errno, strerror(errno), ##__VA_ARGS__)

#define MIN(a, b) (a < b ? a : b)

typedef uint64_t TimePoint;
typedef int64_t TimeSpan;

TimePoint now() {
  struct timespec tv;
  clock_gettime(CLOCK_REALTIME, &tv);
  TimePoint now = tv.tv_sec;
  now *= 1000 * 1000 * 1000;
  now += tv.tv_nsec;
  return now;
}

typedef struct {
  int n;
  TimeSpan* data;
} Samples;

typedef struct {
  int file_size;
  int num_samples;
} Options;

bool parse_args(int argc, char* argv[], Options* opts) {
  opts->file_size = 1024;
  opts->num_samples = 1000;

#define PARSE_ARG(name, value, parser)                 \
  if (strcmp(arg, name) == 0) {                        \
    if (i == argc) {                                   \
      ERROR("Value '" name "' requires an argument."); \
      return false;                                    \
    }                                                  \
    value = parser(argv[i++]);                         \
    continue;                                          \
  }

  int i = 1;
  while (i < argc) {
    const char* arg = argv[i++];
    PARSE_ARG("--file-size", opts->file_size, atoi);
    PARSE_ARG("--num-samples", opts->num_samples, atoi);
    ERROR("Argument '%s' not understood.", arg);
    return false;
  }

  return true;
}

const char* make_filename(const char* working, const int n) {
  static char buffer[1024];
  memset(buffer, 0x00, sizeof(buffer));
  snprintf(buffer, sizeof(buffer), "%s/test-file-%06d.bin", working, n);
  return buffer;
}

void write_file(const char* path, const int size) {
  static char buffer[64 * 1024] = {0};

  const int fd = open(path, O_WRONLY | O_CREAT | O_EXCL, 0644);
  ASSERT_ERRNO(fd >= 0, "unable to open %s", path);

  int remaining = size;
  while (remaining > 0) {
    const int w = write(fd, buffer, MIN(sizeof(buffer), (uint64_t)remaining));
    ASSERT_ERRNO(w >= 0, "Unable to write to file %s", path);
    remaining -= w;
  }

  ASSERT_ERRNO(close(fd) == 0, "Unable to close file %s", path);
}

Samples benchmark(const Options* opts) {
  const char* working = ".testing";
  const int rc = mkdir(working, 0755);
  ASSERT_ERRNO(rc == 0, "unable to make dir %s", working);

  Samples samples = {opts->num_samples,
                     calloc(sizeof(TimeSpan), opts->num_samples)};
  for (int n = 0; n < opts->num_samples; ++n) {
    const char* filename = make_filename(working, n);
    const TimePoint start = now();
    write_file(filename, opts->file_size);
    const TimePoint end = now();
    samples.data[n] = end - start;
  }
  return samples;
}

int sample_less_than(const void* _a, const void* _b) {
  const TimeSpan a = *(TimeSpan*)_a;
  const TimeSpan b = *(TimeSpan*)_b;
  return a < b ? -1 : 1;
}

double usec(const TimeSpan ts) { return (double)ts / 1e3; }

void show_benchmark(const Samples samples) {
  qsort(samples.data, samples.n, sizeof(*samples.data), sample_less_than);
  INFO("Percentiles (usec)");
  INFO(" p00: %.3lf", usec(samples.data[0]));
  INFO(" p25: %.3lf", usec(samples.data[samples.n / 4]));
  INFO(" p50: %.3lf", usec(samples.data[samples.n / 2]));
  INFO(" p75: %.3lf", usec(samples.data[3 * samples.n / 4]));
  INFO("p100: %.3lf", usec(samples.data[samples.n - 1]));
}

void free_samples(const Samples samples) { free(samples.data); }

int main(int argc, char* argv[]) {
  Options opts = {0};
  if (!parse_args(argc, argv, &opts)) {
    return 1;
  }

  INFO("Benchmarking file writes: size=%i, n=%i", opts.file_size,
       opts.num_samples);

  const Samples samples = benchmark(&opts);
  show_benchmark(samples);
  free_samples(samples);

  return 0;
}
