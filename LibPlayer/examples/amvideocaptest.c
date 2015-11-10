/**************************************************
* example based on amcodec
**************************************************/
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <errno.h>
#include <stdbool.h>
#include <ctype.h>

#include <sys/mman.h>
#include <sys/ioctl.h>

#include <Amvideocaptools.h>

#define VIDEOCAPDEV "/dev/amvideocap0"
#define CAP_WIDTH_MAX      1920
#define CAP_HEIGHT_MAX     1080
int simmap_map_cap(char *buf, int size, int w, int h)
{
    int fd = open(VIDEOCAPDEV, O_RDWR);
    int ret;
    char *mbuf;
    if (fd < 0) {
        printf("open failed\n");
        return -1;
    }
    ioctl(fd, AMVIDEOCAP_IOW_SET_WANTFRAME_WIDTH, w);
    ioctl(fd, AMVIDEOCAP_IOW_SET_WANTFRAME_HEIGHT, h);
    ret = ioctl(fd, AMVIDEOCAP_IOW_SET_START_CAPTURE, 10000); //10000 max wait.
    printf("capture ok? %d\n", ret);
    mbuf = mmap(NULL, size, PROT_READ, MAP_SHARED, fd, 0);
    if (mbuf == NULL || mbuf == MAP_FAILED) {
        printf("mmap failed %x\n", buf);
        return -2;
    }
    if (buf && mbuf) {
        memcpy(buf, mbuf, size);
    }
    close(fd);
    return size;
}

int main(int argc, char **argv)
{
    char *filename;
    int w, h, needend;
    char *buf;
    int bufsize;
    int ret;
    int fd;
    int ret_size;
    int fmt = 2;
    int byte_per_pix;
    int format;
    if (argc < 2) {
        printf("usage:%s {file} <width> <height> <end> <format>\n", argv[1]);
        printf("          width&height=0 means used video own size\n");
        printf("          format:0 :S16_RGB565\n");
        printf("                     1 :S24_BGR\n");
        printf("                     2 :S24_RGB\n");
        printf("                     3 :S32_ARGB\n");

        printf("                     4 :S32_ABGR\n");
        printf("                     5 :S32_BGRA\n");
        printf("                     6 :S32_RGBA\n");
        printf("          format default S24_RGB\n");
        return 0;
    }
    w = h = needend = 0;
    filename = argv[1];
    if (argc >= 4) {
        sscanf(argv[2], "%d", &w);
        sscanf(argv[3], "%d", &h);
    }
    if (argc >= 5) {
        sscanf(argv[4], "%d", &needend);
    }
    if (argc >= 6) {
        sscanf(argv[5], "%d", &fmt);
    }
    if (w > CAP_WIDTH_MAX || h > CAP_HEIGHT_MAX) {
        printf("\nERROR (The width must smaller then %d, and the height must smaller then %d\n", CAP_WIDTH_MAX, CAP_HEIGHT_MAX);
        return -2;
    }
    if (fmt == 0) {
        byte_per_pix = 2;
    } else if (fmt < 3 && fmt > 0) {
        byte_per_pix = 3;
    } else if (fmt <= 6 && fmt >= 3) {
        byte_per_pix = 4;
    } else if (fmt > 6 || fmt < 0) {
        fmt = 2;
        byte_per_pix = 3;
    }
    if (w*h == 0) {
        bufsize = 1920 * 1088 * byte_per_pix;
        buf = malloc(1920 * 1088 * byte_per_pix);
        w = h = 0;

    } else {
        bufsize = w * h * byte_per_pix;
        buf = malloc(w * h * byte_per_pix);
    }
    if (!buf) {
        printf("malloc bufsize %d failed\n", bufsize);
        return -2;
    }

    switch (fmt) {
    case 0:
        format = FORMAT_S16_RGB_565;
        break;
    case 1:
        format = FORMAT_S24_BGR;
        break;
    case 2:
        format = FORMAT_S24_RGB;
        break;
    case 3:
        format = FORMAT_S32_ARGB;
        break;
    case 4:
        format = FORMAT_S32_ABGR;
        break;
    case 5:
        format = FORMAT_S32_BGRA;
        break;
    case 6:
        format = FORMAT_S32_RGBA;
        break;
    default:
        format = FORMAT_S24_RGB;
        break;
    }
    printf("start capture %d w=%d h=%d capendfram=%d,fmt=%d\n", bufsize, w, h, needend, fmt);
    ret = amvideocap_capframe(buf, bufsize, &w, &h, 0, needend, &ret_size, format);
    printf("finished capture %d,w=%d,h=%d\n", ret_size, w, h);
    if (ret < 0 || ret_size < 0) {
        printf("[ERROR] captrue failed\n");
        return -3;
    }
#if 1
    fd = open(filename, O_WRONLY | O_CREAT, 0644);

    if (fd < 0) {
        printf("create %s failed\n", filename);
        return -2;
    }
    write(fd, buf, ret_size);
    close(fd);
#endif

#if 0
    w = 800;
    h = 600;
    printf("1==start rect capture %d w=%d h=%d capendfram=%d\n", bufsize, w, h, needend);
    ret = amvideocap_capframe_with_rect(buf, bufsize, 50, 100, &w, &h, 0, needend, &ret_size);
    printf("finished rect capture %d,w=%d,h=%d\n", ret_size, w, h);
    if (ret < 0) {
        return -3;
    }

    char *rectname = malloc(strlen(filename) + 10);
    rectname[0] = 0;
    strcat(rectname, filename);
    strcat(rectname, ".rect");
    fd = open(rectname, O_WRONLY | O_CREAT, 0644);

    if (fd < 0) {
        printf("create %s failed\n", filename);
        return -2;
    }
    write(fd, buf, ret_size);
    close(fd);
#endif
#if 1
    ret = simmap_map_cap(buf, bufsize, w, h);
    printf("finished mam map capture %d\n", ret);
    char *mapname = malloc(strlen(filename) + 10);
    mapname[0] = 0;
    strcat(mapname, filename);
    strcat(mapname, ".mapcap");
    fd = open(mapname, O_WRONLY | O_CREAT, 0644);
    if (fd < 0) {
        printf("create %s failed\n", filename);
        return -2;
    }
    write(fd, buf, ret);
    close(fd);
    free(mapname);
#endif
    free(buf);
    return 0;
}
