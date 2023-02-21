#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <time.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/time.h>
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

// gone in disp2. won't work
void disp_layer_get_info(int dispfd) {
  struct disp_layer_info layer_info;
  unsigned long arg[3];

  printf("disp_layer_get_info\n");

  arg[0] = 0; //screen
  arg[1] = 0; // layer
  arg[2] = (uintptr_t)&layer_info;
  int ret = ioctl(dispfd, DISP_LAYER_GET_INFO, arg);

  PRINTD(ret);
}

void device_switch(int dispfd, unsigned long type) {
  printf("切换\n");
  unsigned long arg[3];
  arg[0] = 0; // display id [0/1]
  arg[1] = type; // disp_output_type
  arg[2] = 0; // disp_tv_mode
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
  struct disp_layer_config config[4];
  // the buffer has to be zeroed. it seems ioctl is using the indices in the config struct
  // to retrieve corresponding layers, rather than copying them sequentially...
  memset(&config, 0, sizeof(config));
  for (int i=0;i<4;++i) {
    config[i].layer_id = i;
  }
  arg[0] = 0;//screen 0
  arg[1] = (uintptr_t)&config;
  arg[2] = 4;//cnt
  int ret = ioctl(dispfd, DISP_LAYER_GET_CONFIG, (void*)arg);
  PRINTD(ret);
  for (int i = 0; i < 4; ++i) {
    PRINTD(i);
    PRINTD(config[i].channel);
    PRINTD(config[i].layer_id);
    PRINTD(config[i].enable);
    PRINTD(config[i].info.mode);
    PRINTLX((uintptr_t)config[i].info.fb.addr);
  }
}

void disp_layer_disable(int dispfd) {
  printf("禁用图层0\n");
  unsigned long arg[3];
  struct disp_layer_config config;
  unsigned int ret = 0;
  memset(&config, 0, sizeof(struct disp_layer_config));
  config.channel = 0;//blending channel
  config.layer_id = 0;//layer index in the blending channel
  config.enable = 0;
  config.info.mode = LAYER_MODE_BUFFER;
  config.info.fb.align[0] = 4;//bytes
  config.info.id = 0;
  arg[0] = 0;//screen 0
  arg[1] = (unsigned long)&config;
  arg[2] = 1; //one layer
  ret = ioctl(dispfd, DISP_LAYER_SET_CONFIG, (void*)arg);
  PRINTD(ret);
}

// works. can map back the framebuffer memory!
void disp_layer_set_config(int dispfd, uint32_t* mem_fbfd_map) {
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
  config.info.fb.addr[0] = (unsigned long long)mem_fbfd_map; //FB 地址
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
    printf("screeninfo ioctl fail\n");
  }

  PRINTLX(fb_fix.smem_start);
  PRINTD(fb_fix.smem_len);

  *paddr = fb_fix.smem_start;
  *psize = fb_fix.smem_len;

  int gfx_layer_id0;
  int gfx_layer_id1;
  if (ioctl(fbfd, FBIOGET_LAYER_HDL_0, &gfx_layer_id0) < 0) {
    printf("ioctl layer hdl 0 fail\n");
  }
  PRINTD(gfx_layer_id0);
  if (ioctl(fbfd, FBIOGET_LAYER_HDL_1, &gfx_layer_id1) < 0) {
    printf("ioctl layer hdl 1 fail\n");
  }
  PRINTD(gfx_layer_id1);
}

void fillrect(int g2dfd,
             unsigned long paddr,
             int x,
             int y,
             int w,
             int h,
             uint32_t color)
{
    g2d_fillrect_h tmp;
    memset(&tmp, 0, sizeof(tmp));

    if (w <= 0 || h <= 0)
        return;

    tmp.dst_image_h.format = G2D_FORMAT_ARGB8888;
    tmp.dst_image_h.height = 1280;
    tmp.dst_image_h.width = 480;
    tmp.dst_image_h.color = color;
    tmp.dst_image_h.align[0] = 4;
    tmp.dst_image_h.laddr[0] = paddr;
    tmp.dst_image_h.alpha = 0xFF;
    tmp.dst_image_h.mode = G2D_PIXEL_ALPHA;
    tmp.dst_image_h.clip_rect.x = x;
    tmp.dst_image_h.clip_rect.y = y;
    tmp.dst_image_h.clip_rect.w = w;
    tmp.dst_image_h.clip_rect.h = h;
    tmp.dst_image_h.use_phy_addr = 1;

    int res = ioctl(g2dfd, G2D_CMD_FILLRECT_H, &tmp);
    /*PRINTD(res);*/
}

