#include "inode_manager.h"

// disk layer -----------------------------------------

disk::disk()
{
  bzero(blocks, sizeof(blocks));
}

void
disk::read_block(blockid_t id, char *buf)
{
  if (id < 0 || id >= BLOCK_NUM || buf == NULL)
    return;

  memcpy(buf, blocks[id], BLOCK_SIZE);
}

void
disk::write_block(blockid_t id, const char *buf)
{
  if (id < 0 || id >= BLOCK_NUM || buf == NULL)
    return;

  memcpy(blocks[id], buf, BLOCK_SIZE);
}

// block layer -----------------------------------------

// Allocate a free disk block.
blockid_t
block_manager::alloc_block()
{
  /*
   * your code goes here.
   * note: you should mark the corresponding bit in block bitmap when alloc.
   * you need to think about which block you can start to be allocated.
   */
  pthread_mutex_lock(&mutex);
  blockid_t result = *free_blocks_number.begin();
  free_blocks_number.erase(free_blocks_number.begin());
  pthread_mutex_unlock(&mutex);
  return result;
}

void
block_manager::free_block(uint32_t id)
{
  /* 
   * your code goes here.
   * note: you should unmark the corresponding bit in the block bitmap when free.
   */
  pthread_mutex_lock(&mutex);
  free_blocks_number.insert(id);
  pthread_mutex_unlock(&mutex);
  return;
}

// The layout of disk should be like this:
// |<-sb->|<-free block bitmap->|<-inode table->|<-data->|
block_manager::block_manager()
{
  d = new disk();

  // format the disk
  sb.size = BLOCK_SIZE * BLOCK_NUM;
  sb.nblocks = BLOCK_NUM;
  sb.ninodes = INODE_NUM;

  // add all blocks to free block set
  for (int i = IBLOCK(INODE_NUM - 1, BLOCK_NUM) + 1; i < BLOCK_NUM; ++i) {
    free_blocks_number.insert(i);
  }

  pthread_mutex_init(&mutex, NULL);
}

void
block_manager::read_block(uint32_t id, char *buf)
{
  d->read_block(id, buf);
}

void
block_manager::write_block(uint32_t id, const char *buf)
{
  d->write_block(id, buf);
}

// inode layer -----------------------------------------

inode_manager::inode_manager()
{
  bm = new block_manager();
  for (uint32_t i = 1; i<INODE_NUM; ++i) {
    free_inode_num.insert(i);
  }
  pthread_mutex_init(&mutex, NULL);
  uint32_t root_dir = alloc_inode(extent_protocol::T_DIR);
  if (root_dir != 1) {
    printf("\tim: error! alloc first inode %d, should be 1\n", root_dir);
    exit(0);
  }
}

/* Create a new file.
 * Return its inum. */
uint32_t
inode_manager::alloc_inode(uint32_t type)
{
  /* 
   * your code goes here.
   * note: the normal inode block should begin from the 2nd inode block.
   * the 1st is used for root_dir, see inode_manager::inode_manager().
   */
  pthread_mutex_lock(&mutex);
  struct inode* ino = (struct inode*)malloc(sizeof(struct inode));
  ino->type = type;
  ino->atime = time(NULL);
  ino->ctime = time(NULL);
  ino->mtime = time(NULL);
  ino->size = 0;
  uint32_t inum = *free_inode_num.begin();
  free_inode_num.erase(free_inode_num.begin());
  put_inode(inum, ino);
  free(ino);
  pthread_mutex_unlock(&mutex);
  return inum;
}

void
inode_manager::free_inode(uint32_t inum)
{
  /* 
   * your code goes here.
   * note: you need to check if the inode is already a freed one;
   * if not, clear it, and remember to write back to disk.
   */
  pthread_mutex_lock(&mutex);
  struct inode* target = get_inode(inum);
  if (target->type == 0) {
    // Already be freed
    return;
  }
  target->type = 0;
  put_inode(inum, target);
  free_inode_num.insert(inum);
  pthread_mutex_unlock(&mutex);
  return;
}


/* Return an inode structure by inum, NULL otherwise.
 * Caller should release the memory. */
struct inode* 
inode_manager::get_inode(uint32_t inum)
{
  struct inode *ino, *ino_disk;
  char buf[BLOCK_SIZE];

  printf("\tim: get_inode %d\n", inum);

  if (inum < 0 || inum >= INODE_NUM) {
    printf("\tim: inum out of range\n");
    return NULL;
  }

  bm->read_block(IBLOCK(inum, bm->sb.nblocks), buf);
  // printf("%s:%d\n", __FILE__, __LINE__);

  ino_disk = (struct inode*)buf + inum%IPB;
  if (ino_disk->type == 0) {
    printf("\tim: inode not exist\n");
    return NULL;
  }

  ino = (struct inode*)malloc(sizeof(struct inode));
  *ino = *ino_disk;

  return ino;
}

