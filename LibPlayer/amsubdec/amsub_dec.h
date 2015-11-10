#ifndef AMSUB_DEC_H
#define AMSUB_DEC_H

#include <pthread.h>
#include <list.h>
//#include "libavutil/avutil.h"

#ifndef ARRAY_SIZE
#define ARRAY_SIZE(x)  (sizeof(x) / sizeof((x)[0]))
#endif

#define lock_t            pthread_mutex_t
#define lp_lock_init(x,v)     pthread_mutex_init(x,v)
#define lp_lock(x)        pthread_mutex_lock(x)
#define lp_unlock(x)       pthread_mutex_unlock(x)



#define VOB_SUB_WIDTH 1920
#define VOB_SUB_HEIGHT 1280
#define VOB_SUB_SIZE VOB_SUB_WIDTH*VOB_SUB_HEIGHT/4
#define DVB_SUB_SIZE VOB_SUB_WIDTH*VOB_SUB_HEIGHT

#define SUBTITLE_VOB      1
#define SUBTITLE_PGS      2
#define SUBTITLE_MKV_STR  3
#define SUBTITLE_MKV_VOB  4
#define SUBTITLE_SSA  5     //add yjf
#define SUBTITLE_DVB  6
#define SUBTITLE_TMD_TXT 7
#define SUBTITLE_IDX_SUB 8


struct subdata_s
{
    list_t  list;            /* head node of subtitle_t list */
    list_t  list_temp;

    int     sub_num;
    int     sub_error;
    int     sub_format;     /*subtitle type*/

    lock_t amsub_lock;
};

//typedef struct subtitle_s subtitle_t;
typedef struct subdata_s subdata_t;

typedef struct
{
    int sub_type;
    int64_t startTimeUs;
    int64_t endTimeUs;
    unsigned int mesc;     //for idx+sub
    unsigned int next_pts; //for idx+sub

    unsigned sync_bytes;
    unsigned buffer_size;
    unsigned pid;
    unsigned int pts;
    unsigned int m_delay;
    //unsigned char *spu_data;
    unsigned char *amsub_data;
    unsigned short cmd_offset;
    unsigned short length;

    unsigned r_pt;
    unsigned frame_rdy;

    unsigned short spu_color;   // color
    unsigned short spu_alpha;   // angle
    unsigned short spu_start_x;
    unsigned short spu_start_y;
    unsigned short spu_width;
    unsigned short spu_height;
    unsigned short top_pxd_addr;
    unsigned short bottom_pxd_addr;

    unsigned disp_colcon_addr;
    unsigned char display_pending;
    unsigned char displaying;
    //unsigned char subtitle_type;
    unsigned char reser[2];

    unsigned rgba_enable;
    unsigned rgba_background;
    unsigned rgba_pattern1;
    unsigned rgba_pattern2;
    unsigned rgba_pattern3;

    int resize_height;
    int resize_width;
    int resize_xstart;
    int resize_ystart;
    int resize_size;

    list_t      list;
} amsub_para_s;


typedef struct
{

    //int sub_num;
    subdata_t *amsub_data_l;
    amsub_para_s *amsub_p;

} amsub_para_t;



typedef struct
{
    int sub_type;
    int sub_pid;
    int stream_type;  // to judge sub how to get data
    char *sub_filename;
    unsigned int curr_timeMs; //for idx+sub
    unsigned int next_pts; //for idx+sub

    unsigned int pts;
    unsigned int m_delay;
    unsigned short sub_start_x;
    unsigned short sub_start_y;
    unsigned short sub_width;
    unsigned short sub_height;
    char *odata;     // point to decoder data
    unsigned buffer_size;
} amsub_info_t;

typedef enum
{
    STREAM_TYPE_UNKNOW,
    STREAM_TYPE_ES_VIDEO,
    STREAM_TYPE_ES_AUDIO,
    STREAM_TYPE_ES_SUB,
    STREAM_TYPE_PS,
    STREAM_TYPE_TS,
    STREAM_TYPE_RM,
} stream_type_t;


typedef enum
{
    IDLE = 0,
    INITING,
    INITTED,
    STARTED,
    RUNNING,
    STOPPED,
    TERMINATED
} amsub_state_t;


typedef struct
{
    pthread_mutex_t  pthread_mutex;
    pthread_cond_t   pthread_cond;
    pthread_t        pthread_id;
} amsub_dec_thread_t;


/* subtitle decoder operation*/
typedef struct amsub_dec_opt amsub_dec_opt_t;
struct amsub_dec_opt
{
    int nInBufSize;
    int nOutBufSize;
    int (*init)(amsub_dec_opt_t *);
    int (*decode)(amsub_dec_opt_t *);
    int (*release)(amsub_dec_opt_t *);
    int (*getinfo)(amsub_dec_opt_t *, amsub_info_t *);
    void *priv_data; //point to amsub
    void *priv_dec_data; //decoder private data
    void *pdecoder; // decoder instance
    int amsub_handle;

    // int extradata_size;      ///< extra data size
    // char extradata[AUDIO_EXTRA_DATA_SIZE];

};


typedef struct
{
    int sub_type;
    int sub_pid;
    int sub_size;
    char *sub_filename;  //for idx+sub
    //int sub_pts;
    char sub_header[20];
    char *sub_idata;
    amsub_dec_opt_t *amsub_ops;

    void *priv_sub;
    amsub_state_t mstate;
    stream_type_t stream_type;
    amsub_para_t *amsub_para;

    amsub_dec_thread_t amsub_thread;
    pthread_t amsub_dec_ctrl_tid;
    pthread_t amsub_dec_tid;

    int need_stop;


} amsub_dec_t;

typedef enum
{
    /* subtitle codecs */
    CODEC_ID_DVD_SUBTITLE = 0x17000,
    CODEC_ID_DVB_SUBTITLE,
    CODEC_ID_TEXT,  ///< raw UTF-8 text
    CODEC_ID_XSUB,
    CODEC_ID_SSA,
    CODEC_ID_MOV_TEXT,
    CODEC_ID_HDMV_PGS_SUBTITLE,
    CODEC_ID_DVB_TELETEXT,
    CODEC_ID_SRT,
    CODEC_ID_MICRODVD,
    //CODEC_ID_SUBRIP   = MKBETAG('S','R','i','p'),
    //CODEC_ID_ASS      = MKBETAG('A','S','S',' '),  ///< ASS as defined in Matroska
    CODEC_ID_IDX_SUB = 0x17010
} subtitle_id;


#define SUBTITLE_READ_DEVICE    "/dev/amstream_sub_read"




#endif