void fillrect_soft(uint32_t *vaddr, int x, int y, int w, int h, uint32_t color) {
  vaddr = vaddr + y * 480 + x;
  // CRLF wink wink
  int crlf = 480 - w;
  for (int i = 0; i < h; ++i) {
    for (int j = 0; j < w; ++j) {
      *vaddr++ = color;
    }
    vaddr += crlf;
  }
}

void rotate(int g2dfd,
    unsigned long src_paddr,
    unsigned long dst_paddr) {
  g2d_blt_h tmp;
  memset(&tmp, 0, sizeof(tmp));
  tmp.flag_h = G2D_ROT_90;
  tmp.src_image_h.use_phy_addr = 1;
  tmp.src_image_h.width = 1280;
  tmp.src_image_h.height = 480;
  tmp.src_image_h.align[0] = 4;
  tmp.src_image_h.laddr[0] = src_paddr;
  tmp.src_image_h.alpha = 0xFF;
  tmp.src_image_h.format = G2D_FORMAT_ARGB8888;
  tmp.src_image_h.mode = G2D_PIXEL_ALPHA;
  tmp.src_image_h.clip_rect.x = 0;
  tmp.src_image_h.clip_rect.y = 0;
  tmp.src_image_h.clip_rect.w = 1280;
  tmp.src_image_h.clip_rect.h = 480;

  tmp.dst_image_h.use_phy_addr = 1;
  tmp.dst_image_h.width = 480;
  tmp.dst_image_h.height = 1280;
  tmp.dst_image_h.align[0] = 4;
  tmp.dst_image_h.laddr[0] = dst_paddr;
  tmp.dst_image_h.alpha = 0xFF;
  tmp.dst_image_h.format = G2D_FORMAT_ARGB8888;
  tmp.dst_image_h.mode = G2D_PIXEL_ALPHA;
  tmp.dst_image_h.clip_rect.x = 0;
  tmp.dst_image_h.clip_rect.y = 0;
  tmp.dst_image_h.clip_rect.w = 480;
  tmp.dst_image_h.clip_rect.h = 1280;

  int res = ioctl(g2dfd, G2D_CMD_BITBLT_H, &tmp);
  /*PRINTD(res);*/
}

void bitblt(int g2dfd,
    unsigned long paddr,
    int src_x, int src_y, int dst_x, int dst_y, int w, int h) {
  g2d_blt_h tmp;
    memset(&tmp, 0, sizeof(tmp));
    //if (src_is_shadow && dst_is_shadow) {
    tmp.flag_h = G2D_ROT_0;
    // tmp.flag_h = 0;
    tmp.src_image_h.use_phy_addr = 1;
    tmp.src_image_h.width = 480;
    tmp.src_image_h.height = 1280;
    tmp.src_image_h.align[0] = 4;
    tmp.src_image_h.laddr[0] = paddr;
    tmp.src_image_h.alpha = 0xFF;
    tmp.src_image_h.format = G2D_FORMAT_ARGB8888;
    tmp.src_image_h.mode = G2D_PIXEL_ALPHA;
    tmp.src_image_h.clip_rect.x = src_x;
    tmp.src_image_h.clip_rect.y = src_y;
    tmp.src_image_h.clip_rect.w = w;
    tmp.src_image_h.clip_rect.h = h;

    tmp.dst_image_h.use_phy_addr = 1;
    tmp.dst_image_h.width = 480;
    tmp.dst_image_h.height = 1280;
    tmp.dst_image_h.align[0] = 4;
    tmp.dst_image_h.laddr[0] = paddr;
    tmp.dst_image_h.alpha = 0xFF;
    tmp.dst_image_h.format = G2D_FORMAT_ARGB8888;
    tmp.dst_image_h.mode = G2D_PIXEL_ALPHA;
    tmp.dst_image_h.clip_rect.x = dst_x;
    tmp.dst_image_h.clip_rect.y = dst_y;
    tmp.dst_image_h.clip_rect.w = w;
    tmp.dst_image_h.clip_rect.h = h;

    ioctl(g2dfd, G2D_CMD_BITBLT_H, &tmp);
}


void rotate_soft(uint32_t* src, uint32_t* dst) {
  for (int i = 0; i < 1280; ++i) {
    for (int j = 0; j < 480; ++j) {
      dst[i * 480 + j] = src[j * 1280 + i];
    }
  }
}

