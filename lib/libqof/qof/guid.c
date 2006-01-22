/********************************************************************\
 * guid.c -- globally unique ID implementation                      *
 * Copyright (C) 2000 Dave Peticolas <peticola@cs.ucdavis.edu>      *
 *                                                                  *
 * This program is free software; you can redistribute it and/or    *
 * modify it under the terms of the GNU General Public License as   *
 * published by the Free Software Foundation; either version 2 of   *
 * the License, or (at your option) any later version.              *
 *                                                                  *
 * This program is distributed in the hope that it will be useful,  *
 * but WITHOUT ANY WARRANTY; without even the implied warranty of   *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the    *
 * GNU General Public License for more details.                     *
 *                                                                  *
 * You should have received a copy of the GNU General Public License*
 * along with this program; if not, contact:                        *
 *                                                                  *
 * Free Software Foundation           Voice:  +1-617-542-5942       *
 * 51 Franklin Street, Fifth Floor    Fax:    +1-617-542-2652       *
 * Boston, MA  02110-1301,  USA       gnu@gnu.org                   *
 *                                                                  *
\********************************************************************/

#define _GNU_SOURCE

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <sys/types.h>
#include <ctype.h>
#include <dirent.h>
#include <glib.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/times.h>
#include <time.h>
#include <unistd.h>
#include "qof.h"
#include "md5.h"

# ifndef P_tmpdir
#  define P_tmpdir "/tmp"
# endif

/* Constants *******************************************************/
#define DEBUG_GUID 0
#define BLOCKSIZE 4096
#define THRESHOLD (2 * BLOCKSIZE)


/* Static global variables *****************************************/
static gboolean guid_initialized = FALSE;
static struct md5_ctx guid_context;
#ifndef HAVE_GLIB29
static GMemChunk *guid_memchunk = NULL;
#endif

/* This static indicates the debugging module that this .o belongs to.  */
static QofLogModule log_module = QOF_MOD_ENGINE;

/* Memory management routines ***************************************/
#ifdef HAVE_GLIB29
GUID *
guid_malloc (void)
{
  return g_slice_new(GUID);
}

void
guid_free (GUID *guid)
{
  if (!guid)
    return;

  g_slice_free(GUID, guid);
}
#else /* !HAVE_GLIB29 */

static void
guid_memchunk_init (void)
{
  if (!guid_memchunk)
    guid_memchunk = g_mem_chunk_create (GUID, 512, G_ALLOC_AND_FREE);
}

static void
guid_memchunk_shutdown (void)
{
  if (guid_memchunk)
  {
    g_mem_chunk_destroy (guid_memchunk);
    guid_memchunk = NULL;
  }
}

GUID *
guid_malloc (void)
{
  if (!guid_memchunk) guid_memchunk_init();
  return g_chunk_new (GUID, guid_memchunk);
}

void
guid_free (GUID *guid)
{
  if (!guid)
    return;

  g_chunk_free (guid, guid_memchunk);
}
#endif


const GUID *
guid_null(void)
{
  static int null_inited = 0;
  static GUID null_guid;

  if (!null_inited)
  {
    int i;
    char *tmp = "NULLGUID.EMPTY.";

      /* 16th space for '\O' */
	  for (i = 0; i < 16; i++)
      null_guid.data[i] = tmp[i];

    null_inited = 1;
  }

  return &null_guid;
}

/* Function implementations ****************************************/

