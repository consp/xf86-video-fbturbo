#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <linux/fb.h>

#include "sunxi_display2.h"
#include "g2d_driver.h"

#define PRINTD(x) printf("%s=%d\n",#x,x);
#define PRINTLD(x) printf("%s=%ld\n",#x,x);
#define PRINTLX(x) printf("%s=%lx\n",#x,x);
#define PRINTS(x) printf("%s=%s\n",#x,x);

void scn_size(int dispfd) {
  printf("获取屏幕水平分辨率\n");
  unsigned int screen_width, screen_height;
  unsigned long arg[3];
  arg[0] = 0;
  screen_width = ioctl(dispfd, DISP_GET_SCN_WIDTH, (void*)arg);
  screen_height = ioctl(dispfd, DISP_GET_SCN_HEIGHT, (void*)arg);
  printf("%dx%d\n", screen_width, screen_height);
}

void output_type(int dispfd) {
  printf("获取当前显示输出类型\n");
  enum disp_output_type output_type;
  unsigned long arg[3];
  arg[0] = 0;
  output_type = (enum disp_output_type)ioctl(dispfd, DISP_GET_OUTPUT_TYPE, (void*)arg);
  printf("%d\n", output_type);

  printf("获取当前显示输出类型\n");
  struct disp_output output;
  enum disp_output_type type;
  enum disp_tv_mode mode;
  arg[0] = 0;
  arg[1] = (unsigned long)&output;
  ioctl(dispfd, DISP_GET_OUTPUT, (void*)arg);
  type = (enum disp_output_type)output.type;
  mode = (enum disp_tv_mode)output.mode;
  PRINTD(type);
  PRINTD(mode);
}

void device_switch(int dispfd) {
  printf("切换\n");
  unsigned long arg[3];
  arg[0] = 0;
  arg[1] = (unsigned long)DISP_OUTPUT_TYPE_LCD;
  arg[2] = 0;
  int result = ioctl(dispfd, DISP_DEVICE_SWITCH, (void*)arg);
  PRINTD(result);
  // 说明：如果传递的type 是DISP_OUTPUT_TYPE_NONE，将会关闭当前显示通道的输出
}

void set_bkcolor(int dispfd) {
  printf("设置显示背景色，dispfd 为显示驱动句柄，sel 为屏0/1\n");
  struct disp_color bk;
  unsigned long arg[3];
  bk.red = 0xff;
  bk.green = 0x00;
  bk.blue = 0x00;
  arg[0] = 0;
  arg[1] = (uintptr_t)&bk;
  int result = ioctl(dispfd, DISP_SET_BKCOLOR, (void*)arg);
  PRINTD(result);
}

void device_get_config(int dispfd) {
  printf("获取当前输出类型及相关的属性参数\n");
  unsigned long arg[3];
  struct disp_device_config config;
  arg[0] = 0;
  arg[1] = (unsigned long)&config;
  int result = ioctl(dispfd, DISP_DEVICE_GET_CONFIG, (void*)arg);
  PRINTD(result);
  PRINTD(config.type);
  PRINTD(config.mode);
  PRINTD(config.format);
  PRINTD(config.bits);
  PRINTD(config.eotf);
  PRINTD(config.cs);
  PRINTD(config.dvi_hdmi);
  PRINTD(config.range);
  PRINTD(config.scan);
  PRINTD(config.aspect_ratio);
  // 说明：如果返回的type 是DISP_OUTPUT_TYPE_NONE，表示当前输出显示通道为关闭状态
}

// wrong docs describing the old 32-bit interface... yikes
void disp_layer_get_config(int dispfd) {
  printf("获取图层参数，dispfd为显示驱动句柄\n");
  unsigned long arg[3];
  struct disp_layer_config config;
  // the buffer has to be zeroed. it seems ioctl is using the indices in the config struct
  // to retrieve corresponding layers, rather than copying them sequentially...
  memset(&config, 0, sizeof(struct disp_layer_config));
  arg[0] = 0;//screen 0
  arg[1] = (uintptr_t)&config;
  arg[2] = 1;//cnt
  int ret = ioctl(dispfd, DISP_LAYER_GET_CONFIG, (void*)arg);
  PRINTD(ret);
  PRINTD(config.channel);
  PRINTD(config.layer_id);
  PRINTD(config.enable);
  PRINTD(config.info.mode);
}