void fill_test(uint32_t* vaddr, int g2dfd, unsigned long paddr, int fill_w, int fill_h, int fill_n) {
  struct timeval t0, t1;
  double elapsed;

  int rand_xmax = 480 - fill_w;
  int rand_ymax = 1280 - fill_h;

  printf(" ========== fill_test, w=%d h=%d n=%d =========== \n", fill_w, fill_h, fill_n);

  printf(" >>> software fillrect: ");
  gettimeofday(&t0, NULL);
  for (int __x = 0; __x < fill_n; ++__x) {
    int x = rand() % (rand_xmax);
    int y = rand() % (rand_ymax);
    int color_r = (rand() & 0xff) << 16;
    int color_g = (rand() & 0xff) << 8;
    int color_b = rand() & 0xff;
    fillrect_soft(vaddr, x, y, fill_w, fill_h, 0xFF000000 | color_r | color_g | color_b);
  }
  gettimeofday(&t1, NULL);
  elapsed = (t1.tv_sec - t0.tv_sec) * 1000.0;
  elapsed += (t1.tv_usec - t0.tv_usec) / 1000.0;
  printf("TIME = %lfms.\n", elapsed);


  printf(" >>> hardware fillrect: ");
  gettimeofday(&t0, NULL);
  for (int __x = 0; __x < fill_n; ++__x) {
    int x = rand() % (rand_xmax);
    int y = rand() % (rand_ymax);
    int color_r = (rand() & 0xff) << 16;
    int color_g = (rand() & 0xff) << 8;
    int color_b = rand() & 0xff;
    fillrect(g2dfd, paddr, x, y, fill_w, fill_h, 0xFF000000 | color_r | color_g | color_b);
  }
  gettimeofday(&t1, NULL);
  elapsed = (t1.tv_sec - t0.tv_sec) * 1000.0;
  elapsed += (t1.tv_usec - t0.tv_usec) / 1000.0;
  printf("TIME = %lfms.\n", elapsed);

}

void rotate_test(uint32_t *mem_fbfd_map, uint32_t *mem_shadow,
    unsigned long paddr, unsigned long paddr_2, int g2dfd) {
  struct timeval t0, t1;
  double elapsed;

  int nframe = 60;

  printf(" ========== rotate_test =========== \n");
  printf(" >>> software rotate: ");
  gettimeofday(&t0, NULL);
  for(int i = 0; i < nframe; ++i) {
    rotate_soft(mem_fbfd_map, mem_shadow);
  }
  gettimeofday(&t1, NULL);
  elapsed = (t1.tv_sec - t0.tv_sec) * 1000.0;
  elapsed += (t1.tv_usec - t0.tv_usec) / 1000.0;
  printf("TIME = %lfms. FPS = %lf.\n", elapsed, nframe / (elapsed / 1000.0));

  printf(" >>> hardware rotate: ");
  gettimeofday(&t0, NULL);
  for(int i = 0; i < nframe; ++i) {
    rotate(g2dfd, paddr, paddr_2);
  }
  gettimeofday(&t1, NULL);
  elapsed = (t1.tv_sec - t0.tv_sec) * 1000.0;
  elapsed += (t1.tv_usec - t0.tv_usec) / 1000.0;
  printf("TIME = %lfms. FPS = %lf.\n", elapsed, nframe / (elapsed / 1000.0));
}

#define WHITE 0xffffffff
#define BLACK 0xff000000
#define RED   0xffff0000
#define GREEN 0xff00ff00
#define BLUE  0xff0000ff

void bitblt_test(int g2dfd, uint32_t* mem_fbfd_map, unsigned long paddr, int x0, int y0, int w, int h, int delta_x, int delta_y) {
  for (int y = 0; y < 480; ++y) {
    for (int x = 0; x < 1280; ++x) {
      mem_fbfd_map[x + y * 1280] = WHITE;
    }
  }
  for (int y = y0; y < y0 + h; ++y) {
    for (int x = x0; x < x0 + w; ++x) {
      mem_fbfd_map[x + y * 480] = (((x+y) & 0xFF) << 8) + ~((unsigned char)(x & 0xFF)) + 0xFF000000;
    }
  }
  printf("==== before ==== (anykey)\n");
  getchar();
  bitblt(g2dfd, paddr, x0, y0, x0 + delta_x, y0 + delta_y, w, h);
  printf("==== after ==== (anykey)\n");
  getchar();
}

