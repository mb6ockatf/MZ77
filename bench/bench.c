#include <config.h>
#include <errno.h>
#include <getopt.h>
#include <limits.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>
#include "lz77.h"

enum BenchMode { BENCH_BOTH, BENCH_COMPRESS, BENCH_DECOMPRESS };

enum { MAX_REPEATS = 100, COPY_CHUNK = 65536 };

typedef struct
{
  int repeats;
  enum BenchMode mode;
  const char *csv_path;
  const char *label;
  const char *keep_path;
  bool archives;
  char work_dir[PATH_MAX];
} BenchOptions;

static double
now_seconds (void)
{
  struct timespec moment;
  clock_gettime (CLOCK_MONOTONIC, &moment);
  return (double) moment.tv_sec + (double) moment.tv_nsec / 1e9;
}

static int
compare_doubles (const void *left, const void *right)
{
  double left_value = *(const double *) left;
  double right_value = *(const double *) right;
  return (left_value > right_value) - (left_value < right_value);
}

static double
median (double *values, int count)
{
  qsort (values, (size_t) count, sizeof (double), compare_doubles);
  if (count % 2 == 1)
    return values[count / 2];
  return (values[count / 2 - 1] + values[count / 2]) / 2.0;
}

static long long
file_size (const char *path)
{
  struct stat info;
  if (stat (path, &info) != 0)
    return -1;
  return (long long) info.st_size;
}

static bool
files_equal (const char *left_path, const char *right_path)
{
  FILE *left = fopen (left_path, "rb");
  FILE *right = fopen (right_path, "rb");
  bool equal = left && right;
  static unsigned char left_block[COPY_CHUNK];
  static unsigned char right_block[COPY_CHUNK];
  while (equal)
    {
      size_t left_count = fread (left_block, 1, sizeof (left_block), left);
      size_t right_count = fread (right_block, 1, sizeof (right_block), right);
      if (left_count != right_count
	  || memcmp (left_block, right_block, left_count) != 0)
	equal = false;
      else if (left_count == 0)
	break;
    }
  if (left)
    fclose (left);
  if (right)
    fclose (right);
  return equal;
}

static const char *
base_name (const char *path)
{
  const char *slash = strrchr (path, '/');
  return slash ? slash + 1 : path;
}

static uint32_t
next_random (uint32_t *state)
{
  *state ^= *state << 13;
  *state ^= *state >> 17;
  *state ^= *state << 5;
  return *state;
}

static const char *const words[] = {
  "the", "of", "and", "to", "in", "a", "is", "that", "for", "it", "as",
  "was", "with", "be", "by", "on", "not", "he", "this", "are", "or", "his",
  "from", "at", "which", "but", "have", "an", "had", "they", "you", "were",
  "compression", "window", "archive", "dictionary", "sequence", "matching",
  "algorithm", "measure", "processor", "memory", "branch", "pipeline",
  "throughput", "latency", "buffer", "stream", "literal", "reference",
  "distance", "length", "encoder", "decoder", "optimisation", "profile",
  "benchmark", "machine", "independent", "dependent", "cache", "register"
};

static int
generate_byte (int pattern, uint32_t *state, size_t offset, char *word_buffer,
	       size_t *word_cursor, size_t *word_length)
{
  static const char phrase[] = "abcabcabd\n";
  switch (pattern)
    {
    case 0:
      if (*word_cursor >= *word_length)
	{
	  uint32_t pick = next_random (state);
	  const char *word = words[pick % (sizeof (words) / sizeof (*words))];
	  char separator = (pick >> 8) % 12 == 0 ? '\n'
	    : (pick >> 8) % 7 == 0 ? ',' : ' ';
	  *word_length = (size_t) snprintf (word_buffer, 32, "%s%c", word,
					    separator);
	  *word_cursor = 0;
	}
      return (unsigned char) word_buffer[(*word_cursor)++];
    case 1:
      return (int) (next_random (state) & 0xFF);
    case 2:
      return 0;
    default:
      return (next_random (state) % 8 == 0)
	? (int) (next_random (state) & 0xFF)
	: phrase[offset % (sizeof (phrase) - 1)];
    }
}

static int
generate_corpus (const char *dir, long long size)
{
  static const char *const names[] = { "text.dat", "random.dat",
    "zeros.dat", "mixed.dat"
  };
  if (mkdir (dir, 0777) != 0 && errno != EEXIST)
    {
      perror (dir);
      return EXIT_FAILURE;
    }
  for (int pattern = 0; pattern < 4; pattern++)
    {
      char path[PATH_MAX];
      snprintf (path, sizeof (path), "%s/%s", dir, names[pattern]);
      if (file_size (path) == size)
	continue;
      FILE *output = fopen (path, "wb");
      if (!output)
	{
	  perror (path);
	  return EXIT_FAILURE;
	}
      uint32_t state = 2463534242u + (uint32_t) pattern;
      char word_buffer[32];
      size_t word_cursor = 0, word_length = 0;
      for (long long offset = 0; offset < size; offset++)
	putc (generate_byte (pattern, &state, (size_t) offset, word_buffer,
			     &word_cursor, &word_length), output);
      if (fclose (output) != 0)
	{
	  perror (path);
	  return EXIT_FAILURE;
	}
      printf ("generated %s (%lld bytes)\n", path, size);
    }
  return EXIT_SUCCESS;
}