// works. can map back the framebuffer memory!
void disp_layer_set_config(int dispfd, uint32_t* mem_in) {
  printf("设置图层参数，dispfd 为显示驱动句柄\n");
  unsigned long arg[3];
  struct disp_layer_config config;
  unsigned int width = 480;
  unsigned int height = 1280;
  unsigned int ret = 0;
  memset(&config, 0, sizeof(struct disp_layer_config));
  config.channel = 0;//blending channel
  config.layer_id = 0;//layer index in the blending channel
  config.enable = 1;
  config.info.mode = LAYER_MODE_BUFFER;
  config.info.fb.addr[0] = (unsigned long long)mem_in; //FB 地址
  config.info.fb.size[0].width = width;
  config.info.fb.align[0] = 4;//bytes
  config.info.fb.format = DISP_FORMAT_ARGB_8888; //DISP_FORMAT_YUV420_P
  config.info.fb.crop.x = 0;
  config.info.fb.crop.y = 0;
  config.info.fb.crop.width = ((unsigned long)width) << 32;//定点小数。高32bit 为整数，低32bit 为小数
  config.info.fb.crop.height= ((unsigned long)height)<<32;//定点小数。高32bit 为整数，低32bit 为小数
  config.info.fb.flags = DISP_BF_NORMAL;
  config.info.fb.scan = DISP_SCAN_PROGRESSIVE;
  config.info.alpha_mode = 1; //global pixel alpha
  config.info.alpha_value = 0xff;//global alpha value
  config.info.screen_win.x = 0;
  config.info.screen_win.y = 0;
  config.info.screen_win.width = width;
  config.info.screen_win.height= height;
  config.info.id = 0;
  config.info.b_trd_out = 0;
  config.info.zorder = 99; // looks like there's another active layer pointing to the framebuffer memory
  arg[0] = 0;//screen 0
  arg[1] = (unsigned long)&config;
  arg[2] = 1; //one layer
  ret = ioctl(dispfd, DISP_LAYER_SET_CONFIG, (void*)arg);
  PRINTD(ret);
}

void fb_info(int fbfd, unsigned long *paddr, __u32 *psize) {
  struct fb_var_screeninfo fb_var;
  struct fb_fix_screeninfo fb_fix;

  if (ioctl(fbfd, FBIOGET_VSCREENINFO, &fb_var) < 0
      || ioctl(fbfd, FBIOGET_FSCREENINFO, &fb_fix) < 0) {
    printf("ioctl fail\n");
  }

  PRINTLX(fb_fix.smem_start);
  PRINTD(fb_fix.smem_len);

  *paddr = fb_fix.smem_start;
  *psize = fb_fix.smem_len;
}

int sunxi_g2d_fill(int g2dfd,
                   void* paddr,
                   int x,
                   int y,
                   int w,
                   int h,
                   uint32_t color)
{
    g2d_fillrect tmp;

    if (w <= 0 || h <= 0)
        return 0;

    tmp.flag                = G2D_FIL_NONE;
    tmp.dst_image.addr[0]   = paddr;
    tmp.dst_image.w         = disp->xres;
    tmp.dst_image.h         = disp->framebuffer_height;
    tmp.dst_image.format    = G2D_FMT_ARGB_AYUV8888;
    tmp.dst_image.pixel_seq = G2D_SEQ_NORMAL;
    tmp.dst_rect.x          = x;
    tmp.dst_rect.y          = y;
    tmp.dst_rect.w          = w;
    tmp.dst_rect.h          = h;
    tmp.color               = color;
    tmp.alpha               = 0;

    return ioctl(g2dfd, G2D_CMD_FILLRECT, &tmp);
}


int main() {
  int dispfd = open("/dev/disp", O_RDWR);
  printf("dispfd=%d\n", dispfd);
  if (dispfd < 0) {
    exit(-1);
  }

  int fbfd = open("/dev/fb0", O_RDWR);
  printf("fbfd=%d\n", fbfd);
  if (fbfd < 0) {
    close(dispfd);
    exit(-1);
  }

  unsigned long paddr, paddr_2;
  unsigned int psize;
  fb_info(fbfd, &paddr, &psize);
  paddr_2 = paddr + 1280 * 480 * 4;

  scn_size(dispfd);
  // output_type(dispfd);
  device_switch(dispfd);
  output_type(dispfd);
  // system("dmesg | tail");
  // set_bkcolor(dispfd);
  device_get_config(dispfd);
  disp_layer_get_config(dispfd);

  uint32_t* mem_in = mmap(0, psize, PROT_READ | PROT_WRITE, MAP_SHARED, fbfd, 0);
  uint32_t* mem_in2 = mem_in + 1280*480;
  // uint32_t* mem_in = malloc(1280 * 480 * 4); <<- doesn't work. need to pass in physical addr
  for (int y = 0; y < 480; ++y) {
    for (int x = 0; x < 1280; ++x) {
      mem_in[x + y * 1280] = x & 0xFF + 0xFF000000;
      // mem_in[x + y * 480] = 0xFFFFFFFF;
      // mem_in[x + y * 480] = 0;
    }
  }

  disp_layer_set_config(dispfd, (uint32_t*)paddr_2);
  system("dmesg | tail");

  printf("anykey\n");
  getchar();

  g2d_fillrect(

  printf("anykey\n");
  getchar();

  close(fbfd);
  close(dispfd);
	return 0;
}
