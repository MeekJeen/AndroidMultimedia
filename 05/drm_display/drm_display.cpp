/***********************************************************
* Author        : 公众号: Android系统攻城狮
* Create time   : 2023-10-17 22:53:51 星期二
* Filename      : drm_display.cpp
* Description   : DRM输出显示示例
************************************************************/

#include <fcntl.h>
#include <cstdio>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>
#include <xf86drm.h>
#include <xf86drmMode.h>

typedef struct drm_contexts{
  uint32_t width;
  uint32_t height;
  uint32_t pitch;
  uint32_t handle;
  uint32_t size;
  uint8_t *vaddr;
  uint32_t fb_id;
}drm_context;

drm_context drm_ct;

static int drm_create_fb(int fd, drm_context *drm_con){
  struct drm_mode_create_dumb drm_create;
  struct drm_mode_map_dumb drm_map;

  drm_create.width = drm_con->width;
  drm_create.height = drm_con->height;
  drm_create.bpp = 32;
  drmIoctl(fd, DRM_IOCTL_MODE_CREATE_DUMB, &drm_create);

  drm_con->pitch = drm_create.pitch;
  drm_con->size = drm_create.size;
  drm_con->handle = drm_create.handle;
  drmModeAddFB(fd, drm_con->width, drm_con->height, 24, 32, drm_con->pitch, drm_con->handle, &drm_con->fb_id);

  drm_map.handle = drm_create.handle;
  drmIoctl(fd, DRM_IOCTL_MODE_MAP_DUMB, &drm_map);

  drm_con->vaddr = (uint8_t*)mmap(0, drm_create.size, PROT_READ | PROT_WRITE,MAP_SHARED, fd, drm_map.offset);
  for (int i = 0; i < drm_con->size / 4; i++) {
    *((uint32_t*)drm_con->vaddr + i) = 0x00FF00;
  }
  return 0;
}

static void drm_destroy_fb(int fd, drm_context *drm_con){
  drmModeRmFB(fd, drm_con->fb_id);
  munmap(drm_con->vaddr, drm_con->size);
  drmIoctl(fd, DRM_IOCTL_MODE_DESTROY_DUMB, &drm_con);
}

int main(){
  int fd;
  drmModeConnector *con;
  drmModeRes *dmres;

  fd = open("/dev/dri/card0", O_RDWR | O_CLOEXEC);
  dmres = drmModeGetResources(fd);
  con = drmModeGetConnector(fd, dmres->connectors[0]);
  drm_ct.width = con->modes[0].hdisplay;
  drm_ct.height = con->modes[0].vdisplay;

  drm_create_fb(fd, &drm_ct);
  drmModeSetCrtc(fd, dmres->crtcs[0], drm_ct.fb_id, 0, 0, &(dmres->connectors[0]), 1, &con->modes[0]);
  getchar();

  drm_destroy_fb(fd, &drm_ct);
  drmModeFreeConnector(con);
  drmModeFreeResources(dmres);
  close(fd);
  return 0;
}