int main() {
  int dispfd = open("/dev/disp", O_RDWR);
  printf("dispfd=%d\n", dispfd);
  if (dispfd < 0) {
    exit(-1);
  }

  int step_fds = 0;
  
  if (step_fds) {
    printf("dispfd is now open. anykey\n");
    getchar();
  }


  int fbfd = open("/dev/fb0", O_RDWR);
  printf("fbfd=%d\n", fbfd);
  if (fbfd < 0) {
    close(dispfd);
    exit(-1);
  }
  if (step_fds) {
    printf("fbfd is now open. anykey\n");
    getchar();
  }

  int g2dfd = open("/dev/g2d", O_RDWR);
  printf("g2dfd=%d\n", fbfd);
  if (g2dfd < 0) {
    close(fbfd);
    close(dispfd);
    exit(-1);
  }

  if (step_fds) {
    printf("g2dfd is now open. anykey\n");
    getchar();
  }


  unsigned long paddr, paddr_2;
  unsigned int psize;
  fb_info(fbfd, &paddr, &psize);
  paddr_2 = paddr + 1280 * 480 * 4;
  uint32_t* mem_fbfd_map;
  uint32_t* mem_shadow;

  scn_size(dispfd);
  // device_switch(dispfd, (unsigned long)DISP_OUTPUT_TYPE_LCD);
  output_type(dispfd);
  // system("dmesg | tail");
  // set_bkcolor(dispfd);
  device_get_config(dispfd);
  disp_layer_get_config(dispfd);
  // disp_layer_get_info(dispfd);
  //
  // printf("anykey\n");
  // getchar();

  if (1) {
    mem_fbfd_map = mmap(0, psize, PROT_READ | PROT_WRITE, MAP_SHARED, fbfd, 0);
    mem_shadow = mem_fbfd_map + 1280*480;
    // uint32_t* mem_fbfd_map = malloc(1280 * 480 * 4); <<- doesn't work. need to pass in physical addr
    for (int y = 0; y < 480; ++y) {
      for (int x = 0; x < 1280; ++x) {
        /*mem_fbfd_map[x + y * 1280] = ((x & 0xFF) << 8) + 0xFF000000;*/
        /*mem_shadow[x + y * 1280] = ((x & 0xFF) << 8) + 0xFF000000;*/
        // mem_fbfd_map[x + y * 480] = 0xFFFFFFFF;
        mem_fbfd_map[x + y * 1280] = 0xFFFFFFFF;
      }
    }
  }
  if (1) {
    disp_layer_set_config(dispfd, (uint32_t*)paddr);
    system("dmesg | tail");
    printf("screen is initialized\n");
    printf("anykey\n");
    getchar();
  }

  if (0) {
    printf("anykey to proceed to fillrect test\n");
    getchar();

    srand(time(NULL));
    fill_test(mem_fbfd_map, g2dfd, paddr, 10, 10, 10000);
    fill_test(mem_fbfd_map, g2dfd, paddr, 30, 30, 10000);
    fill_test(mem_fbfd_map, g2dfd, paddr, 50, 50, 10000);
    fill_test(mem_fbfd_map, g2dfd, paddr, 70, 70, 10000);
    fill_test(mem_fbfd_map, g2dfd, paddr, 90, 90, 10000);
    fill_test(mem_fbfd_map, g2dfd, paddr, 100, 100, 10000);
    fill_test(mem_fbfd_map, g2dfd, paddr, 100, 100, 10000);
    fill_test(mem_fbfd_map, g2dfd, paddr, 200, 200, 10000);
    fill_test(mem_fbfd_map, g2dfd, paddr, 300, 300, 10000);

    printf("anykey\n");
    getchar();
  }

  // note: in-place rotation is not supported.
  if (0) {
    rotate_test(mem_shadow, mem_fbfd_map, paddr_2, paddr, g2dfd);
    printf("anykey\n");
    getchar();
  }

  /* SIZE(432,808) ORIGIN(i,217) OFFSET(8,0) results:
   * 0..8
   * 9..15 X
   * 16..24
   * 25..31 X
   * 32..40
   * 41..47 X
   * 48..56
   * 57..63 X
   * ...
   * X + 1 doesn't change the pattern.
   * Width + 1 doesn't change the pattern.
   * Delta X + 1 DOES change the pattern.
   * TODO Moving Y?
   *
   * ... OFFSET(9,0) results:
   * 0..7
   * 8..15 X
   * ...
   *
   * ... OFFSET(1,0) results:
   * 0..255 OK
   *
   * ... OFFSET(2,0) results:
   * 0..14 OK
   * 15 X
   * 16..30 OK
   * 31 X
   *
   * ... OFFSET(14,0) results:
   * 0..2 OK
   * 3..15 X
   *
   * ... OFFSET(16,0) results:
   * 0 OK
   * 1..15 X
   *
   * ... OFFSET(17,0) results:
   * 0..255 X
   *
   * ... Negative X offset ALL OK.
   * ... Applying both X & Y offsets destroys the pattern.
   */
  for (int i = 0; i < 100; ++i) {
	  printf("x = %d\n", i);
	  bitblt_test(g2dfd, mem_fbfd_map, paddr, i, 219, 432, 808, 18, 0);
  }

  printf("bitblt test END. anykey\n");
  getchar();

  system("dmesg | tail");

  // release
  if (0) {
  // device_switch(dispfd, (unsigned long)DISP_OUTPUT_TYPE_NONE);
  }
  disp_layer_disable(dispfd);
  close(fbfd);
  close(dispfd);
  close(g2dfd);
	return 0;
}
