/**
* @file kplayer.c
* @brief
* @author Xu Hui <hui.xu@amlogic.com>
* @version 1.0.0
* @date 2011-02-28
*/
/*!< Copyright (C) 2007-2011, Amlogic Inc.
* All right reserved
*
*/
#include <stdlib.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include <cutils/log.h>


#include "player.h"
#include "player_type.h"
#include "streamsource.h"
#include "Amsysfsutils.h"
typedef enum {
    //EMU_STEP_NONE = 0,
    EMU_STEP_PAUSE = 2,
    EMU_STEP_RESUME = 3,
    EMU_STEP_START = 4,
    EMU_STEP_FF = 5,
    EMU_STEP_RR = 6,
    EMU_STEP_SEEK = 7,
    //EMU_STEP_MUTE= 8,
    //EMU_STEP_SETVOL=9,
    //EMU_STEP_GETVOL = 10,
    //EMU_STEP_SETTONE= 11,
    EMU_STEP_SETLOOP = 14,
    EMU_STEP_STOP = 16,
    //EMU_STEP_SPECTRUM = 17,
    //EMU_STEP_SETSUBT = 19,
    EMU_STEP_MENU = 20,
    EMU_STEP_EXIT = 21,
    //EMU_STEP_ATRACK = 22,
    EMU_STEP_GETAVMEDIAINFO = 25,
    //EMU_STEP_LISTALLMEDIAID = 27,
} EMU_STEP;
#define SCREEN_SPLITER            "***************************************************************************\r\n"

static int axis[8] = {0};

int update_player_info(int pid, player_info_t * info)
{
    ALOGD("pid:%d,status:%x,current pos:%d,total:%d,errcode:%x\n", pid, info->status, info->current_time, info->full_time, ~(info->error_no));
    return 0;
}
int _media_info_dump(media_info_t* minfo)
{
    int i = 0;
    ALOGD("@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@\n");
    ALOGD("======||file size:%lld\n", minfo->stream_info.file_size);
    ALOGD("======||file type:%d\n", minfo->stream_info.type);
    ALOGD("======||has internal subtitle?:%s\n", minfo->stream_info.has_sub > 0 ? "YES!" : "NO!");
    ALOGD("======||internal subtile counts:%d\n", minfo->stream_info.total_sub_num);
    ALOGD("======||has video track?:%s\n", minfo->stream_info.has_video > 0 ? "YES!" : "NO!");
    ALOGD("======||has audio track?:%s\n", minfo->stream_info.has_audio > 0 ? "YES!" : "NO!");
    ALOGD("======||duration:%d\n", minfo->stream_info.duration);
    if (minfo->stream_info.has_video && minfo->stream_info.total_video_num > 0) {
        ALOGD("======||video counts:%d\n", minfo->stream_info.total_video_num);
        ALOGD("======||video width:%d\n", minfo->video_info[0]->width);
        ALOGD("======||video height:%d\n", minfo->video_info[0]->height);
        ALOGD("======||video bitrate:%d\n", minfo->video_info[0]->bit_rate);
        ALOGD("======||video format:%d\n", minfo->video_info[0]->format);

    }
    if (minfo->stream_info.has_audio && minfo->stream_info.total_audio_num > 0) {
        ALOGD("======||audio counts:%d\n", minfo->stream_info.total_audio_num);

        if (NULL != minfo->audio_info[0]->audio_tag) {
            ALOGD("======||track title:%s", minfo->audio_info[0]->audio_tag->title != NULL ? minfo->audio_info[0]->audio_tag->title : "unknow");
            ALOGD("\n======||track album:%s", minfo->audio_info[0]->audio_tag->album != NULL ? minfo->audio_info[0]->audio_tag->album : "unknow");
            ALOGD("\n======||track author:%s\n", minfo->audio_info[0]->audio_tag->author != NULL ? minfo->audio_info[0]->audio_tag->author : "unknow");
            ALOGD("\n======||track year:%s\n", minfo->audio_info[0]->audio_tag->year != NULL ? minfo->audio_info[0]->audio_tag->year : "unknow");
            ALOGD("\n======||track comment:%s\n", minfo->audio_info[0]->audio_tag->comment != NULL ? minfo->audio_info[0]->audio_tag->comment : "unknow");
            ALOGD("\n======||track genre:%s\n", minfo->audio_info[0]->audio_tag->genre != NULL ? minfo->audio_info[0]->audio_tag->genre : "unknow");
            ALOGD("\n======||track copyright:%s\n", minfo->audio_info[0]->audio_tag->copyright != NULL ? minfo->audio_info[0]->audio_tag->copyright : "unknow");
            ALOGD("\n======||track track:%d\n", minfo->audio_info[0]->audio_tag->track);
        }



        for (i = 0; i < minfo->stream_info.total_audio_num; i++) {
            ALOGD("^^^^^^^^^^^^^^^^^^^^^^^^^^^^^\n");
            ALOGD("======||%d 'st audio track codec type:%d\n", i, minfo->audio_info[i]->aformat);
            ALOGD("======||%d 'st audio track audio_channel:%d\n", i, minfo->audio_info[i]->channel);
            ALOGD("======||%d 'st audio track bit_rate:%d\n", i, minfo->audio_info[i]->bit_rate);
            ALOGD("======||%d 'st audio track audio_samplerate:%d\n", i, minfo->audio_info[i]->sample_rate);
            ALOGD("^^^^^^^^^^^^^^^^^^^^^^^^^^^^^\n");

        }

    }
    if (minfo->stream_info.has_sub && minfo->stream_info.total_sub_num > 0) {
        for (i = 0; i < minfo->stream_info.total_sub_num; i++) {
            if (0 == minfo->sub_info[i]->internal_external) {
                ALOGD("^^^^^^^^^^^^^^^^^^^^^^^^^^^^^\n");
                ALOGD("======||%d 'st internal subtitle pid:%d\n", i, minfo->sub_info[i]->id);
                ALOGD("======||%d 'st internal subtitle language:%s\n", i, minfo->sub_info[i]->sub_language ? minfo->sub_info[i]->sub_language : "unknow");
                ALOGD("======||%d 'st internal subtitle width:%d\n", i, minfo->sub_info[i]->width);
                ALOGD("======||%d 'st internal subtitle height:%d\n", i, minfo->sub_info[i]->height);
                ALOGD("======||%d 'st internal subtitle resolution:%d\n", i, minfo->sub_info[i]->resolution);
                ALOGD("======||%d 'st internal subtitle subtitle size:%lld\n", i, minfo->sub_info[i]->subtitle_size);
                ALOGD("^^^^^^^^^^^^^^^^^^^^^^^^^^^^^\n");
            }
        }
    }
    ALOGD("@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@\n");
    return 0;
}
static int get_axis(const char *para, int para_num, int *result)
{
    char *endp;
    const char *startp = para;
    int *out = result;
    int len = 0, count = 0;

    if (!startp) {
        return 0;
    }

    len = strlen(startp);

    do {
        //filter space out
        while (startp && (isspace(*startp) || !isgraph(*startp)) && len) {
            startp++;
            len--;
        }

        if (len == 0) {
            break;
        }

        *out++ = strtol(startp, &endp, 0);

        len -= endp - startp;
        startp = endp;
        count++;

    } while ((endp) && (count < para_num) && (len > 0));

    return count;
}
static int set_display_axis(int recovery)
{
    int fd, fd1, fd2;
    int fd_video_axis, fd_video_disable, fd_video_screenmode, fd_freescale_fb1, fd_freescale_fb0;
    char *path1 = "/sys/class/graphics/fb0/blank";
    char *path2 = "/sys/class/graphics/fb1/blank";

    char *videoaxis_patch = "/sys/class/video/axis";
    char *videodisable_patch = "/sys/class/video/disable_video";
    char *videoscreenmode_patch = "/sys/class/video/screen_mode";
    char str[128];
    int count, i;

    amsysfs_set_sysfs_str(path1, "1");
    amsysfs_set_sysfs_str(path2, "1");
    amsysfs_set_sysfs_str(videoaxis_patch, "0 0 -1 -1");
    amsysfs_set_sysfs_str(videoscreenmode_patch, "1");
    amsysfs_set_sysfs_str(videodisable_patch, "2");
    return 0;

}

