#include "storage.h"
#include "blocks.h"
#include "directory.h"
#include "inode.h"
#include "slist.h"
#include "bitmap.h"
#include <string.h>
#include <stdlib.h>
#define LINE_MAX_LENGTH 50

// initialize file structure
void storage_init(const char *path) {
  blocks_init(path);
  if (!bitmap_get(get_blocks_bitmap(), 4)) {
    directory_init();
  }
}

// updates the stats of the file at given path to the given file stats
int storage_stat(const char *path, struct stat *st) {
  int i = lookup(path);

  if (i > 0) {
    inode_t *n = get_inode(i);
    st->st_uid = getuid();
    st->st_size = n->size;
    st->st_mode = n->mode;
    st->st_nlink = n->refs;
    return 0;
  }
  return -1;
}

// reads file from given path
// returns size of data read
int storage_read(const char *path, char *buf, size_t size, off_t offset) {
  int i = lookup(path);
  inode_t *inode = get_inode(i);
  size_t src = offset;
  size_t dst = 0;
  size_t rem = size;

  while (rem > 0) {
    int bn = inode_get_bnum(inode, src);
    char *b = blocks_get_block(bn);
    int mod = src % BLOCK_SIZE;
    b += mod;
    size_t block = BLOCK_SIZE - mod;

    // adjust block size
    if (rem < block) {
      block = rem;
    }

    // copy data
    memcpy(buf + dst, b, block);

    // update indices and rem
    src += block;
    dst += block;
    rem -= block;
  }

  return size;
}

// writes to file at given path
// returns size of written data
int storage_write(const char *path, const char *buf, size_t size, off_t offset) {
  int i = lookup(path);
  inode_t *n = get_inode(i);

  // size after write operation
  size_t total = size + offset;

  // increase file size
  if (n->size < total) {
    storage_truncate(path, total);
  }

  size_t src = 0;
  size_t dst = offset;
  size_t rem = size;

  while (rem > 0) {
    int bnum = inode_get_bnum(n, dst);
    char *b = blocks_get_block(bnum);
    int mod = dst % BLOCK_SIZE;
    b += mod;

    size_t block = BLOCK_SIZE - mod;
    if (rem < block) {
      block = rem;
    }

    // copy data
    memcpy(b, buf + src, block);

    src += block;
    dst += block;
    rem -= block;
  }

  return size;
}

// truncates file at given path
int storage_truncate(const char *path, off_t size) {
  int i = lookup(path);
  inode_t *n = get_inode(i);

  if (n->size <= size) {
    grow_inode(n, size);
  } else {
    shrink_inode(n, size);
  }
  return 0;
}

// adds a directory at given path
int storage_mknod(const char *path, int mode) {
  size_t p_size = strlen(path);
  char *n = malloc(LINE_MAX_LENGTH);
  char *p = malloc(p_size);

  slist_t *p_list = slist_explode(path, '/');
  p[0] = 0;

  while (p_list->next) {
    strncat(p, "/", 2);
    strncat(p, p_list->data, 48);
    p_list = p_list->next;
  }

  size_t p_len = strlen(p_list->data);
  memcpy(n, p_list->data, p_len);
  n[p_len] = 0;

  int i = lookup(p);
  int new = alloc_inode();
  inode_t *in = get_inode(new);
  in->size = 0;
  in->refs = 1;
  in->mode = mode;

  int r = directory_put(get_inode(i), n, new);
  slist_free(p_list);
  return r;
}

// removes a link
int storage_unlink(const char *path) {
  char *n = malloc(LINE_MAX_LENGTH);
  char *p = malloc(strlen(path));

  slist_t *plist = slist_explode(path, '/');
  p[0] = 0;
  while (plist->next) {
    strncat(p, "/", 2);
    strncat(p, plist->data, 48);
    plist = plist->next;
  }

  strncpy(n, plist->data, LINE_MAX_LENGTH);
  int i = lookup(p);
  inode_t *par = get_inode(i);
  int r = directory_delete(par, n);
  slist_free(plist);
  return r;
}

// adds a link
int storage_link(const char *from, const char *to) {
  char *n = malloc(LINE_MAX_LENGTH);
  char *p = malloc(strlen(from));
  slist_t *plist = slist_explode(from, '/');
  p[0] = 0;

  while (plist->next) {
    strncat(p, "/", 2);
    strncat(p, plist->data, 48);
    plist = plist->next;
  }

  strncpy(n, plist->data, LINE_MAX_LENGTH);

  int from_i = lookup(p);
  inode_t *par = get_inode(from_i);
  int to_i = lookup(to);
  int r = directory_put(par, n, to_i);
  get_inode(to_i)->refs += 1;

  slist_free(plist);
  return r;
}

// renames file at given from path to the name of given to path
int storage_rename(const char *from, const char *to) {
  storage_link(to, from);
  storage_unlink(from);
  return 0;
}

// sets last modified time of file
int storage_set_time(const char *path, const struct timespec ts[2]) {
  int i = lookup(path);
  if (i >= 0) {
    return 0;
  }

  return -1;
}

// lists files/directories at given path
slist_t *storage_list(const char *path) {
  return directory_list(path);
}