/* This code is based on code in md5.c in GNU textutils. */
static size_t
init_from_stream(FILE *stream, size_t max_size)
{
  char buffer[BLOCKSIZE + 72];
  size_t sum, block_size, total;

  if (max_size <= 0)
    return 0;

  total = 0;

  /* Iterate over file contents. */
  while (1)
  {
    /* We read the file in blocks of BLOCKSIZE bytes.  One call of the
     * computation function processes the whole buffer so that with the
     * next round of the loop another block can be read.  */
    size_t n;
    sum = 0;

    if (max_size < BLOCKSIZE)
      block_size = max_size;
    else
      block_size = BLOCKSIZE;

    /* Read block.  Take care for partial reads.  */
    do
    {
      n = fread (buffer + sum, 1, block_size - sum, stream);

      sum += n;
    }
    while (sum < block_size && n != 0);

    max_size -= sum;

    if (n == 0 && ferror (stream))
      return total;

    /* If end of file or max_size is reached, end the loop. */
    if ((n == 0) || (max_size == 0))
      break;

    /* Process buffer with BLOCKSIZE bytes.  Note that
     * BLOCKSIZE % 64 == 0  */
    md5_process_block (buffer, BLOCKSIZE, &guid_context);

    total += sum;
  }

  /* Add the last bytes if necessary.  */
  if (sum > 0)
  {
    md5_process_bytes (buffer, sum, &guid_context);
    total += sum;
  }

  return total;
}

static size_t
init_from_file(const char *filename, size_t max_size)
{
  struct stat stats;
  size_t total = 0;
  size_t file_bytes;
  FILE *fp;

  memset(&stats, 0, sizeof(stats));
  if (stat(filename, &stats) != 0)
    return 0;

  md5_process_bytes(&stats, sizeof(stats), &guid_context);
  total += sizeof(stats);

  if (max_size <= 0)
    return total;

  fp = fopen (filename, "r");
  if (fp == NULL)
    return total;

  file_bytes = init_from_stream(fp, max_size);

  PINFO ("guid_init got %llu bytes from %s", (unsigned long long int) file_bytes,
	 filename);

  total += file_bytes;

  fclose(fp);

  return total;
}

static size_t
init_from_dir(const char *dirname, unsigned int max_files)
{
  char filename[1024];
  struct dirent *de;
  struct stat stats;
  size_t total;
  int result;
  DIR *dir;

  if (max_files <= 0)
    return 0;

  dir = opendir (dirname);
  if (dir == NULL)
    return 0;

  total = 0;

  do
  {
    de = readdir(dir);
    if (de == NULL)
      break;

    md5_process_bytes(de, sizeof(struct dirent), &guid_context);
    total += sizeof(struct dirent);

    result = snprintf(filename, sizeof(filename),
                      "%s/%s", dirname, de->d_name);
    if ((result < 0) || (result >= (int)sizeof(filename)))
      continue;

    memset(&stats, 0, sizeof(stats));
    if (stat(filename, &stats) != 0)
      continue;
    md5_process_bytes(&stats, sizeof(stats), &guid_context);
    total += sizeof(stats);

    max_files--;
  } while (max_files > 0);

  closedir(dir);

  return total;
}

static size_t
init_from_time(void)
{
  size_t total;
  time_t t_time;
  clock_t clocks;
  struct tms tms_buf;

  total = 0;

  t_time = time(NULL);
  md5_process_bytes(&t_time, sizeof(t_time), &guid_context);
  total += sizeof(t_time);

  clocks = times(&tms_buf);
  md5_process_bytes(&clocks, sizeof(clocks), &guid_context);
  md5_process_bytes(&tms_buf, sizeof(tms_buf), &guid_context);
  total += sizeof(clocks) + sizeof(tms_buf);

  return total;
}

static size_t
init_from_int(int val)
{
  md5_process_bytes(&val, sizeof(val), &guid_context);
  return sizeof(int);
}

static size_t
init_from_buff(unsigned char * buf, size_t buflen)
{
  md5_process_bytes(buf, buflen, &guid_context);
  return buflen;
}

