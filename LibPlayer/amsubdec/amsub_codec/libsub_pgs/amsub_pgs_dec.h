#ifndef AMSUB_PGS_SUB_H
#define AMSUB_PGS_SUB_H

#include "amsub_dec.h"


typedef struct
{
    int start_time;
    int end_time;
    int x;
    int y;
    int width;
    int height;
    char fpsCode;
    int objectCount;

    char state;
    char palette_update_flag;
    char palette_Id_ref;
    char number;
    char item_flag; //cropped |=0x80, forced |= 0x40

    /**/
    int window_width_offset;
    int window_height_offset;
    int window_width;
    int window_height;
    /**/
    int image_width;
    int image_height;
    /**/
    int palette[0x100];
    unsigned char *rle_buf;
    int rle_buf_size;
    int rle_rd_off;
    /**/
} pgs_info_t;

typedef struct
{
    off_t pos;
    unsigned char *pgs_data_buf;
    int pgs_data_buf_size;
    unsigned char pgs_time_head_buf[13];
} pgs_buf_stru_t;

typedef struct
{
    int start_time;
    int end_time;
    off_t pos;
} pgs_item_t;

/* PGS subtitle API */
typedef struct
{
    int x;
    int y;
    int width;
    int height;

    int window_width_offset;
    int window_height_offset;
    int window_width;
    int window_height;

    int image_width;
    int image_height;

    int *palette;
    unsigned char *rle_buf;
    unsigned char *result_buf;
    int rle_buf_size;
    int render_height;
    int pts;
} PGS_subtitle_showdata;

typedef struct
{
    /* Add more members here */
    pgs_info_t *pgs_info;
    char *cur_idx_url;
    pgs_buf_stru_t pgs_buf_stru;

    int fd;
    off_t file_pos;
    pgs_item_t *pgs_item_table;
    int pgs_item_count;
    int pgs_display_index;

    /**/
    PGS_subtitle_showdata showdata;
    /*end*/
} subtitlepgs_t;

typedef struct draw_result
{
    unsigned char *buf;
    short x;
    short y;
    unsigned short w;
    unsigned short h;
} draw_result_t;
typedef int (*draw_pixel_fun_t)(int x, int y, unsigned pixel, void *arg);

typedef struct _VOB_SPUVAR
{
    unsigned short spu_color;
    unsigned short spu_alpha;
    unsigned short spu_start_x;
    unsigned short spu_start_y;
    unsigned short spu_width;
    unsigned short spu_height;
    unsigned short top_pxd_addr;  // CHIP_T25
    unsigned short bottom_pxd_addr; // CHIP_T25

    unsigned mem_start; // CHIP_T25
    unsigned mem_end; // CHIP_T25
    unsigned mem_size; // CHIP_T25
    unsigned mem_rp;
    unsigned mem_wp;
    unsigned mem_rp2;
    unsigned char spu_cache[8];
    int spu_cache_pos;
    int spu_decoding_start_pos; //0~OSD_HALF_SIZE*2-1, start index to vob_pixData1[0~OSD_HALF_SIZE*2]

    unsigned disp_colcon_addr;  // CHIP_T25
    unsigned char display_pending;
    unsigned char displaying;
    unsigned char reser[2];
} VOB_SPUVAR;

int pgs_subtitle_decode(amsub_dec_t *amsub_dec, int read_handle);

int init_pgs_subtitle();
#endif
