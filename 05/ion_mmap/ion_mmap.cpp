/***********************************************************
 * Author        : 公众号: Android系统攻城狮
 * Create time   : 2023-10-20 00:26:11 星期五
 * Filename      : ion_mmap.cpp
 * Description   : ION共享内存分配及读写示例
 ************************************************************/

#include <iostream>
#include <fcntl.h>
#include <string>
#include <sys/ioctl.h>
#include <sys/mman.h>

#define ION_IOC_MAGIC 'I'
#define ION_IOC_ALLOC _IOWR(ION_IOC_MAGIC, 0, struct ion_allocation_data)
#define ION_IOC_FREE _IOWR(ION_IOC_MAGIC, 1, struct ion_handle_data)
#define ION_IOC_MAP _IOWR(ION_IOC_MAGIC, 2, struct ion_fd_data)
#define ION_IOC_SHARE _IOWR(ION_IOC_MAGIC, 4, struct ion_fd_data)
#define ION_IOC_IMPORT _IOWR(ION_IOC_MAGIC, 5, struct ion_fd_data)
#define ION_IOC_SYNC _IOWR(ION_IOC_MAGIC, 7, struct ion_fd_data)
#define ION_IOC_CUSTOM _IOWR(ION_IOC_MAGIC, 6, struct ion_custom_data)
#define ION_IOC_ABI_VERSION _IOR(ION_IOC_MAGIC, 9, __u32)

struct ion_fd_data {
  int handle;
  int fd;
};

struct ion_handle_data {
  int handle;
};

struct ion_allocation_data {
  size_t len;
  size_t align;
  unsigned int heap_id_mask;
  unsigned int flags;
  int handle;
};

int main() {
  int ion_fd, shared_fd;
  int size = 4096;
  void* shared_buffer;
  struct ion_allocation_data alloc_data;
  struct ion_fd_data fd_data;

  ion_fd = open("/dev/ion", O_RDWR);
  memset(&alloc_data, 0, sizeof(alloc_data));

  alloc_data.len = size;
  alloc_data.heap_id_mask = 1 << 25;
  alloc_data.flags = 0;
  int rc = ioctl(ion_fd,ION_IOC_ALLOC,&alloc_data);

  memset(&fd_data, 0, sizeof(fd_data));
  fd_data.handle = alloc_data.handle;
  rc = ioctl(ion_fd, ION_IOC_MAP, &fd_data);

  shared_fd = fd_data.fd;
  shared_buffer = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd_data.fd, 0);
  std::string tmpbuf = "Hello ION Memory!";
  memcpy(shared_buffer, tmpbuf.c_str(), tmpbuf.size());

  struct ion_fd_data read_fd;
  memset(&read_fd, 0, sizeof(read_fd));
  void *read_shared_buffer = nullptr;
  read_fd.fd = shared_fd;
  rc = ioctl(ion_fd, ION_IOC_IMPORT, &read_fd);
  read_shared_buffer = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, read_fd.fd, 0);
  printf(" %s\n", (char*)read_shared_buffer);

  ioctl(ion_fd, ION_IOC_FREE, &alloc_data);
  munmap(shared_buffer,size);
  return 0;
}
