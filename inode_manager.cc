#include "inode_manager.h"
#include <ctime>

// helping functions
static inline unsigned int
get_block_num( const unsigned int size )
{
  return size == 0 ? 0 : ( size - 1 ) / BLOCK_SIZE + 1;
}

static inline bool 
block_id_is_valid( const blockid_t id )  
{ 
  if ( id < 0 || id >= BLOCK_NUM )
  { 
    printf( "block id (%d) is not valid! BLOCK_NUM is %d\n", id, BLOCK_NUM );
    return false; 
  }
  return true; 
}

static inline bool 
buf_is_valid( const char * buf )
{
  if ( !buf )
  {
    printf( "buffer is null!\n" );
    return false;
  }
  return true;
}

static inline bool 
free_block_id_is_used( const blockid_t id, const std::map <uint32_t, int> & using_blocks )
{
  if ( !block_id_is_valid( id ) )
    return false;
  if ( using_blocks.at(id) == 0 )
  {
    printf( "the block %d is already freed!\n", id );
    return false;
  }
  return true;
}

static inline bool 
inode_id_is_valid( const uint32_t id )  
{ 
  if ( id < 0 || id >= INODE_NUM )
  { 
    printf( "inode id (%d) is not valid! INODE_NUM is %d\n", id, BLOCK_NUM );
    return false; 
  }
  return true; 
}

static inline bool 
free_inode_id_is_used( const uint32_t id, const std::map <uint32_t, bool> & using_inodes )
{
  if ( !inode_id_is_valid( id ) )
    return false;
  if ( using_inodes.at(id) == false )
  {
    printf( "the inode %d is already freed!\n" );
    return false;
  }
  return true;
}

// disk layer -----------------------------------------

disk::disk()
{
  bzero(blocks, sizeof(blocks));
}

void
disk::read_block(blockid_t id, char *buf)
{
  // written by Alexanderia
  if ( !block_id_is_valid( id ) || !buf_is_valid( buf ) )
    exit( 0 );
  memcpy( buf, blocks[id], BLOCK_SIZE );
}

void
disk::write_block(blockid_t id, const char *buf)
{
  // written by Alexanderia
  if ( !block_id_is_valid( id ) || !buf_is_valid( buf ) )
    exit( 0 );
  memcpy( blocks[id], buf, BLOCK_SIZE );
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
  // written by Alexanderia
  if ( free_block_list.empty() )
  {
    printf( "the disk is full!\n" );
    exit( 0 );
  }
  blockid_t next_free_block = free_block_list.front();
  free_block_list.pop_front();
  using_blocks[next_free_block] = 1;
  return next_free_block;
}

void
block_manager::free_block(uint32_t id)
{
  /* 
   * your code goes here.
   * note: you should unmark the corresponding bit in the block bitmap when free.
   */
  if ( !free_block_id_is_used( id, using_blocks ) )
    exit( 0 );
  free_block_list.push_back( id );
  using_blocks[id] = 0;
  return;
}

