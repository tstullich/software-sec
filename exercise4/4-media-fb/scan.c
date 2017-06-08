#include <stdlib.h>
#include <stdio.h>
#include <dirent.h>
#include <unistd.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <syslog.h>
#include <magic.h>
#include "mediacenter.h"

#define FILEDATA_PER_CHUNK		4000
#define	LIBMAGIC_FEATURE_BITS		(MAGIC_MIME_TYPE | \
					MAGIC_NO_CHECK_COMPRESS | \
					MAGIC_NO_CHECK_CDF | \
					MAGIC_NO_CHECK_ENCODING | \
					MAGIC_NO_CHECK_TAR | \
					MAGIC_NO_CHECK_TEXT | \
					MAGIC_NO_CHECK_TOKENS)

/**
 * Memory Management
 * -----------------
 *
 * Each scanned directory is represented as one (or more) singly-linked
 * 'media_file_chunk' structures. Memory is allocated pagewise; the head
 * of each page contains the 'media_file_chunk' data, followed by as
 * many variable-length 'media_file' entries as would fit. If the directory
 * has more entries, another page is allocated.
 */

static struct media_file_chunk head = { NULL, "", 0, 0, {} };
struct media_file *cursor = NULL;
static magic_t libmagic_cookie;
unsigned int total_count;

/**
 * Allocate a new page for the directory 'dir', and add it to the linked
 * list. (New chunks are appended to the end of the list.)
 */
static struct media_file_chunk *allocate_chunk(const char *dir) {
  struct media_file_chunk *A, *B;

  A = (struct media_file_chunk *)mmap(NULL, 4096, PROT_READ|PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  A->next = 0;
  strcpy(A->dir, dir);
  A->count = 0;
  A->next_file_offset = 0;

  for (B = &head; B->next; B = B->next) ;
  B->next = A;

  return A;
}

static void find_existing_dirs(void) {
  DIR *d;
  struct dirent *de;
  char no_dir1[256];
  char no_dir2[256];

  d = opendir(media_path);

  if (!d) {
    mediacenter_log(LOG_ERR, "Error, cannot access %s tree...\n", media_path);
    return;
  }

  while ((de = readdir(d))) {
    struct stat dest;
    char den[256];
    sprintf(den, "%s/%s", media_path, de->d_name);
    (void)stat(den, &dest);

    sprintf(no_dir1, "%s/.", media_path);
    sprintf(no_dir2, "%s/..", media_path);

    if (S_ISDIR(dest.st_mode) && strcmp(den, no_dir1) && strcmp(den, no_dir2)) {
      add_notify(den);
      scan_dir(den);
    }
  }

  closedir(d);
}

void init_scan_subsystem() {
  if (!libmagic_cookie) {
    libmagic_cookie = magic_open(LIBMAGIC_FEATURE_BITS);
    magic_load(libmagic_cookie, NULL);
  }
  total_count = 0;
  find_existing_dirs();
}

/**
 * Scan the new directory 'dir'. Assumes that the directory is mounted and
 * accessible. This operation does not clear old entries for the directory;
 * that had better be done beforehand.
 */
void scan_dir(const char *dir) {
  struct media_file_chunk *currChunk = NULL;
  DIR *d;
  struct dirent *de;
  struct stat des;
  char de_fullname[256];
  const char *dem;

  d = opendir(dir);
  if (d == NULL) return;

  while ((de = readdir(d)) != NULL) {
    struct media_file *mf;
    int at = -1;

    sprintf(de_fullname, "%s/%s", dir, de->d_name);
    if (stat(de_fullname, &des)) continue;
    if (!S_ISREG(des.st_mode)) continue;
    dem = magic_file(libmagic_cookie, de_fullname);
    if (!dem) continue;
    at = find_handler(dem);
    if (at == -1) continue;

    if (!currChunk) {
      currChunk = allocate_chunk(dir);
    }
    if (currChunk->next_file_offset + sizeof(struct media_file) + strlen(de->d_name) + 1 >= FILEDATA_PER_CHUNK) {
      total_count += currChunk->count;
      currChunk = allocate_chunk(dir);
    }

    mf = (struct media_file *)(currChunk->files + currChunk->next_file_offset);
    mf->type = at;
    strcpy(mf->name, de->d_name);
    currChunk->count++;
    currChunk->next_file_offset += sizeof(struct media_file) + strlen(de->d_name) + 1;

    mediacenter_log(LOG_INFO, "New file: ");
    mediacenter_log(LOG_INFO, de_fullname);
    mediacenter_log(LOG_INFO, "\n");

    if (cursor == NULL) cursor = mf;
  }

  if (currChunk) {
    total_count += currChunk->count;
  }

  closedir(d);

  mediacenter_log(LOG_INFO, "Found new files in %s! Now %d tracks.\n", dir, total_count);
}

/**
 * Drop all chunks which represent (parts of) directory 'dir'.
 */
void forget_dir(const char *dir) {
  struct media_file_chunk *A, *AN;

  for (A = &head; A->next; A = A->next) {
    while (A->next && strcmp(A->next->dir, dir) == 0) {
      total_count -= A->next->count;
      if (file_to_chunk(cursor) == A->next)
        cursor = NULL;
      AN = A->next->next;
      munmap(A->next, 4096);
      A->next = AN;
    }
    if (!A->next) break;
  }

  mediacenter_log(LOG_INFO, "Directory %s gone! Now %d tracks.\n", dir, total_count);
}

/**
 * Determine the successor to the given media file.
 * NULL as argument returns the first entry.
 * NULL as return value means the end of the list has been reached.
 */
struct media_file *next_file(struct media_file *af) {
  struct media_file_chunk *ac;
  int off;

  if (!af) {
    if (head.next) {
      return (struct media_file *)head.next->files;
    } else {
      return NULL;
    }
  } else {
    ac = file_to_chunk(af);
    off = ((char *)af) - (ac->files);
    if (off + (int)sizeof(struct media_file) + (int)strlen(af->name) + 1 >= ac->next_file_offset) {
      ac = ac->next;
      if (!ac) return NULL;
      return (struct media_file *)ac->files;
    } else {
      return (struct media_file *)(ac->files + off + sizeof(struct media_file) + strlen(af->name) + 1);
    }
  }
}

/**
 * Advance the global playlist cursor.
 */
void advance_cursor() {
  cursor = next_file(cursor);
  if (cursor == NULL)
    cursor = next_file(NULL);
}