static void signal_handler(int signum)
{
    ALOGD("Get signum=%x\n", signum);
    player_progress_exit();
    signal(signum, SIG_DFL);
    raise(signum);
}

#define TMP_COMMAND_MAX 512

int main(int argc, char *argv[])
{
    play_control_t *pCtrl = NULL;
    int pid;
    int pos = 0;
    int speed = 0;
    int tmpneedexit = 0;
    int ret = -1;
    int fd_di_bypass_all = -1;
    media_info_t minfo;
    int osd_is_blank = 0;
    char tmpcommand[TMP_COMMAND_MAX];
    char newframestring[30] = {0};
    char newstring0[30] = {0};
    int new_frame_count = 0;
    EMU_STEP tmpstep = EMU_STEP_MENU;
    char *di_bypass_all_path = "/sys/module/di/parameters/bypass_all";
    pCtrl = (play_control_t*)malloc(sizeof(play_control_t));
    memset(pCtrl, 0, sizeof(play_control_t));
    memset(&minfo, 0, sizeof(media_info_t));
    if (argc < 2) {
        ALOGD("usage:player file\n");
        return -1;
    }
    amsysfs_set_sysfs_str(di_bypass_all_path, "1");

    player_init();
    streamsource_init();
    //set_display_axis(0);      //move osd out of screen to set video layer out

    player_register_update_callback(&pCtrl->callback_fn, &update_player_info, 1000);
    ALOGD("player callback register....\n");

    pCtrl->file_name = strdup(argv[1]);

    //pCtrl->nosound = 1;   // if disable audio...,must call this api
    pCtrl->video_index = -1;// MUST
    pCtrl->audio_index = -1;// MUST
    pCtrl->sub_index = -1;/// MUST
    // pCtrl->hassub = 1;  // enable subtitle
    pCtrl->displast_frame = 1;/// MUST
    //just open a buffer,just for p2p,http,etc...
    //pCtrl->auto_buffing_enable = 1;
    //pCtrl->buffing_min = 0.001;
    //pCtrl->buffing_middle = 0.02;
    //pCtrl->buffing_max = 0.9;

    pCtrl->t_pos = -1;  // start position, if live streaming, need set to -1
    pCtrl->need_start = 0; // if 0,you can omit player_start_play API.just play video/audio immediately. if 1,need call "player_start_play" API;

    pid = player_start(pCtrl, 0);
    if (pid < 0) {
        ALOGD("player start failed!error=%d\n", pid);
        return -1;
    }
    signal(SIGSEGV, signal_handler);
    //SYS_disable_osd0();
    while ((!tmpneedexit) && (!PLAYER_THREAD_IS_STOPPED(player_get_state(pid)))) {
        if (!osd_is_blank) {
            memset(newframestring, 0, sizeof(newframestring));
            new_frame_count = amsysfs_get_sysfs_int16("/sys/module/amvideo/parameters/new_frame_count");
            if (new_frame_count > 0) {
                ALOGD("set_display_axis =%s\n", newframestring);
                set_display_axis(0);
                osd_is_blank = 1;
            }
        }
        switch (tmpstep) {
        case EMU_STEP_PAUSE:
            ALOGD("@@@@@@@@@@@@@@@@@@@@@@@@@@   1    @@@@\n");
            player_pause(pid);
            tmpstep = EMU_STEP_MENU;
            break;
        case EMU_STEP_RESUME:
            ALOGD("@@@@@@@@@@@@@@@@@@@@@@@@@@   2    @@@@\n");
            player_resume(pid);
            tmpstep = EMU_STEP_MENU;
            break;
        case EMU_STEP_SEEK:
            ALOGD("will  seek position:100\n");
            pos = 100;
            player_timesearch(pid, pos);
            tmpstep = EMU_STEP_MENU;
            break;

            break;
        case EMU_STEP_STOP:
            ALOGD("@@@@@@@@@@@@@@@@@@@@@@@@@@   3    @@@@\n");
            player_stop(pid);
            tmpstep = EMU_STEP_MENU;
            break;
        case EMU_STEP_FF:
            ALOGD("please input fastforward speed:\n");
            speed = 1;
            player_forward(pid, speed);
            tmpstep = EMU_STEP_MENU;
            break;
        case EMU_STEP_RR:
            ALOGD("please input fastrewind speed:");
            speed = 1;
            player_backward(pid, speed);
            tmpstep = EMU_STEP_MENU;
            break;
        case EMU_STEP_SETLOOP:
            ALOGD("@@@@@@@@@@@@@@@@@@@@@@@@@@   4    @@@@\n");
            player_loop(pid);
            tmpstep = EMU_STEP_MENU;
            break;
        case EMU_STEP_EXIT:
            ALOGD("@@@@@@@@@@@@@@@@@@@@@@@@@@   5    @@@@\n");
            player_exit(pid);
            tmpneedexit = 1;
            break;
        case EMU_STEP_START:
            ALOGD("@@@@@@@@@@@@@@@@@@@@@@@@@@   6    @@@@\n");
            player_start_play(pid);
            //SYS_set_tsync_enable(0);///< f no sound,can set to be 0
            tmpstep = EMU_STEP_MENU;
            break;
        case EMU_STEP_GETAVMEDIAINFO:
            if (pid >= 0) {
                ALOGD("@@@@@@@@@@@@@@@@@@@@@@@@@@   7    @@@@\n");
                if (player_get_state(pid) > PLAYER_INITOK) {
                    ret = player_get_media_info(pid, &minfo);
                    if (ret == 0) {
                        _media_info_dump(&minfo);
                    }
                }
            }
            tmpstep = EMU_STEP_MENU;
            break;
        default:
            ALOGD("@@@@@@@@@@@@@@@@@@@@@@@@@@   8    @@@@\n");
            break;
        }

        usleep(100 * 1000);
        signal(SIGCHLD, SIG_IGN);
        signal(SIGTSTP, SIG_IGN);
        signal(SIGTTOU, SIG_IGN);
        signal(SIGTTIN, SIG_IGN);
        signal(SIGHUP, signal_handler);
        signal(SIGTERM, signal_handler);
        signal(SIGSEGV, signal_handler);
        signal(SIGINT, signal_handler);
        signal(SIGQUIT, signal_handler);
    }
    free(pCtrl->file_name);
    free(pCtrl);
    ALOGD("...........player exit,~,byeybe...........\n");
    amsysfs_set_sysfs_str(di_bypass_all_path, "0");
    return 0;
}