void
inode_manager::put_inode(uint32_t inum, struct inode *ino)
{
  char buf[BLOCK_SIZE];
  struct inode *ino_disk;

  printf("\tim: put_inode %d\n", inum);
  if (ino == NULL)
    return;

  bm->read_block(IBLOCK(inum, bm->sb.nblocks), buf);
  ino_disk = (struct inode*)buf + inum%IPB;
  *ino_disk = *ino;
  bm->write_block(IBLOCK(inum, bm->sb.nblocks), buf);
}

#define MIN(a,b) ((a)<(b) ? (a) : (b))

/* Get all the data of a file by inum. 
 * Return alloced data, should be freed by caller. */
void
inode_manager::read_file(uint32_t inum, char **buf_out, int *size)
{
  /*
   * your code goes here.
   * note: read blocks related to inode number inum,
   * and copy them to buf_Out
   */
  struct inode* target = get_inode(inum);
  if (target == NULL)
    return;
  int file_size = target->size;
  char* copy_buf = (char*)malloc(file_size);
  *buf_out = copy_buf;
  *size = file_size;
  if (file_size <= NDIRECT * BLOCK_SIZE) {
    // no need to use indirect block
    int copied = 0;
    int inode_blocks_index = 0;
    while (copied + BLOCK_SIZE <= file_size) {
      char local_buf[BLOCK_SIZE];
      bm->read_block(target->blocks[inode_blocks_index], local_buf);
      memcpy(copy_buf + inode_blocks_index * BLOCK_SIZE, local_buf, BLOCK_SIZE);
      inode_blocks_index++;
      copied += BLOCK_SIZE;
    }
    if (copied < file_size) {
      char local_buf[BLOCK_SIZE];
      bm->read_block(target->blocks[inode_blocks_index], local_buf);
      memcpy(copy_buf + inode_blocks_index * BLOCK_SIZE, local_buf, file_size - copied);
      return;
    }
    return;
  }
  else {
    // indirect block is used
    int copied = 0;
    int inode_blocks_index = 0;
    while (copied + BLOCK_SIZE <= file_size) {
      char local_buf[BLOCK_SIZE];
      get_datablock(target, inode_blocks_index, local_buf);
      memcpy(copy_buf + inode_blocks_index * BLOCK_SIZE, local_buf, BLOCK_SIZE);
      inode_blocks_index++;
      copied += BLOCK_SIZE;
    }
    if (copied < file_size) {
      char local_buf[BLOCK_SIZE];
      get_datablock(target, inode_blocks_index, local_buf);
      memcpy(copy_buf + inode_blocks_index * BLOCK_SIZE, local_buf, file_size - copied);
      return;
    }
    return;
  }
  return;
}

void
inode_manager::get_datablock(struct inode* inode, int inode_blocks_index, char* buf) {
  if (inode_blocks_index >= 0 && inode_blocks_index < NDIRECT) {
    bm->read_block(inode->blocks[inode_blocks_index], buf);
    return;
  }
  else if (inode_blocks_index >= NDIRECT && inode_blocks_index < NDIRECT + NINDIRECT) {
    char local_buf[BLOCK_SIZE];
    bm->read_block(inode->blocks[NDIRECT], local_buf);
    uint32_t* data_blocks_number = (uint32_t*) local_buf;
    inode_blocks_index -= NDIRECT;
    bm->read_block(data_blocks_number[inode_blocks_index], buf);
    return;
  }
}

uint32_t
inode_manager::get_datablock_number(struct inode* inode, int inode_blocks_index) {
  if (inode_blocks_index >= 0 && inode_blocks_index < NDIRECT) {
    return inode->blocks[inode_blocks_index];
  } else if (inode_blocks_index >= NDIRECT && inode_blocks_index < NDIRECT + NINDIRECT) {
    char local_buf[BLOCK_SIZE];
    bm->read_block(inode->blocks[NDIRECT], local_buf);
    uint32_t* indirect_block_number = (uint32_t*) local_buf;
    inode_blocks_index -= NDIRECT;
    return indirect_block_number[inode_blocks_index];
  }
  return -1;
}