static FILE *
open_csv (const char *path)
{
  bool fresh = file_size (path) <= 0;
  FILE *csv = fopen (path, "a");
  if (!csv)
    {
      perror (path);
      return NULL;
    }
  if (fresh)
    fprintf (csv, "label,file,size,archive_size,ratio,repeats,"
	     "compress_s,decompress_s,compress_mb_s,decompress_mb_s\n");
  return csv;
}

static int
bench_file (const BenchOptions *options, const char *input_path, FILE *csv)
{
  long long size = file_size (input_path);
  if (size < 0)
    {
      perror (input_path);
      return EXIT_FAILURE;
    }
  char archive[PATH_MAX + 32], restored[PATH_MAX + 32];
  if (options->archives)
    snprintf (archive, sizeof (archive), "%s", input_path);
  else if (options->keep_path)
    snprintf (archive, sizeof (archive), "%s", options->keep_path);
  else
    snprintf (archive, sizeof (archive), "%s/archive.lz77", options->work_dir);
  snprintf (restored, sizeof (restored), "%s/restored", options->work_dir);
  double compress_times[MAX_REPEATS], decompress_times[MAX_REPEATS];
  int status = EXIT_SUCCESS;
  if (options->mode == BENCH_DECOMPRESS && !options->archives
      && lz77_compress (input_path, archive) != 0)
    {
      fprintf (stderr, "mz77-bench: %s: compression failed\n", input_path);
      return EXIT_FAILURE;
    }
  for (int run = 0; run < options->repeats; run++)
    {
      compress_times[run] = decompress_times[run] = 0.0;
      if (options->mode != BENCH_DECOMPRESS)
	{
	  double start = now_seconds ();
	  int result = lz77_compress (input_path, archive);
	  compress_times[run] = now_seconds () - start;
	  if (result != 0)
	    {
	      fprintf (stderr, "mz77-bench: %s: compression failed\n",
		       input_path);
	      status = EXIT_FAILURE;
	      break;
	    }
	}
      if (options->mode != BENCH_COMPRESS)
	{
	  double start = now_seconds ();
	  int result = lz77_decompress (archive, restored);
	  decompress_times[run] = now_seconds () - start;
	  if (result != 0)
	    {
	      fprintf (stderr, "mz77-bench: %s: decompression failed\n",
		       input_path);
	      status = EXIT_FAILURE;
	      break;
	    }
	  if (run == 0 && !options->archives
	      && !files_equal (input_path, restored))
	    {
	      fprintf (stderr, "mz77-bench: %s: round trip mismatch\n",
		       input_path);
	      status = EXIT_FAILURE;
	      break;
	    }
	}
    }
  long long archive_size = file_size (archive);
  if (options->archives)
    {
      archive_size = size;
      size = file_size (restored);
    }
  else if (!options->keep_path)
    remove (archive);
  remove (restored);
  if (status != EXIT_SUCCESS)
    return status;
  double compress_s = median (compress_times, options->repeats);
  double decompress_s = median (decompress_times, options->repeats);
  double megabytes = (double) size / 1e6;
  double ratio = size > 0 ? (double) archive_size / (double) size : 0.0;
  double compress_rate = compress_s > 0 ? megabytes / compress_s : 0.0;
  double decompress_rate = decompress_s > 0 ? megabytes / decompress_s : 0.0;
  printf ("%-14s %11lld %11lld %6.3f %9.4f %9.4f %8.2f %8.2f\n",
	  base_name (input_path), size, archive_size, ratio, compress_s,
	  decompress_s, compress_rate, decompress_rate);
  if (csv)
    fprintf (csv, "%s,%s,%lld,%lld,%.4f,%d,%.6f,%.6f,%.3f,%.3f\n",
	     options->label, base_name (input_path), size, archive_size,
	     ratio, options->repeats, compress_s, decompress_s,
	     compress_rate, decompress_rate);
  return EXIT_SUCCESS;
}

