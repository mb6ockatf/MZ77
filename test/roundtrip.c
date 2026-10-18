#include <config.h>
#include <dirent.h>
#include <limits.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include "lz77.h"

enum
{ TIME_LIMIT_SECONDS = 120 };

static char work_dir[64];
static int failures, checks;

static void
fail (const char *name, const char *reason)
{
  failures++;
  fprintf (stderr, "FAIL: %s: %s\n", name, reason);
}

static void
pass (const char *name, const char *what)
{
  printf ("PASS: %s: %s\n", name, what);
}

static bool
files_equal (const char *left_path, const char *right_path)
{
  FILE *left = fopen (left_path, "rb");
  FILE *right = fopen (right_path, "rb");
  bool equal = left && right;
  while (equal)
    {
      int left_byte = fgetc (left);
      int right_byte = fgetc (right);
      if (left_byte != right_byte)
	equal = false;
      else if (left_byte == EOF)
	break;
    }
  if (left)
    fclose (left);
  if (right)
    fclose (right);
  return equal;
}

static bool
join_path (char *buffer, size_t size, const char *dir, const char *name,
	   const char *suffix)
{
  int written = snprintf (buffer, size, "%s/%s%s", dir, name, suffix);
  return written >= 0 && (size_t) written < size;
}

static bool
work_path (char *buffer, size_t size, const char *name, const char *suffix)
{
  return join_path (buffer, size, work_dir, name, suffix);
}

static void
check_roundtrip (const char *name, const char *input_path)
{
  char archive[PATH_MAX];
  char restored[PATH_MAX];
  if (!work_path (archive, sizeof (archive), name, ".lz77")
      || !work_path (restored, sizeof (restored), name, ".restored"))
    {
      fail (name, "path too long");
      return;
    }
  checks++;
  printf ("checking %s\n", name);
  if (lz77_compress (input_path, archive) != 0)
    fail (name, "compression returned an error");
  else if (lz77_decompress (archive, restored) != 0)
    fail (name, "decompression returned an error");
  else if (!files_equal (input_path, restored))
    fail (name, "round trip does not reproduce the input");
  else
    pass (name, "round trip");
  remove (archive);
  remove (restored);
}

static void
check_committed_archive (const char *data_dir, const char *name,
			 const char *input_path)
{
  char archive[PATH_MAX];
  char restored[PATH_MAX];
  char dist_dir[PATH_MAX];
  if (!join_path (dist_dir, sizeof (dist_dir), data_dir, "dist", "")
      || !join_path (archive, sizeof (archive), dist_dir, name, ".lz77"))
    {
      fail (name, "path too long");
      return;
    }
  if (access (archive, R_OK) != 0)
    return;
  if (!work_path (restored, sizeof (restored), name, ".committed"))
    {
      fail (name, "path too long");
      return;
    }
  checks++;
  if (lz77_decompress (archive, restored) != 0)
    fail (name, "committed archive failed to decompress");
  else if (!files_equal (input_path, restored))
    fail (name, "committed archive does not decompress to the input");
  else
    pass (name, "committed archive");
  remove (restored);
}

static void
check_data_dir (const char *data_dir)
{
  char input_dir[PATH_MAX];
  if (!join_path (input_dir, sizeof (input_dir), data_dir, "input", ""))
    {
      fail (data_dir, "path too long");
      return;
    }
  DIR *directory = opendir (input_dir);
  if (!directory)
    {
      fail (input_dir, "cannot open test input directory");
      return;
    }
  struct dirent *entry;
  while ((entry = readdir (directory)) != NULL)
    {
      if (entry->d_name[0] == '.')
	continue;
      char input_path[PATH_MAX];
      if (!join_path (input_path, sizeof (input_path), input_dir,
		      entry->d_name, ""))
	{
	  fail (entry->d_name, "path too long");
	  continue;
	}
      check_roundtrip (entry->d_name, input_path);
      check_committed_archive (data_dir, entry->d_name, input_path);
    }
  closedir (directory);
}

static uint32_t
next_random (uint32_t *state)
{
  *state ^= *state << 13;
  *state ^= *state >> 17;
  *state ^= *state << 5;
  return *state;
}

static void
check_generated (const char *name, size_t size, int pattern)
{
  char input_path[PATH_MAX];
  if (!work_path (input_path, sizeof (input_path), name, ""))
    {
      fail (name, "path too long");
      return;
    }
  FILE *output = fopen (input_path, "wb");
  if (!output)
    {
      fail (name, "cannot create generated input");
      return;
    }
  uint32_t random_state = 2463534242u;
  static const char phrase[] = "abcabcabd\n";
  for (size_t offset = 0; offset < size; offset++)
    {
      int byte;
      if (pattern == 0)
	byte = phrase[offset % (sizeof (phrase) - 1)];
      else if (pattern == 1)
	byte = (int) (next_random (&random_state) & 0xFF);
      else if (pattern == 2)
	byte = 0;
      else
	byte = (next_random (&random_state) % 8 == 0)
	  ? (int) (next_random (&random_state) & 0xFF)
	  : phrase[offset % (sizeof (phrase) - 1)];
      fputc (byte, output);
    }
  fclose (output);
  check_roundtrip (name, input_path);
  remove (input_path);
}

static void
check_rejects_bad_reference (void)
{
  const char *name = "bad-reference";
  char archive[PATH_MAX];
  char restored[PATH_MAX];
  if (!work_path (archive, sizeof (archive), name, ".lz77")
      || !work_path (restored, sizeof (restored), name, ".restored"))
    {
      fail (name, "path too long");
      return;
    }
  FILE *output = fopen (archive, "wb");
  if (!output)
    {
      fail (name, "cannot create archive");
      return;
    }
  static const unsigned char reference_before_start[] = { 0x80, 0x00, 0x00 };
  fwrite (reference_before_start, 1, sizeof (reference_before_start),
	  output);
  fclose (output);
  checks++;
  if (lz77_decompress (archive, restored) == 0)
    fail (name, "archive referring before the start was accepted");
  else
    pass (name, "rejected");
  remove (archive);
  remove (restored);
}

int
main (void)
{
  setvbuf (stdout, NULL, _IOLBF, 0);
  alarm (TIME_LIMIT_SECONDS);
  const char *data_dir = getenv ("MZ77_TEST_DATA");
  if (!data_dir || !*data_dir)
    data_dir = ".";
  strcpy (work_dir, "roundtrip-work.XXXXXX");
  if (!mkdtemp (work_dir))
    {
      perror ("mkdtemp");
      return EXIT_FAILURE;
    }
  check_data_dir (data_dir);
  check_generated ("empty", 0, 0);
  check_generated ("one-byte", 1, 0);
  check_generated ("repetitive-2M", 2000000, 0);
  check_generated ("random-1M", 1000000, 1);
  check_generated ("zeros-300K", 300000, 2);
  check_generated ("mixed-1M", 1000000, 3);
  check_rejects_bad_reference ();
  rmdir (work_dir);
  printf ("%d checks, %d failed\n", checks, failures);
  return failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