/* alloc/free blocks if needed */
void
inode_manager::write_file(uint32_t inum, const char *buf, int size)
{
  /*
   * your code goes here.
   * note: write buf to blocks of inode inum.
   * you need to consider the situation when the size of buf 
   * is larger or smaller than the size of original inode
   */
  struct inode* target = get_inode(inum);
  if (target == NULL)
    return;
  unsigned int original_size = target->size;

  int inode_blocks_index = 0;
  unsigned int current_size = 0;

  // int round_up_size = (size/BLOCK_SIZE + 1) * BLOCK_SIZE;
  int round_up_size = ((size + BLOCK_SIZE - 1) / BLOCK_SIZE) * BLOCK_SIZE;
  char local_file_buf[round_up_size];
  memcpy(local_file_buf, buf, size);

  // free all the blocks of the original file
  while (current_size < original_size) {
    uint32_t data_block_number = get_datablock_number(target, inode_blocks_index);
    bm->free_block(data_block_number);
    current_size += BLOCK_SIZE;
    inode_blocks_index++;
  }

  // free the indirect block if necessary
  if (original_size > NDIRECT * BLOCK_SIZE) {
    uint32_t indirect_block_number = target->blocks[NDIRECT];
    bm->free_block(indirect_block_number);
  }

  // allocate new blocks for the new file
  inode_blocks_index = 0;
  current_size = 0;
  target->atime = time(NULL);
  target->ctime = time(NULL);
  target->mtime = time(NULL);
  target->size = size;
  while (current_size < round_up_size) {
    if (inode_blocks_index < NDIRECT) {
      uint32_t block_index = bm->alloc_block();
      bm->write_block(block_index, local_file_buf + inode_blocks_index * BLOCK_SIZE);
      target->blocks[inode_blocks_index] = block_index;
      inode_blocks_index++;
      current_size += BLOCK_SIZE;
      continue;
    }
    if (inode_blocks_index == NDIRECT) {
      uint32_t local_buf[NINDIRECT] = { 0 };
      uint32_t block_index_for_indirect_block = bm->alloc_block();
      target->blocks[NDIRECT] = block_index_for_indirect_block;
      uint32_t data_block_index = bm->alloc_block();
      bm->write_block(data_block_index, local_file_buf + inode_blocks_index * BLOCK_SIZE);
      local_buf[0] = data_block_index;
      bm->write_block(block_index_for_indirect_block, (const char*)local_buf);
      inode_blocks_index++;
      current_size += BLOCK_SIZE;
      continue;
    }
    if (inode_blocks_index > NDIRECT) {
      uint32_t local_buf[NINDIRECT];
      bm->read_block(target->blocks[NDIRECT], (char*)local_buf);
      uint32_t data_block_index = bm->alloc_block();
      bm->write_block(data_block_index, local_file_buf + inode_blocks_index * BLOCK_SIZE);
      local_buf[inode_blocks_index - NDIRECT] = data_block_index;
      bm->write_block(target->blocks[NDIRECT], (const char*)local_buf);
      inode_blocks_index++;
      current_size += BLOCK_SIZE;
      continue;
    }
  }
  // TODO: delete this
  if (current_size != round_up_size) {
    std::cout << "Error in inode_manager::write_file" << std::endl;
    exit(1);
  }

  put_inode(inum, target);
  return;
}

void
inode_manager::getattr(uint32_t inum, extent_protocol::attr &a)
{
  /*
   * your code goes here.
   * note: get the attributes of inode inum.
   * you can refer to "struct attr" in extent_protocol.h
   */
  struct inode* target = get_inode(inum);
  if (target == NULL) {
    a.type = 0;
    return;
  }
  a.atime = target->atime;
  a.ctime = target->ctime;
  a.mtime = target->mtime;
  a.size = target->size;
  a.type = target->type;
  return;
}

void
inode_manager::remove_file(uint32_t inum)
{
  /*
   * your code goes here
   * note: you need to consider about both the data block and inode of the file
   */
  struct inode* target = get_inode(inum);
  unsigned int original_size = target->size;

  int inode_blocks_index = 0;
  unsigned int current_size = 0;

  // free all the data blocks
  while (current_size < original_size) {
    uint32_t data_block_number = get_datablock_number(target, inode_blocks_index);
    bm->free_block(data_block_number);
    current_size += BLOCK_SIZE;
    inode_blocks_index++;
  }

  // free the indirect block if necessary
  if (original_size > NDIRECT * BLOCK_SIZE) {
    uint32_t indirect_block_number = target->blocks[NDIRECT];
    bm->free_block(indirect_block_number);
  }

  free_inode(inum);
  return;
}