void
guid_init(void)
{
  size_t bytes = 0;

  /* Not needed; taken care of on first malloc.
   * guid_memchunk_init(); */

  md5_init_ctx(&guid_context);

  /* entropy pool */
  bytes += init_from_file ("/dev/urandom", 512);

  /* files */
  {
    const char * files[] =
    { "/etc/passwd",
      "/proc/loadavg",
      "/proc/meminfo",
      "/proc/net/dev",
      "/proc/rtc",
      "/proc/self/environ",
      "/proc/self/stat",
      "/proc/stat",
      "/proc/uptime",
      NULL
    };
    int i;

    for (i = 0; files[i] != NULL; i++)
      bytes += init_from_file(files[i], BLOCKSIZE);
  }

  /* directories */
  {
    const char * dirname;
    const char * dirs[] =
    {
      "/proc",
      P_tmpdir,
      "/var/lock",
      "/var/log",
      "/var/mail",
      "/var/spool/mail",
      "/var/run",
      NULL
    };
    int i;

    for (i = 0; dirs[i] != NULL; i++)
      bytes += init_from_dir(dirs[i], 32);

    dirname = g_get_home_dir();
    if (dirname != NULL)
      bytes += init_from_dir(dirname, 32);
  }

  /* process and parent ids */
  {
    pid_t pid;

    pid = getpid();
    md5_process_bytes(&pid, sizeof(pid), &guid_context);
    bytes += sizeof(pid);

    pid = getppid();
    md5_process_bytes(&pid, sizeof(pid), &guid_context);
    bytes += sizeof(pid);
  }

  /* user info */
  {
    uid_t uid;
    gid_t gid;
    char *s;

    s = getlogin();
    if (s != NULL)
    {
      md5_process_bytes(s, strlen(s), &guid_context);
      bytes += strlen(s);
    }

    uid = getuid();
    md5_process_bytes(&uid, sizeof(uid), &guid_context);
    bytes += sizeof(uid);

    gid = getgid();
    md5_process_bytes(&gid, sizeof(gid), &guid_context);
    bytes += sizeof(gid);
  }

  /* host info */
  {
    char string[1024];

    memset(string, 0, sizeof(string));
    gethostname(string, sizeof(string));
    md5_process_bytes(string, sizeof(string), &guid_context);
    bytes += sizeof(string);
  }

  /* plain old random */
  {
    int n, i;

    srand((unsigned int) time(NULL));

    for (i = 0; i < 32; i++)
    {
      n = rand();

      md5_process_bytes(&n, sizeof(n), &guid_context);
      bytes += sizeof(n);
    }
  }

  /* time in secs and clock ticks */
  bytes += init_from_time();

  PINFO ("got %llu bytes", (unsigned long long int) bytes);

  if (bytes < THRESHOLD)
    PWARN("only got %llu bytes.\n"
              "The identifiers might not be very random.\n",
	  (unsigned long long int)bytes);

  guid_initialized = TRUE;
}

void
guid_init_with_salt(const void *salt, size_t salt_len)
{
  guid_init();

  md5_process_bytes(salt, salt_len, &guid_context);
}

void
guid_init_only_salt(const void *salt, size_t salt_len)
{
  md5_init_ctx(&guid_context);

  md5_process_bytes(salt, salt_len, &guid_context);

  guid_initialized = TRUE;
}

void 
guid_shutdown (void)
{
#ifndef HAVE_GLIB29
	guid_memchunk_shutdown();
#endif
}

#define GUID_PERIOD 5000

void
guid_new(GUID *guid)
{
  static int counter = 0;
  struct md5_ctx ctx;

  if (guid == NULL)
    return;

  if (!guid_initialized)
    guid_init();

  /* make the id */
  ctx = guid_context;
  md5_finish_ctx(&ctx, guid->data);

  /* update the global context */
  init_from_time();

  /* Make it a little extra salty.  I think init_from_time was buggy,
	* or something, since duplicate id's actually happened. Or something
	* like that.  I think this is because init_from_time kept returning
	* the same values too many times in a row.  So we'll do some 'block
	* chaining', and feed in the old guid as new random data.
	*
	* Anyway, I think the whole fact that I saw a bunch of duplicate 
	* id's at one point, but can't reproduce the bug is rather alarming.
	* Something must be broken somewhere, and merely adding more salt
	* is just hiding the problem, not fixing it.
	*/
  init_from_int (433781*counter);
  init_from_buff (guid->data, 16);

  if (counter == 0)
  {
    FILE *fp;

    fp = fopen ("/dev/urandom", "r");
    if (fp == NULL)
      return;

    init_from_stream(fp, 32);

    fclose(fp);

    counter = GUID_PERIOD;
  }

  counter--;
}

