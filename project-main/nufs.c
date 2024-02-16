// based on cs3650 starter code

#include <assert.h>
#include <bsd/string.h>
#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>

#define FUSE_USE_VERSION 26
#include <fuse.h>
// #include "storage.h"
#include "directory.h"
#include "inode.h"
#include "bitmap.h"

#define LINE_MAX_LENGTH 50

// implementation for: man 2 access
// Checks if a file exists.
int nufs_access(const char *path, int mask) {
  int rv = lookup(path);

  // Only the root directory and our simulated file are accessible for now...
  // If it was found in tree
  if (rv >= 0) {
    inode_t *i = get_inode(rv);
    rv = 0;
  } else { // ...not found in tree
    rv = -ENOENT;
  }

  printf("access(%s, %04o) -> %d\n", path, mask, rv);
  return rv;
}

// Gets an object's attributes (type, permissions, size, etc).
// Implementation for: man 2 stat
// This is a crucial function.
int nufs_getattr(const char *path, struct stat *st) {
  int rv;

  // Return some metadata for the root directory...
  if (strcmp(path, "/") == 0) {
    st->st_mode = 040755; // directory
    st->st_size = 0;
    st->st_uid = getuid();
    rv = 0;
  }
  // ...and the simulated file...
  else {
    int i = lookup(path);

    if (i >0) {
      inode_t *node = get_inode(i);
      st->st_uid = getuid();
      st->st_size = node->size;
      st->st_mode = node->mode;
      st->st_nlink = node->refs;
      rv = 0;
    } else if (i <= 0) {
      rv = -1;
    }

    if (rv >= 0) {
      st->st_uid = getuid();
    } else {
      // Other files don't exist
      return -ENOENT;
    }
  }
  printf("getattr(%s) -> (%d) {mode: %04o, size: %ld}\n", path, rv, st->st_mode,
         st->st_size);
  return 0;
}

// implementation for: man 2 readdir
// lists the contents of a directory
int nufs_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
                 off_t offset, struct fuse_file_info *fi) {
  struct stat st;
  int rv;

  rv = nufs_getattr(path, &st);
  assert(rv == 0);
  slist_t *dirs = directory_list(path);
  filler(buf, ".", &st, 0);

  if (!dirs) {
    printf("readdir(%s) -> %d\n", path, rv);
    return 0;
  }

  slist_t *curr = dirs;
  while (curr) {
    int len = strlen(path);
    char pc[len + 50];
    strncpy(pc, path, len);

    if (path[len - 1] != '/') {
      pc[len] = '/';
      pc[len + 1] = 0;
    } else {
      pc[len] = 0;
    }

  
    strncat(pc, curr->data, 48);
    nufs_getattr(pc, &st);
    filler(buf, curr->data, &st, 0);
    curr = curr->next;
  }
  printf("readdir(%s) -> %d\n", path, rv);
  return 0;
}

// mknod makes a filesystem object like a file or directory
// called for: man 2 open, man 2 link
// Note, for this assignment, you can alternatively implement the create
// function.
int nufs_mknod(const char *path, mode_t mode, dev_t rdev) {
  int rv;

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

  rv = directory_put(get_inode(i), n, new);
  slist_free(p_list);

  printf("mknod(%s, %04o) -> %d\n", path, mode, rv);
  return rv;
}

// most of the following callbacks implement
// another system call; see section 2 of the manual
int nufs_mkdir(const char *path, mode_t mode) {
  int rv = nufs_mknod(path, mode | 040000, 0);
  printf("mkdir(%s) -> %d\n", path, rv);
  return rv;
}

int nufs_unlink(const char *path) {
  int rv;
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
  rv = directory_delete(par, n);
  slist_free(plist);
  printf("unlink(%s) -> %d\n", path, rv);
  return rv;
}

int nufs_link(const char *from, const char *to) {
  int rv;
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
  rv = directory_put(par, n, to_i);
  get_inode(to_i)->refs += 1;

  slist_free(plist);
  printf("link(%s => %s) -> %d\n", from, to, rv);
  return rv;
}