static void
show_usage (FILE *stream, const char *program)
{
  fprintf (stream,
	   "Usage: %s [OPTION]... FILE...\n"
	   "  or:  %s --generate=DIR [--size=BYTES]\n"
	   "Time lz77_compress and lz77_decompress on each FILE.\n"
	   "\n"
	   "  -r, --repeat=N      run each file N times, report the median"
	   " (default 5)\n"
	   "  -m, --mode=MODE     both, compress or decompress (default both)\n"
	   "  -c, --csv=FILE      append one CSV row per file to FILE\n"
	   "  -l, --label=TEXT    value of the CSV label column\n"
	   "  -a, --archives      each FILE is an archive: time decompression"
	   " only\n"
	   "  -k, --keep=PATH     keep the archive of the single FILE at PATH\n"
	   "  -w, --work-dir=DIR  where temporary archives go"
	   " (default /dev/shm or /tmp)\n"
	   "  -g, --generate=DIR  write the synthetic corpus to DIR and exit\n"
	   "  -s, --size=BYTES    size of each generated file"
	   " (default 20000000)\n"
	   "  -h, --help          display this help and exit\n", program,
	   program);
}

int
main (int argc, char **argv)
{
  static const struct option long_options[] = {
    {"repeat", required_argument, NULL, 'r'},
    {"mode", required_argument, NULL, 'm'},
    {"csv", required_argument, NULL, 'c'},
    {"label", required_argument, NULL, 'l'},
    {"archives", no_argument, NULL, 'a'},
    {"keep", required_argument, NULL, 'k'},
    {"work-dir", required_argument, NULL, 'w'},
    {"generate", required_argument, NULL, 'g'},
    {"size", required_argument, NULL, 's'},
    {"help", no_argument, NULL, 'h'},
    {NULL, 0, NULL, 0}
  };
  BenchOptions options = {.repeats = 5,.mode = BENCH_BOTH,.label = "" };
  const char *generate_dir = NULL;
  const char *work_parent = access ("/dev/shm", W_OK) == 0 ? "/dev/shm"
    : "/tmp";
  long long generate_size = 20000000;
  int option;
  while ((option = getopt_long (argc, argv, "r:m:c:l:ak:w:g:s:h", long_options,
				NULL)) != -1)
    {
      switch (option)
	{
	case 'r':
	  options.repeats = atoi (optarg);
	  if (options.repeats < 1 || options.repeats > MAX_REPEATS)
	    {
	      fprintf (stderr, "%s: --repeat must be 1 to %d\n", argv[0],
		       MAX_REPEATS);
	      return EXIT_FAILURE;
	    }
	  break;
	case 'm':
	  if (strcmp (optarg, "both") == 0)
	    options.mode = BENCH_BOTH;
	  else if (strcmp (optarg, "compress") == 0)
	    options.mode = BENCH_COMPRESS;
	  else if (strcmp (optarg, "decompress") == 0)
	    options.mode = BENCH_DECOMPRESS;
	  else
	    {
	      fprintf (stderr, "%s: unknown mode '%s'\n", argv[0], optarg);
	      return EXIT_FAILURE;
	    }
	  break;
	case 'c':
	  options.csv_path = optarg;
	  break;
	case 'l':
	  options.label = optarg;
	  break;
	case 'a':
	  options.archives = true;
	  break;
	case 'k':
	  options.keep_path = optarg;
	  break;
	case 'w':
	  work_parent = optarg;
	  break;
	case 'g':
	  generate_dir = optarg;
	  break;
	case 's':
	  generate_size = atoll (optarg);
	  break;
	case 'h':
	  show_usage (stdout, argv[0]);
	  return EXIT_SUCCESS;
	default:
	  show_usage (stderr, argv[0]);
	  return EXIT_FAILURE;
	}
    }
  if (generate_dir)
    return generate_corpus (generate_dir, generate_size);
  if (options.archives)
    options.mode = BENCH_DECOMPRESS;
  if (optind >= argc)
    {
      show_usage (stderr, argv[0]);
      return EXIT_FAILURE;
    }
  if (options.keep_path && (options.archives || optind + 1 != argc))
    {
      fprintf (stderr, "%s: --keep needs exactly one FILE and no --archives\n",
	       argv[0]);
      return EXIT_FAILURE;
    }
  snprintf (options.work_dir, sizeof (options.work_dir),
	    "%s/mz77-bench.XXXXXX", work_parent);
  if (!mkdtemp (options.work_dir))
    {
      perror (options.work_dir);
      return EXIT_FAILURE;
    }
  FILE *csv = options.csv_path ? open_csv (options.csv_path) : NULL;
  if (options.csv_path && !csv)
    return EXIT_FAILURE;
  printf ("%-14s %11s %11s %6s %9s %9s %8s %8s\n", "file", "size",
	  "archive", "ratio", "comp_s", "decomp_s", "comp_MB/s", "dec_MB/s");
  int status = EXIT_SUCCESS;
  for (int arg_index = optind; arg_index < argc; arg_index++)
    if (bench_file (&options, argv[arg_index], csv) != EXIT_SUCCESS)
      status = EXIT_FAILURE;
  if (csv)
    fclose (csv);
  rmdir (options.work_dir);
  return status;
}