GUID
guid_new_return(void)
{
  GUID guid;

  guid_new (&guid);

  return guid;
}

/* needs 32 bytes exactly, doesn't print a null char */
static void
encode_md5_data(const unsigned char *data, char *buffer)
{
  size_t count;

  for (count = 0; count < 16; count++, buffer += 2)
    sprintf(buffer, "%02x", data[count]);
}

/* returns true if the first 32 bytes of buffer encode
 * a hex number. returns false otherwise. Decoded number
 * is packed into data in little endian order. */
static gboolean
decode_md5_string(const unsigned char *string, unsigned char *data)
{
  unsigned char n1, n2;
  size_t count = -1;
  unsigned char c1, c2;

  if (NULL == data) return FALSE;
  if (NULL == string) goto badstring;

  for (count = 0; count < 16; count++)
  {
    /* check for a short string e.g. null string ... */
    if ((0==string[2*count]) || (0==string[2*count+1])) goto badstring;

    c1 = tolower(string[2 * count]);
    if (!isxdigit(c1)) goto badstring;

    c2 = tolower(string[2 * count + 1]);
    if (!isxdigit(c2)) goto badstring;

    if (isdigit(c1))
      n1 = c1 - '0';
    else
      n1 = c1 - 'a' + 10;

    if (isdigit(c2))
      n2 = c2 - '0';
    else
      n2 = c2 - 'a' + 10;

    data[count] = (n1 << 4) | n2;
  }
  return TRUE;

badstring:
  for (count = 0; count < 16; count++)
  {
    data[count] = 0;
  }
  return FALSE;
}

/* Allocate the key */

const char *
guid_to_string(const GUID * guid)
{
#ifdef G_THREADS_ENABLED
  static GStaticPrivate guid_buffer_key = G_STATIC_PRIVATE_INIT;
  gchar *string;

  string = g_static_private_get (&guid_buffer_key);
  if (string == NULL) {
    string = malloc(GUID_ENCODING_LENGTH+1);
    g_static_private_set (&guid_buffer_key, string, g_free);
  }
#else
  static char string[64];
#endif

  encode_md5_data(guid->data, string);
  string[GUID_ENCODING_LENGTH] = '\0';

  return string;
}

char *
guid_to_string_buff(const GUID * guid, char *string)
{
  if (!string || !guid) return NULL;

  encode_md5_data(guid->data, string);

  string[GUID_ENCODING_LENGTH] = '\0';
  return &string[GUID_ENCODING_LENGTH];
}

gboolean
string_to_guid(const char * string, GUID * guid)
{
  return decode_md5_string(string, (guid != NULL) ? guid->data : NULL);
}

gboolean
guid_equal(const GUID *guid_1, const GUID *guid_2)
{
  if (guid_1 && guid_2)
    return (memcmp(guid_1, guid_2, sizeof(GUID)) == 0);
  else
    return FALSE;
}

gint
guid_compare(const GUID *guid_1, const GUID *guid_2)
{
  if (guid_1 == guid_2)
    return 0;

  /* nothing is always less than something */
  if (!guid_1 && guid_2)
    return -1;

  if (guid_1 && !guid_2)
    return 1;

  return memcmp (guid_1, guid_2, sizeof (GUID));
}

guint
guid_hash_to_guint (gconstpointer ptr)
{
  const GUID *guid = ptr;

  if (!guid)
  {
    PERR ("received NULL guid pointer.");
    return 0;
  }

  if (sizeof(guint) <= sizeof(guid->data))
  {
    return (*((guint *) guid->data));
  }
  else
  {
    guint hash = 0;
    unsigned int i, j;

    for (i = 0, j = 0; i < sizeof(guint); i++, j++) {
      if (j == 16) j = 0;

      hash <<= 4;
      hash |= guid->data[j];
    }

    return hash;
  }
}

static gint
guid_g_hash_table_equal (gconstpointer guid_a, gconstpointer guid_b)
{
  return guid_equal (guid_a, guid_b);
}

GHashTable *
guid_hash_table_new (void)
{
  return g_hash_table_new (guid_hash_to_guint, guid_g_hash_table_equal);
}
