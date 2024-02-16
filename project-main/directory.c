#include "directory.h" 
#include <string.h> 
#include "bitmap.h"
#include "inode.h"
#include "slist.h"

// initializes
void directory_init() {
  inode_t *r = get_inode(alloc_inode());
  r->mode = 040755;
}

// finds inum of given inode
int directory_lookup(inode_t *di, const char *name) {
  // root
  if (!strcmp(name, "")) {
    return 0;
  } else {
    dirent_t *directs = blocks_get_block(di->ptrs[0]);
    int i = 0;
    while (i < 64) {
      dirent_t curr = directs[i];
      if (!strcmp(name, curr.name)) {
        return curr.inum;
      }

      ++i;
    }

    // no directory exists
    return -1;
  }
}

// finds node at given path
int lookup(const char *path) {
  slist_t *p = slist_explode(path, '/');
  int i = 0;

  while (p) {
    inode_t *n = get_inode(i);
    i = directory_lookup(n, p->data);

    if (i >= 0) {
      p = p->next;
    } else {
      i = -1;
      break;
    }
  }
  slist_free(p);
  return i;
}

// creates new directory
int directory_put(inode_t *di, const char *name, int inum) {
  dirent_t dir;

  strncpy(dir.name, name, DIR_NAME_LENGTH);
  dir.inum = inum;

  int count = di->size / DIR_SIZE;
  dirent_t *dirs = blocks_get_block(di->ptrs[0]);

  dirs[count] = dir;
  di->size += DIR_SIZE;
  return 0;
}

// deletes given directory
int directory_delete(inode_t *di, const char *name) {
  int count = di->size / DIR_SIZE;
  dirent_t *dirs = blocks_get_block(di->ptrs[0]);

  int i = 0;
  while (i < count) {
    if (!strcmp(dirs[i].name, name)) {
      int d = dirs[i].inum;
      inode_t *n = get_inode(d);

      --n->refs;
      if (n->refs <= 0) {
        free_inode(d);
      }
      for (int j = i; j < count - 1; ++j) {
        dirs[j] = dirs[j + 1];
      }

      di->size -= DIR_SIZE;
      return 0;
    }
    ++i;
  }

  return -1;
}

// gets list of directories at given path
slist_t *directory_list(const char *path) {
  int curr = lookup(path);
  inode_t *curr_inode = get_inode(curr);
  int n = curr_inode->size / DIR_SIZE;
  dirent_t *dirs = blocks_get_block(curr_inode->ptrs[0]);
  slist_t *list = NULL;

  int i = 0;
  while (i < n) {
    if (dirs[i]._reserved) {
      list = slist_cons(dirs[i].name, list);
    }
    ++i;
  }

  return list;
}

// prints first directory at given inode
void print_directory(inode_t *dd) {
  dirent_t *dirs = blocks_get_block(dd->ptrs[0]);
  printf("%s", dirs[0].name);
}