// The layout of disk should be like this:
// |<-sb->|<-free block bitmap->|<-inode table->|<-data->|
block_manager::block_manager()
{
  d = new disk();
  // constructing free list and using blocks bitmap
  for ( blockid_t i = IBLOCK( INODE_NUM, sb.nblocks ) ; i < BLOCK_NUM; ++i )
    free_block_list.push_back( i );
  for ( blockid_t i = 0; i < BLOCK_NUM; ++i )
    using_blocks[i] = 0;
  // format the disk
  sb.size = BLOCK_SIZE * BLOCK_NUM;
  sb.nblocks = BLOCK_NUM;
  sb.ninodes = INODE_NUM;
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

blockid_t 
inode_manager::inode_get_block_id( const inode_t * inode, const unsigned int index ) const
{
  if ( index >= get_block_num( inode->size ) )
  {
    printf( "inode_get_block_id: index (%d) out of range! size = %d\n", index, get_block_num( inode->size ) );
    exit( 0 );
  }
  if ( index >= MAXFILE )
  {
    printf( "inode_get_block_id: index (%d) out of range! MAXFILE = %d\n", index, MAXFILE );
    exit( 0 );
  }
  if ( index < NDIRECT )
    return inode->blocks[index];
  if ( index < MAXFILE )
  {
    char id_map[BLOCK_SIZE];
    bm->read_block( inode->blocks[NDIRECT], id_map );
    return ( ( blockid_t *) id_map ) [ index - NDIRECT ];
  }
}

void 
inode_manager::inode_allocate_block( inode_t * inode, unsigned int index )
{
  if ( index > NDIRECT + NINDIRECT )
  {
    printf(
      "inode_allocate_block: index (%d) is larger than maximum (%d)!\n", 
      index, NDIRECT + NINDIRECT 
    );
    exit( 0 );
  }
  if ( index < NDIRECT )
  {
    inode->blocks[index] = bm->alloc_block();
    return;
  }
  if ( index == NDIRECT )
  {
    inode->blocks[index] = bm->alloc_block();
    char block_buf[BLOCK_SIZE];
    unsigned int * p_new_block_id;
    * p_new_block_id = bm->alloc_block();
    memcpy( block_buf, p_new_block_id, sizeof( uint ) );
    bm->write_block( inode->blocks[index], block_buf );
    return;
  }
  if ( index > NDIRECT )
  {
    char block_id_table[BLOCK_SIZE];
    bm->read_block( inode->blocks[NDIRECT], block_id_table );
    unsigned int * p_new_block_id;
    * p_new_block_id = bm->alloc_block();
    memcpy( 
      block_id_table + (index - NDIRECT) * sizeof( uint ), 
      p_new_block_id, 
      sizeof( uint ) 
    );
    bm->write_block( inode->blocks[NDIRECT], block_id_table );
  }
}

void 
inode_manager::inode_free_block_id( inode_t * inode, unsigned int index )
{
  if ( index >= inode->size )
  {
    printf( "inode_free_block: index (%d) is larger than size (%d)", index, inode->size );
    exit( 0 );
  }
  if ( index > NDIRECT + NINDIRECT )
  {
    printf( 
      "inode_free_block: index (%d) is larger than maximum (%d)!\n", 
      index, NDIRECT + NINDIRECT 
    );
    exit( 0 );
  }
  if ( index != NDIRECT )
  {
    bm->free_block( inode_get_block_id( inode, index ) );
    return;
  }
  if ( index == NDIRECT )
  {
    bm->free_block( inode_get_block_id( inode, index ) );
    bm->free_block( inode->blocks[NDIRECT] );
  }
}

void 
inode_manager::alloc_inode_with_id_and_size( const uint32_t type, const uint32_t inum, const unsigned int size )
{
  if ( size < 0 || size > MAXFILE * BLOCK_SIZE )
  {
    printf( "write_file: the file size (%d) is invalid!\n", size );
    exit( 0 );
  }
  // update type and time
  // inum is not used
  if ( using_inodes[inum] == false )
  {
    using_inodes[inum] = true;
    for ( std::list<unsigned int>::iterator it = free_inode_list.begin(); it != free_inode_list.end(); ++it )
    {
      if ( *it == inum )
      {
        free_inode_list.erase( it );
        break;
      }
    }
    inode_t * inode = (inode_t *) malloc( sizeof( inode_t ) );
    inode->type   = type;
    inode->size   = 0;
    inode->mtime  = std::time( 0 );
    inode->ctime  = std::time( 0 );
    inode->atime  = std::time( 0 );
    put_inode( inum, inode );
    free( inode );
  }
  // inum is already used
  else
  {
    inode_t * inode = get_inode( inum );
    inode->type   = type;
    inode->mtime  = std::time( 0 );
    inode->ctime  = std::time( 0 );
    inode->atime  = std::time( 0 );
    put_inode( inum, inode );
    free( inode );
  }
  // readjust the size
  inode_t * inode = get_inode( inum );
  unsigned int old_block_num = get_block_num( inode->size );
  unsigned int new_block_num = get_block_num( size );
  if ( new_block_num > old_block_num )
    for ( unsigned int i = old_block_num; i < new_block_num; ++i )
      inode_allocate_block( inode, i );
  else
    for ( unsigned int i = new_block_num; i < old_block_num; ++i )
      inode_free_block_id( inode, i );
  inode->size = size;
  put_inode( inum, inode );
  free( inode );
}

inode_manager::inode_manager()
{
  bm = new block_manager();
  for ( uint32_t i = 1; i < INODE_NUM; ++i )
  {
    free_inode_list.push_back( i );
    using_inodes[i] = false;
  }
  uint32_t root_dir = alloc_inode(extent_protocol::T_DIR);
  if (root_dir != 1) 
  {
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
  if ( type != extent_protocol::T_DIR && type != extent_protocol::T_FILE )
  {
    printf( "alloc_inode: type (%d) is not valid. It should be 1 for directory or 0 for file", type );
    exit( 0 );
  }
  if ( free_inode_list.empty() )
  {
    printf( "alloc_inode: the inode table is full!\n" );
    exit( 0 );
  }
  uint32_t next_free_inode_id = free_inode_list.front();
  free_inode_list.pop_front();
  using_inodes[next_free_inode_id] = true;
  inode_t * next_free_inode = (inode_t *) malloc( sizeof( inode_t ) );
  // set attributes for the inode
  next_free_inode->type   = type;
  next_free_inode->size   = 0;
  next_free_inode->atime  = (unsigned) std::time( 0 );
  next_free_inode->mtime  = (unsigned) std::time( 0 );
  next_free_inode->ctime  = (unsigned) std::time( 0 );
  // put the inode back to the disk and free the copy
  put_inode( next_free_inode_id, next_free_inode );
  free( next_free_inode );
  return next_free_inode_id;
}

void
inode_manager::free_inode(uint32_t inum)
{
  /* 
   * your code goes here.
   * note: you need to check if the inode is already a freed one;
   * if not, clear it, and remember to write back to disk.
   */
  if ( !free_inode_id_is_used( inum, using_inodes ) )
    exit( 0 );
  inode_t * inode = get_inode( inum );
  // mark the inode as deleted
  inode->type = 0;
  // delete the file contents
  unsigned int block_num = get_block_num( inode->size );
  for ( unsigned int i = 0; i < block_num; ++i )
  {
    inode_free_block_id( inode, i );
  }
  free_inode_list.push_back( inum );
  using_inodes[inum] = false;
  free( inode );
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
  // read the block that contains the inode
  bm->read_block(IBLOCK(inum, bm->sb.nblocks), buf);
  // printf("%s:%d\n", __FILE__, __LINE__);
  // get the inode structure in that block
  ino_disk = (struct inode*)buf + inum%IPB;
  if (ino_disk->type == 0) {
    printf("\tim: inode not exist\n");
    return NULL;
  }
  // allocate a memory to store the content of that inode
  ino = (struct inode*)malloc(sizeof(struct inode));
  *ino = *ino_disk;
  // return the allocated copy of the specific inode
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
  inode_t * inode = get_inode( inum );
  * size = inode->size;
  * buf_out = (char *) malloc( * size );
  
  char buf[BLOCK_SIZE];
  uint32_t fully_occupied_block_num = * size / BLOCK_SIZE;
  unsigned int block_remainder_size = * size % BLOCK_SIZE;
  for ( blockid_t i = 0; i < fully_occupied_block_num; ++i )
  {
    bm->read_block( inode_get_block_id( inode, i ), buf );
    memcpy( * buf_out + i * BLOCK_SIZE, buf, BLOCK_SIZE );
  }
  if ( block_remainder_size )
  {
    bm->read_block( inode_get_block_id( inode, fully_occupied_block_num ), buf );
    memcpy( * buf_out + fully_occupied_block_num * BLOCK_SIZE, buf, block_remainder_size );
  }
  return;
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
  // adjust new blocks in block manager for the file, and set proper attributes for inode
  alloc_inode_with_id_and_size( extent_protocol::T_FILE, inum, size );
  // copy the contents
  blockid_t fully_occupied_block_num = size / BLOCK_SIZE;
  unsigned int remainder_size = size % BLOCK_SIZE;
  inode_t * inode = get_inode( inum );
  assert( inode->size == size );
  for ( unsigned int i = 0; i < fully_occupied_block_num; ++i )
  {
    bm->write_block( inode_get_block_id( inode, i ), buf + i * BLOCK_SIZE );
  }
  if ( remainder_size )
  {
    char temp[BLOCK_SIZE];
    memcpy( temp, buf + fully_occupied_block_num * BLOCK_SIZE, remainder_size );
    bm->write_block( inode_get_block_id( inode, fully_occupied_block_num ), temp );
  }
  free( inode );
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
  inode_t * inode = get_inode( inum );
  if ( inode == NULL )
  {
    printf( "getattr: inode inum is invalid" );
    exit( 0 );
  }
  a.type  = (uint32_t) inode->type;
  a.size  = inode->size;
  a.atime = inode->atime;
  a.mtime = inode->mtime;
  a.ctime = inode->ctime;
  free( inode );
  return;
}

void
inode_manager::remove_file(uint32_t inum)
{
  /*
   * your code goes here
   * note: you need to consider about both the data block and inode of the file
   */
  // aha
  return;
}