int nufs_rmdir(const char *path) {
  int rv = -1;
  printf("rmdir(%s) -> %d\n", path, rv);
  return rv;
}

// implements: man 2 rename
// called to move a file within the same filesystem
int nufs_rename(const char *from, const char *to) {
  int rv = 0;
  nufs_link(to, from);
  nufs_unlink(from);
  printf("rename(%s => %s) -> %d\n", from, to, rv);
  return rv;
}

int nufs_chmod(const char *path, mode_t mode) {
  int rv = -1;
  printf("chmod(%s, %04o) -> %d\n", path, mode, rv);
  return rv;
}

int nufs_truncate(const char *path, off_t size) {
  int rv = 0;
  int i = lookup(path);
  inode_t *n = get_inode(i);

  if (n->size <= size) {
    grow_inode(n, size);
  } else {
    shrink_inode(n, size);
  }

  printf("truncate(%s, %ld bytes) -> %d\n", path, size, rv);
  return rv;
}

// This is called on open, but doesn't need to do much
// since FUSE doesn't assume you maintain state for
// open files.
// You can just check whether the file is accessible.
int nufs_open(const char *path, struct fuse_file_info *fi) {
  int rv = 0;
  printf("open(%s) -> %d\n", path, rv);
  return rv;
}

// Actually read data
int nufs_read(const char *path, char *buf, size_t size, off_t offset,
              struct fuse_file_info *fi) {
  int rv = size;
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

  printf("read(%s, %ld bytes, @+%ld) -> %d\n", path, size, offset, rv);
  return rv;
}

// Actually write data
int nufs_write(const char *path, const char *buf, size_t size, off_t offset,
               struct fuse_file_info *fi) {
  int rv = size;
  int i = lookup(path);
  inode_t *n = get_inode(i);

  // size after write operation
  size_t total = size + offset;

  // increase file size
  if (n->size < total) {
    nufs_truncate(path, total);
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
  printf("write(%s, %ld bytes, @+%ld) -> %d\n", path, size, offset, rv);
  return rv;
}

// Update the timestamps on a file or directory.
int nufs_utimens(const char *path, const struct timespec ts[2]) {
  int rv;
  int i = lookup(path);
  if (i < 0) {
    rv = -1;
  } else {
    rv = 0;
  }
  printf("utimens(%s, [%ld, %ld; %ld %ld]) -> %d\n", path, ts[0].tv_sec,
         ts[0].tv_nsec, ts[1].tv_sec, ts[1].tv_nsec, rv);
  return rv;
}

// Extended operations
int nufs_ioctl(const char *path, int cmd, void *arg, struct fuse_file_info *fi,
               unsigned int flags, void *data) {
  int rv = 0;
  printf("ioctl(%s, %d, ...) -> %d\n", path, cmd, rv);
  return rv;
}

void nufs_init_ops(struct fuse_operations *ops) {
  memset(ops, 0, sizeof(struct fuse_operations));
  ops->access = nufs_access;
  ops->getattr = nufs_getattr;
  ops->readdir = nufs_readdir;
  ops->mknod = nufs_mknod;
  // ops->create   = nufs_create; // alternative to mknod
  ops->mkdir = nufs_mkdir;
  ops->link = nufs_link;
  ops->unlink = nufs_unlink;
  ops->rmdir = nufs_rmdir;
  ops->rename = nufs_rename;
  ops->chmod = nufs_chmod;
  ops->truncate = nufs_truncate;
  ops->open = nufs_open;
  ops->read = nufs_read;
  ops->write = nufs_write;
  ops->utimens = nufs_utimens;
  ops->ioctl = nufs_ioctl;
};

struct fuse_operations nufs_ops;

int main(int argc, char *argv[]) {
  assert(argc > 2 && argc < 6);
  blocks_init(argv[--argc]);
  if (!bitmap_get(get_blocks_bitmap(), 4)) {
    directory_init();
  }
  nufs_init_ops(&nufs_ops);
  return fuse_main(argc, argv, &nufs_ops, NULL);
}

