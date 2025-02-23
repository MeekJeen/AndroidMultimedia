/***********************************************************
* Author        : 公众号: Android系统攻城狮
* Create time   : 2023-11-20 12:38:59 星期一
* Filename      : gpu_mmap.cpp
* Description   : Adreno GPU使用显存示例
************************************************************/

#include <fcntl.h>
#include <unistd.h>
#include <cstdio>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <iostream>

struct kgsl_gpumem_alloc {
  unsigned long gpuaddr;
  size_t size;
  unsigned int flags;
};
#define KGSL_IOC_TYPE 0x09
#define KGSL_MEMALIGN_SHIFT 16
#define KGSL_MEMFLAGS_USE_CPU_MAP 0x10000000ULL
#define KGSL_CACHEMODE_SHIFT 26
#define KGSL_CACHEMODE_WRITEBACK 3
#define KGSL_MEMTYPE_SHIFT 8
#define KGSL_MEMTYPE_OBJECTANY 0
#define IOCTL_KGSL_GPUMEM_ALLOC _IOWR(KGSL_IOC_TYPE, 0x2f, struct kgsl_gpumem_alloc)

struct kgsl_sharedmem_free {
  unsigned long gpuaddr;
};
#define IOCTL_KGSL_SHAREDMEM_FREE  _IOW(KGSL_IOC_TYPE, 0x21, struct kgsl_sharedmem_free)

int main(void) {
  unsigned long size = 4096;
  struct kgsl_gpumem_alloc kpa;

  kpa.size = size;
  kpa.flags = (2<<KGSL_MEMALIGN_SHIFT) | KGSL_MEMFLAGS_USE_CPU_MAP | (KGSL_CACHEMODE_WRITEBACK << KGSL_CACHEMODE_SHIFT) | (KGSL_MEMTYPE_OBJECTANY << KGSL_MEMTYPE_SHIFT);

  int fd = open("/dev/kgsl-3d0", O_RDWR);
  ioctl(fd, IOCTL_KGSL_GPUMEM_ALLOC, &kpa);


  void *map_buf = mmap(NULL, size, PROT_READ|PROT_WRITE, MAP_SHARED, fd, kpa.gpuaddr);
  std::string writebuf = "Hello GPU Memory!";
  memcpy(map_buf, writebuf.c_str(), writebuf.size());

  char *readbuf = nullptr;
  int read_fd = fd;

  readbuf = (char*)mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, read_fd, kpa.gpuaddr);

  printf("readbuf = %s\n",readbuf);

  ioctl(read_fd, IOCTL_KGSL_SHAREDMEM_FREE, &kpa);
  munmap(readbuf, size);
  close(read_fd);
}
