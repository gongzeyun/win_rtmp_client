/***************************************************
 * name : player.c
 * function:  player thread, include all player functions
 * date     :  2010.2.2
 ***************************************************/

//header file
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <getopt.h>

#include <codec.h>
#include <message.h>
#include <player_set_sys.h>

//private
#include "player_priv.h"
#include "player_av.h"
#include "player_update.h"
#include "player_hwdec.h"
#include "thread_mgt.h"
#include "stream_decoder.h"
#include "player_ffmpeg_ctrl.h"
#include <amconfigutils.h>


/******************************
 * reset subtitle prop
 ******************************/
static void release_subtitle(play_para_t *para)
{
    if (para->sstream_info.has_sub) {
        set_subtitle_num(0);
        set_subtitle_curr(0);
        set_subtitle_index(0);
        set_subtitle_fps(0);
        set_subtitle_subtype(0);
        set_subtitle_startpts(0);
    }
}

/******************************
 * release player play info
 ******************************/
static int player_para_release(play_para_t *para)
{
    int i;

    if (para->decoder && para->decoder->stop_async) {
        para->decoder->stop_async(para);
    }

    if (para->vstream_info.has_video) {
        for (i = 0; i < para->media_info.stream_info.total_video_num; i ++) {
            if (para->media_info.video_info[i] != NULL) {
                FREE(para->media_info.video_info[i]);
                para->media_info.video_info[i] = NULL;
            }
        }
    }

    if (para->astream_info.has_audio) {
        for (i = 0; i < para->media_info.stream_info.total_audio_num; i ++) {
            if (para->media_info.audio_info[i] != NULL) {
                if (para->stream_type == STREAM_AUDIO) {
                    if (para->media_info.audio_info[i]->audio_tag) {
                        FREE(para->media_info.audio_info[i]->audio_tag);
                        para->media_info.audio_info[i]->audio_tag = NULL;
                    }
                }
                FREE(para->media_info.audio_info[i]);
                para->media_info.audio_info[i] = NULL;
            }
        }
        if (para->astream_info.extradata != NULL) {
            FREE(para->astream_info.extradata);
            para->astream_info.extradata = NULL;
        }
    }

    if (para->sstream_info.has_sub) {
        for (i = 0; i < para->media_info.stream_info.total_sub_num; i ++) {
            if (para->media_info.sub_info[i] != NULL) {
                FREE(para->media_info.sub_info[i]);
                para->media_info.sub_info[i] = NULL;
            }
        }
        for (i = 0; i < SSTREAM_MAX_NUM; i++) {
            if (para->sstream_info.sub_buf[i] != NULL) {
                FREE(para->sstream_info.sub_buf[i]);
                para->sstream_info.sub_buf[i] = NULL;
            }
        }
    }



    if (para->file_name) {
        FREE(para->file_name);
        para->file_name = NULL;
    }
    /*
    if (para->playctrl_info.pause_flag) {
           codec_resume(para->codec);     //clear pause state
           para->playctrl_info.pause_flag = 0;
    }
    */
    release_subtitle(para);
    if (para->decoder && para->decoder->release) {
        para->decoder->release(para);
        para->decoder = NULL;
    }
    ffmpeg_close_file(para);
    return PLAYER_SUCCESS;
}

static void setsysfs(char * path, char * value)
{
    int fd = -1;
    fd = open(path, O_RDWR);
    if (fd >= 0) {
        write(fd, value, 2);
        close(fd);
        fd = -1;
    } else {
        log_print("setsysfs: open file failed.\n");
    }
}


/******************************
 * check decoder status
 * if rm decoder error,
 * need reset decoder
 ******************************/
static int check_decoder_worksta(play_para_t *para)
{


    codec_para_t *codec;
    struct vdec_status vdec;
    int ret;

    /*if (para->vstream_info.video_format != VFORMAT_REAL) {
        return PLAYER_SUCCESS;
    }*/
    if (get_player_state(para) == PLAYER_PAUSE) {
        return PLAYER_SUCCESS;    //paused,don't care buf lowlevel
    }
    if (para->vstream_info.has_video/* && (!para->playctrl_info.video_low_buffer)*/) {
        if (para->vcodec) {
            codec = para->vcodec;
        } else {
            codec = para->codec;
        }
        if (codec) {
            ret = codec_get_vdec_state(codec, &vdec);
            if (ret != 0) {
                log_error("pid:[%d]::codec_get_vdec_state error: %x\n", para->player_id, -ret);
                return PLAYER_CHECK_CODEC_ERROR;
            } else {
                if (vdec.status & DECODER_ERROR_MASK) {
                    log_error("pid:[%d]:: decoder error vdec.status: %x\n", para->player_id, vdec.status);
                    int is_decoder_fatal_error = vdec.status & (DECODER_FATAL_ERROR_SIZE_OVERFLOW | DECODER_FATAL_ERROR_UNKNOW);
                    if (vdec.status & DECODER_FATAL_ERROR_NO_MEM) {
                        log_error("pid:[%d]::not enough codec memory for this file.\n", para->player_id);
                        send_event(para, PLAYER_EVENTS_ERROR, PLAYER_NOMEM, "not enough memory!");
                        return PLAYER_NOMEM;
                    }
                    if (!para->vbuffer.rp_is_changed) {
                        if (check_time_interrupt(&para->playctrl_info.vbuf_rpchanged_Old_time, 100)) {
                            para->vbuffer.check_rp_change_cnt --;
                        }
                    } else {
                        para->vbuffer.check_rp_change_cnt = CHECK_VIDEO_HALT_CNT;
                    }
                    if ((para->vbuffer.check_rp_change_cnt <= 0 && para->playctrl_info.video_low_buffer) ||
                        (is_decoder_fatal_error) ||
                        para->vbuffer.check_rp_change_cnt < - CHECK_VIDEO_HALT_CNT * 2) {
                        /*too long time no changes.*/
                        para->vbuffer.check_rp_change_cnt = CHECK_VIDEO_HALT_CNT;
                        if (para->stream_type == STREAM_RM) {
                            para->playctrl_info.time_point = -1; /*do reset  only & find next key frame only,*/
                        } else if (para->state.full_time > 0 && (para->state.current_time < para->state.full_time - 5)) {
                            para->playctrl_info.time_point = para->state.current_time + 1;
                        } else {
                            para->playctrl_info.time_point = -1;    /*do reset only.*/
                        }
                        para->playctrl_info.reset_flag = 1;
                        set_black_policy(0);
                        para->playctrl_info.end_flag = 1;
                        log_print("[%s]time=%f cnt=%d vlevel=%.03f vdec err, need reset\n", __FUNCTION__, para->playctrl_info.time_point, para->vbuffer.check_rp_change_cnt, para->state.video_bufferlevel);
                    }
                }
            }
        }
    }
    if (para->astream_info.has_audio) {
        if (check_audiodsp_fatal_err() == AUDIO_DSP_FATAL_ERROR || audio_digital_mode_change(para)) {
            para->playctrl_info.seek_base_audio = 1;
            para->playctrl_info.time_point = para->state.current_time + 1;
            para->playctrl_info.reset_flag = 1;
            para->playctrl_info.end_flag = 1;
            if (para->astream_info.audio_format == AFORMAT_COOK) {
                para->astream_info.has_audio = 0;
                para->playctrl_info.no_audio_flag = 1;
                para->start_param->nosound = 1;
                setsysfs("/sys/class/audiodsp/codec_fatal_err", "0");
            }
            set_black_policy(0);
            log_print("adec err::[%s:%d]time=%f ret=%d, need reset\n", __FUNCTION__, __LINE__, para->playctrl_info.time_point, ret);
        }
    }
    return PLAYER_SUCCESS;
}

static int check_subtitle_info(play_para_t *player)
{
    int sub_stream_num = 0;
    int cur_sub_id = -1;
    int i;
    subtitle_info_t sub_info[MAX_SUB_NUM];

    if (player->start_param->hassub == 0) {
        return PLAYER_SUCCESS;
    }
    if (player->stream_type == STREAM_PS && !player->sstream_info.sub_has_found) {
        if (!player->codec) {
            return PLAYER_EMPTY_P;
        }
        memset(sub_info, -1, sizeof(subtitle_info_t) * MAX_SUB_NUM);

        sub_stream_num = codec_get_sub_num(player->codec);
        if (sub_stream_num > player->media_info.stream_info.total_sub_num) {

            codec_get_sub_info(player->codec, sub_info);
            if (set_ps_subtitle_info(player, sub_info, sub_stream_num) == PLAYER_SUCCESS) {
                set_subtitle_num(sub_stream_num);

                //set_subtitle_curr(0);
                set_subtitle_enable(1);
                set_player_state(player, PLAYER_FOUND_SUB);
                update_playing_info(player);
                update_player_states(player, 1);
            }

            if (sub_info[1].id != -1 && sub_info[1].id != player->codec->sub_pid) {
                player->codec->sub_pid = sub_info[1].id;
                player->codec->sub_type = 0x17000;
                player->sstream_info.sub_pid = player->codec->sub_pid;
                player->sstream_info.sub_type = player->codec->sub_type;
                log_print("[%s]defatult:sub_info[1] id=0x%x\n", __FUNCTION__, sub_info[1].id);

                if (player->astream_info.start_time > 0) {
                    set_subtitle_startpts(player->astream_info.start_time);
                } else if (player->vstream_info.start_time > 0) {
                    set_subtitle_startpts(player->vstream_info.start_time);
                } else {
                    set_subtitle_startpts(0);
                }
                codec_set_sub_type(player->codec);
                codec_set_sub_id(player->codec);
                codec_reset_subtile(player->codec);
            }
        }
    }
    return PLAYER_SUCCESS;
}

/******************************
 * get audio codec pointer
 ******************************/
codec_para_t *get_audio_codec(play_para_t *player)
{
    if (player->stream_type == STREAM_ES ||
        player->stream_type == STREAM_AUDIO) {
        return player->acodec;
    } else {
        return player->codec;
    }
}

/******************************
 * get video codec pointer
 ******************************/
codec_para_t *get_video_codec(play_para_t *player)
{
    if (player->stream_type == STREAM_ES ||
        player->stream_type == STREAM_VIDEO) {
        return player->vcodec;
    } else {
        return player->codec;
    }
}

/******************************
 * get subtitle codec pointer
 ******************************/
codec_para_t *get_subtitle_codec(play_para_t *player)
{
    log_print("player->stream_type=%d.\n", player->stream_type);
    if (player->stream_type == STREAM_ES) {
        return player->scodec;
    } else {
        return player->codec;
    }
}

void check_msg(play_para_t *para, player_cmd_t *msg)
{
#ifdef DEBUG_VARIABLE_DUR
    int64_t t_fsize = 0;
    int t_fulltime = 0;
#endif
    if (((msg->ctrl_cmd & CMD_PAUSE) || (msg->ctrl_cmd & CMD_RESUME)) &&
        (get_player_state(para) == PLAYER_SEARCHING)) {
        if (para->vstream_info.has_video) {
            para->playctrl_info.init_ff_fr = 0;
            para->playctrl_info.last_f_step = para->playctrl_info.f_step;
            para->playctrl_info.f_step = 0;
        }
        return;
    }
    if ((msg->ctrl_cmd & CMD_EXIT) || (msg->ctrl_cmd & CMD_STOP)) {
        para->playctrl_info.end_flag = 1;
        para->playctrl_info.loop_flag = 0;
        para->playctrl_info.search_flag = 0;
        para->playctrl_info.request_end_flag = 1;
        para->playctrl_info.fast_forward = 0;
        para->playctrl_info.fast_backward = 0;
    } else if (msg->ctrl_cmd & CMD_SEARCH) {
#ifdef DEBUG_VARIABLE_DUR
        if (para->playctrl_info.info_variable && msg->f_param > para->state.full_time) {
            update_variable_info(para);
        }
#endif
        //clear ff/fb state
        if (get_player_state(para) == PLAYER_SEARCHING) {
            para->playctrl_info.fast_forward = 0;
            para->playctrl_info.fast_backward = 0;
            para->playctrl_info.f_step = 0;
            para->playctrl_info.trick_start_us = 0;
            para->astream_info.has_audio = para->astream_info.resume_audio;
            para->playctrl_info.reset_drop_buffered_data = 0;
            set_cntl_mode(para, TRICKMODE_NONE);
            log_print("seek durint searching, clear ff/fb first\n");
        }

        if ((para->start_param->is_livemode == 1 || para->start_param->is_livemode == 2) || (msg->f_param < para->state.full_time && msg->f_param >= 0)) {
            para->playctrl_info.search_flag = 1;
            para->playctrl_info.time_point = msg->f_param;
            para->playctrl_info.end_flag = 1;
            para->playctrl_info.reset_drop_buffered_data = 0;
            para->playctrl_info.seek_keyframe = 1;
            para->state.seek_point = msg->f_param;
            para->state.seek_delay = am_getconfig_int_def("libplayer.seek.delay_count", 1000);
        } else if (msg->f_param < 0) {
            log_print("pid[%d]::seek reset\n", para->player_id);
            para->playctrl_info.reset_flag = 1;
            para->playctrl_info.time_point = -1;
            para->playctrl_info.end_flag = 1;
            para->playctrl_info.reset_drop_buffered_data = 1;
            if (para->stream_type == STREAM_TS && para->vstream_info.has_video && (VFORMAT_MPEG12 == para->vstream_info.video_format  || VFORMAT_H264 == para->vstream_info.video_format)) {
                para->playctrl_info.seek_keyframe = 1;
            }
        } else if (msg->f_param == para->state.full_time) {
            para->playctrl_info.end_flag = 1;
            para->playctrl_info.search_flag = 0;
            para->playctrl_info.reset_drop_buffered_data = 0;
            para->state.current_time = msg->f_param;
            para->state.current_ms = para->state.current_time * 1000;
            para->state.seek_point = msg->f_param;
            log_print("seek the end,update current_time:%d (ms:%d)\n",
                      para->state.current_time, para->state.current_ms);
            set_player_state(para, PLAYER_SEARCHOK);
            update_player_states(para, 1);
            set_player_state(para, PLAYER_PLAYEND);
            update_playing_info(para);
            update_player_states(para, 1);
        } else {
            if (para->state.full_time == -1/* && msg->f_param == 0*/) {
                log_print("not support seek when fulltime is -1\n", para->player_id);
                return;
            }
            para->playctrl_info.reset_drop_buffered_data = 0;
            log_print("pid[%d]::seek time out of range!\n", para->player_id);
            set_player_error_no(para, PLAYER_SEEK_OVERSPILL);

            //set playend state when seek time overspill
            para->playctrl_info.end_flag = 1;
            para->playctrl_info.search_flag = 0;
            para->state.current_time = msg->f_param;
            para->state.current_ms = para->state.current_time * 1000;
            para->state.seek_point = msg->f_param;
            set_player_state(para, PLAYER_PLAYEND);
            update_playing_info(para);
            update_player_states(para, 1);
        }
    } else if (msg->ctrl_cmd & CMD_PAUSE) {
        para->playctrl_info.pause_flag = 1;
    } else if (msg->ctrl_cmd & CMD_RESUME) {
        para->playctrl_info.pause_flag = 0;
    } else if (msg->ctrl_cmd & CMD_FF) {
        if (para->vstream_info.has_video) {
            para->playctrl_info.init_ff_fr = 0;
            para->playctrl_info.last_f_step = para->playctrl_info.f_step;
            log_print("[%s:%d]f_step %d, last %d\n", __FUNCTION__, __LINE__, para->playctrl_info.f_step, para->playctrl_info.last_f_step);
            if (msg->param == 0) {
                para->playctrl_info.f_step = 0;
            } else {
                para->playctrl_info.f_step = msg->param * FF_FB_BASE_STEP;
                if (para->playctrl_info.fast_backward || (para->playctrl_info.last_f_step != para->playctrl_info.f_step)) {
                    log_print("[%s:%d]ff/fb change, reset start\n", __FUNCTION__, __LINE__);
                    para->playctrl_info.trick_start_us = 0;
                }

                para->playctrl_info.fast_forward = 1;
                para->playctrl_info.fast_backward = 0;

                if (para->playctrl_info.pause_flag) {
                    int has_audio_saved = para->codec->has_audio ;
                    if (para->codec->has_audio) {
                        para->codec->has_audio = 0;
                    }
                    codec_resume(para->codec);      //clear pause state
                    para->codec->has_audio = has_audio_saved;
                    para->playctrl_info.pause_flag = 0;
                }
                set_player_state(para, PLAYER_RUNNING);
            }
        } else {
            log_error("pid[%d]::no video, can't support ff!\n", para->player_id);
            set_player_error_no(para, PLAYER_FFFB_UNSUPPORT);
        }
    } else if (msg->ctrl_cmd & CMD_FB) {
        if (para->vstream_info.has_video) {
            para->playctrl_info.init_ff_fr = 0;
            para->playctrl_info.last_f_step = para->playctrl_info.f_step;
            log_print("[%s:%d]f_step %d, last %d\n", __FUNCTION__, __LINE__, para->playctrl_info.f_step, para->playctrl_info.last_f_step);
            if (msg->param == 0) {
                para->playctrl_info.f_step = 0;
            } else {
                para->playctrl_info.f_step = msg->param * FF_FB_BASE_STEP;
                if (para->playctrl_info.fast_forward || (para->playctrl_info.last_f_step != para->playctrl_info.f_step)) {
                    log_print("[%s:%d]ff/fb change, reset start\n", __FUNCTION__, __LINE__);
                    para->playctrl_info.trick_start_us = 0;
                }
                para->playctrl_info.fast_backward = 1;
                para->playctrl_info.fast_forward = 0;
                if (para->playctrl_info.pause_flag) {
                    int has_audio_saved = para->codec->has_audio ;
                    if (para->codec->has_audio) {
                        para->codec->has_audio = 0;
                    }
                    codec_resume(para->codec);      //clear pause state
                    para->codec->has_audio = has_audio_saved;
                    para->playctrl_info.pause_flag = 0;
                }
                set_player_state(para, PLAYER_RUNNING);
            }
        } else {
            log_error("pid[%d]::no video, can't support fb!\n", para->player_id);
            set_player_error_no(para, PLAYER_FFFB_UNSUPPORT);
        }
    } else if (msg->ctrl_cmd & CMD_SWITCH_AID) {
        para->playctrl_info.seek_base_audio = 1;
        para->playctrl_info.switch_audio_id = msg->param;
        set_black_policy(0);
    } else if (msg->set_mode & CMD_LOOP) {
        para->playctrl_info.loop_flag = 1;
    } else if (msg->set_mode & CMD_NOLOOP) {
        para->playctrl_info.loop_flag = 0;
    } else if (msg->set_mode & CMD_EN_AUTOBUF) {
        para->buffering_enable = msg->param;
    } else if (msg->set_mode & CMD_SET_AUTOBUF_LEV) {
        para->buffering_threshhold_min = msg->f_param;
        para->buffering_threshhold_middle = msg->f_param1;
        para->buffering_threshhold_max = msg->f_param2;
    } else if (msg->set_mode & CMD_SET_FREERUN_MODE) {
        /*low delay mode.
        #define FREERUN_NONE 0 // no freerun mode
        #define FREERUN_NODUR 1 // freerun without duration
        #define FREERUN_DUR 2 // freerun with duration
        bit 2: iponly_flag
        bit 3: no decoder reference buffer
        bit 4: vsync_pts_inc to up
        bit 5: no error recovery
        */
        int mode = msg->param;
        log_print("set freerun_mode 0x%x\n", mode);
        if (mode || am_getconfig_bool("media.libplayer.wfd")) { /*mode=1,2,is low buffer mode also*/
            if (para->pFormatCtx && para->pFormatCtx->pb) {
                ffio_set_buf_size(para->pFormatCtx->pb, 1024 * 4); //reset aviobuf to small.
                url_set_seek_flags(para->pFormatCtx->pb, LESS_BUFF_DATA | NO_READ_RETRY);
            }
            para->playctrl_info.lowbuffermode_flag = 1;
        } else {
            para->playctrl_info.lowbuffermode_flag = 0;
        }
        if (mode & 0x04) {
            /* ip frame only mode */
            para->playctrl_info.iponly_flag = 1;
        }
        if (mode & 0x08) {
            /* no decoder reference buffer */
            para->playctrl_info.no_dec_ref_buf = 1;
        }
        if (mode & 0x10) {
            /* vsync pts inc to up +1 */
            para->playctrl_info.vsync_upint = 1;
        }
        if (mode & 0x20) {
            /* no error recovery */
            para->playctrl_info.no_error_recovery = 1;
        }

        mode = mode & 0x3;
        para->playctrl_info.freerun_mode = mode;
        if (para->vcodec) {
            codec_set_freerun_mode(para->vcodec, mode);   /*es*/
        } else if (para->codec) {
            codec_set_freerun_mode(para->codec, mode);   /*not es*/
        }
    }
#if 1
    else if (msg->ctrl_cmd & CMD_SWITCH_SID) {
        para->playctrl_info.switch_sub_id = msg->param;
        player_switch_sub(para);
    }else if (msg->ctrl_cmd & CMD_SWITCH_TSPROGRAM) {
        para->playctrl_info.switch_ts_video_pid = msg->param;
        para->playctrl_info.switch_ts_audio_pid = msg->param1;
        para->playctrl_info.switch_ts_program_flag = 1;
    }
#endif
}
static void check_amutils_msg(play_para_t *para, player_cmd_t *msg)
{
    codec_para_t *p;
    if (msg->set_mode & CMD_LEFT_MONO) {
        log_print("set soundtrack left mono\n");
        p = get_audio_codec(para);
        if (p != NULL) {
            codec_left_mono(p);
        }

    } else if (msg->set_mode & CMD_RIGHT_MONO) {
        log_print("set soundtrack right mono\n");
        p = get_audio_codec(para);
        if (p != NULL) {
            codec_right_mono(p);
        }

    } else if (msg->set_mode & CMD_SET_STEREO) {
        log_print("set soundtrack stereo\n");
        p = get_audio_codec(para);
        if (p != NULL) {
            codec_stereo(p);
        }
    }
}
int nextcmd_is_cmd(play_para_t *player, ctrl_cmd_t c_cmd)
{
    int is = 0;
    player_cmd_t *msg;
    lock_message_pool(player);
    msg = peek_message_locked(player);
    if (msg && msg->ctrl_cmd == c_cmd) {
        is = 1;
    }
    unlock_message_pool(player);
    return is;
}
int check_flag(play_para_t *p_para)
{
    player_cmd_t *msg = NULL;
    int ret = 0;
    AVFormatContext *pFormat = p_para->pFormatCtx;
    AVStream *pStream;
    AVCodecContext *pCodec;
    int find_subtitle_index = 0;
    unsigned int i = 0;
    int subtitle_curr = 0;

    /*
    check the cmd & do for main thread;
    */
    if (p_para->oldcmd.ctrl_cmd == CMD_SEARCH &&
        nextcmd_is_cmd(p_para, CMD_SEARCH) &&
        (abs(player_get_systemtime_ms() - p_para->oldcmdtime) < 300) && /*lastcmd is not too old.*/
        ((p_para->stream_type == STREAM_ES && p_para->vcodec != NULL) || /*ES*/
         (p_para->stream_type != STREAM_ES  && p_para->codec && p_para->vstream_info.has_video))) { /*PS,RM,TS*/
        /*if latest cmd and next cmd are all search,we must wait the frame show.*/
        if (p_para->vcodec) {
            ret = codec_get_cntl_state(p_para->vcodec);    /*es*/
        } else {
            ret = codec_get_cntl_state(p_para->codec);    /*not es*/
        }
        if (p_para->avsynctmpchanged == 0) {
            p_para->oldavsyncstate = get_tsync_enable();
        }
        if (p_para->oldavsyncstate == 1) {
            set_tsync_enable(0);
            p_para->avsynctmpchanged = 1;
        }
        if (ret <= 0) {
            return NONO_FLAG;
        }
    }
    msg = get_message(p_para);  //msg: pause, resume, timesearch,add file, rm file, move up, move down,...
    if (msg) {
        p_para->oldcmd = *msg;
        p_para->oldcmdtime = player_get_systemtime_ms();
        log_print("pid[%d]:: [check_flag:%d]cmd=%x set_mode=%x info=%x param=%d fparam=%f\n", p_para->player_id, __LINE__, msg->ctrl_cmd, msg->set_mode, msg->info_cmd, msg->param, msg->f_param);
        log_print("[check_flag:%d]endflag %d\n", __LINE__, p_para->playctrl_info.end_flag);
        if (msg->ctrl_cmd != CMD_SEARCH && p_para->avsynctmpchanged > 0) {
            /*not search now,resore the sync states...*/
            set_tsync_enable(p_para->oldavsyncstate);
            p_para->avsynctmpchanged = 0;
        }
        // check resume play
        if (msg->ctrl_cmd == CMD_SEARCH) {
            int64_t time_diff = (p_para->oldcmdtime - p_para->play_start_systemtime_us);
            if (time_diff < 1 * 1000) { // 1s
                p_para->resume_play_flag = 1;
            }
        }
        check_msg(p_para, msg);
        message_free(msg);
        msg = NULL;
    } else {
        if (p_para->avsynctmpchanged > 0) {
            set_tsync_enable(p_para->oldavsyncstate);
            p_para->avsynctmpchanged = 0;
        }
    }

    if (p_para->playctrl_info.end_flag) {
        if (!p_para->playctrl_info.search_flag &&
            !p_para->playctrl_info.fast_forward &&
            !p_para->playctrl_info.fast_backward) {
            set_black_policy(p_para->playctrl_info.black_out);
            if (p_para->playctrl_info.request_end_flag) {
                set_player_state(p_para, PLAYER_STOPED);
            } else {
                set_player_state(p_para, PLAYER_PLAYEND);
            }
        } else if (p_para->playctrl_info.search_flag) {
            set_black_policy(0);
        }

        if (!p_para->playctrl_info.fast_forward && !p_para->playctrl_info.fast_backward) {
            return BREAK_FLAG;
        }
    }

    if ((p_para->playctrl_info.fast_forward ||
         p_para->playctrl_info.fast_backward)
        && (!p_para->playctrl_info.init_ff_fr)) {
        if (p_para->playctrl_info.f_step != 0) {
            p_para->astream_info.has_audio = 0;
            p_para->playctrl_info.init_ff_fr = 1;
            p_para->playctrl_info.time_point = p_para->state.current_time;
            set_black_policy(0);
            set_cntl_mode(p_para, TRICKMODE_FFFB);
        } else {
            p_para->playctrl_info.fast_forward = 0;
            p_para->playctrl_info.fast_backward = 0;
            p_para->playctrl_info.search_flag = 1;
            p_para->playctrl_info.trick_start_us = 0;
            p_para->astream_info.has_audio = p_para->astream_info.resume_audio;
            set_cntl_mode(p_para, TRICKMODE_NONE);
        }
        log_print("pid[%d]::[%s:%d]ff=%d fb=%d step=%d curtime=%d timepoint=%f\n", p_para->player_id, __FUNCTION__, __LINE__,
                  p_para->playctrl_info.fast_forward, p_para->playctrl_info.fast_backward,
                  p_para->playctrl_info.f_step, p_para->state.current_time, p_para->playctrl_info.time_point);
        log_print("[%s:%d]last step %d\n", __FUNCTION__, __LINE__, p_para->playctrl_info.last_f_step);
        if (p_para->playctrl_info.last_f_step && (p_para->playctrl_info.f_step != 0)) {
            return NONO_FLAG;
        } else {
            return BREAK_FLAG;
        }
    }

    if (p_para->playctrl_info.pause_flag) {
        if (get_player_state(p_para) != PLAYER_PAUSE) {
            ret = codec_pause(p_para->codec);
            if (ret != 0) {
                log_error("[%s:%d]pause failed!ret=%d\n", __FUNCTION__, __LINE__, ret);
            }
            set_player_state(p_para, PLAYER_PAUSE);
            update_playing_info(p_para);
            update_player_states(p_para, 1);
        }
        return CONTINUE_FLAG;
    } else {
        if ((get_player_state(p_para) == PLAYER_PAUSE) || (get_player_state(p_para) == PLAYER_SEARCHOK)) {
            ret = codec_resume(p_para->codec);
            if (ret != 0) {
                log_error("[%s:%d]resume failed!ret=%d\n", __FUNCTION__, __LINE__, ret);
            }
            set_player_state(p_para, PLAYER_RUNNING);
            update_playing_info(p_para);
            update_player_states(p_para, 1);
        }
    }

    if (p_para->playctrl_info.seek_base_audio) {
        player_switch_audio(p_para);
        p_para->playctrl_info.seek_base_audio = 0;
    }

    if ((p_para->state.current_pts - p_para->state.last_pts) >= PTS_FREQ / 10) {
        p_para->state.last_pts = p_para->state.current_pts;
        if (p_para->sstream_info.has_sub == 0) {
            return NONO_FLAG;
        }

        if (p_para->sstream_info.has_sub) {
            subtitle_curr = av_get_subtitle_curr();
        }

        if (subtitle_curr >= 0 && subtitle_curr < p_para->sstream_num && \
            subtitle_curr != p_para->sstream_info.cur_subindex) {
            log_print("start change subtitle from %d to %d \n", p_para->sstream_info.cur_subindex, subtitle_curr);
            //find new stream match subtitle_curr

            for (i = 0; i < pFormat->nb_streams; i++) {
                pStream = pFormat->streams[i];
                pCodec = pStream->codec;
                if (pCodec->codec_type == CODEC_TYPE_SUBTITLE) {
                    find_subtitle_index ++;
                }
                if (find_subtitle_index == subtitle_curr + 1) {
                    p_para->playctrl_info.switch_sub_id = pStream->id;
                    break;
                }
            }
            p_para->sstream_info.cur_subindex = subtitle_curr;
            if (p_para->stream_type == STREAM_PS) {
                p_para->codec->sub_pid = p_para->media_info.sub_info[subtitle_curr]->id;
                p_para->codec->sub_type = CODEC_ID_DVD_SUBTITLE;
                log_print("[%s]defatult:sub_info[1] id=0x%x\n", __FUNCTION__, p_para->media_info.sub_info[1]->id);
                if (p_para->astream_info.start_time > 0) {
                    set_subtitle_startpts(p_para->astream_info.start_time);
                } else if (p_para->vstream_info.start_time > 0) {
                    set_subtitle_startpts(p_para->vstream_info.start_time);
                } else {
                    set_subtitle_startpts(0);
                }
                codec_set_sub_type(p_para->codec);
                codec_set_sub_id(p_para->codec);
                codec_reset_subtile(p_para->codec);
            } else if (i == pFormat->nb_streams) {
                log_print("can not find subtitle curr\n\n");
            } else {
                player_switch_sub(p_para);
            }
        } else if (subtitle_curr >= p_para->sstream_num) {
            p_para->playctrl_info.switch_sub_id = 0xffff;
            p_para->sstream_info.sub_index = 0xffff;
            p_para->sstream_info.cur_subindex = subtitle_curr;
            player_switch_sub(p_para);
        }
    }

    if (p_para->playctrl_info.switch_ts_program_flag == 1) {
        return BREAK_FLAG;
    }

    return NONO_FLAG;
}

void set_player_error_no(play_para_t *player, int error_no)
{
    player->state.error_no = error_no;
}

void update_player_start_paras(play_para_t *p_para, play_control_t *c_para)
{
    //p_para->file_name                     = c_para->file_name;
    p_para->file_name = MALLOC(strlen(c_para->file_name) + 1);
    strcpy(p_para->file_name, c_para->file_name);
    p_para->file_name[strlen(c_para->file_name)] = '\0';
    p_para->state.name                  = p_para->file_name;
    p_para->vstream_info.video_index    = c_para->video_index;
    p_para->astream_info.audio_index    = c_para->audio_index;
    p_para->sstream_info.sub_index      = c_para->sub_index;
    p_para->playctrl_info.no_audio_flag = c_para->nosound;
    p_para->playctrl_info.no_video_flag = c_para->novideo;
    p_para->playctrl_info.has_sub_flag  = c_para->hassub;
    p_para->playctrl_info.loop_flag     = c_para->loop_mode;
    p_para->playctrl_info.time_point    = c_para->t_pos;
    p_para->playctrl_info.duration_url  = c_para->t_duration_ms;
    log_print("hassub %x starttime:%f(ms) duration_url:%d(ms) \n", c_para->hassub, c_para->t_pos, c_para->t_duration_ms);
    if (am_getconfig_bool("media.amplayer.noaudio")) {
        p_para->playctrl_info.no_audio_flag = 1;
    }
    if (am_getconfig_bool("media.amplayer.novideo")) {
        p_para->playctrl_info.no_video_flag = 1;
    }
#ifdef DEBUG_VARIABLE_DUR
    p_para->playctrl_info.info_variable = c_para->is_variable;
#endif
    p_para->playctrl_info.read_max_retry_cnt = c_para->read_max_cnt;
    if (p_para->playctrl_info.read_max_retry_cnt <= 0) {
        p_para->playctrl_info.read_max_retry_cnt = MAX_TRY_READ_COUNT;
    }
    //if(!p_para->playctrl_info.no_audio_flag)
    //    p_para->playctrl_info.audio_mute= codec_get_mutesta(NULL);
    p_para->playctrl_info.is_playlist   = c_para->is_playlist;
    p_para->update_state                = c_para->callback_fn;
    p_para->playctrl_info.black_out     = !c_para->displast_frame;
    p_para->buffering_enable            = c_para->auto_buffing_enable;
    p_para->byteiobufsize = c_para->byteiobufsize;
    p_para->loopbufsize = c_para->loopbufsize;
    p_para->audio_digital_raw = get_audio_digital_output_mode();
    p_para->enable_rw_on_pause = c_para->enable_rw_on_pause;
    p_para->playctrl_info.lowbuffermode_flag = c_para->lowbuffermode_flag;
    p_para->playctrl_info.buf_limited_time_ms = c_para->lowbuffermode_limited_ms;
    if (p_para->buffering_enable) {
        /*check threshhold is valid*/
        if (c_para->buffing_starttime_s <= 0) {
            p_para->buffering_exit_time_s = 10;  //10 seonds
        } else {
            p_para->buffering_exit_time_s = c_para->buffing_starttime_s;
        }
        p_para->buffering_time_s_changed = 1;
        p_para->buffering_enter_time_s = am_getconfig_float_def("media.amplayer.onbuffering.S", 0.120); //120ms
        p_para->div_buf_time = (int)am_getconfig_float_def("media.amplayer.divtime", 1);
        log_print("set buffering exit time to %f S,enter time t %f S\n", p_para->buffering_exit_time_s, p_para->buffering_enter_time_s);
        if (c_para->buffing_starttime_s > 0 && c_para->buffing_middle <= 0) {
            c_para->buffing_middle = 0.02;    //for tmp start.we will reset after start.
        }
        if (c_para->buffing_min < c_para->buffing_middle &&
            c_para->buffing_middle < c_para->buffing_max &&
            c_para->buffing_max < 1 &&
            c_para->buffing_min > 0
           ) {
            p_para->buffering_threshhold_min = c_para->buffing_min;
            p_para->buffering_threshhold_middle = c_para->buffing_middle;
            p_para->buffering_threshhold_max = c_para->buffing_max;
            if (c_para->buffing_force_delay_s > 0) {
                p_para->buffering_force_delay_s = c_para->buffing_force_delay_s;
                log_print("delay %d s to do buffering\n", c_para->buffing_force_delay_s);
            } else {
                p_para->buffering_force_delay_s  = 0;
            }
        } else {
            log_print("not a valid threadhold settings for buffering(must min=%f<middle=%f<max=%f)\n",
                      c_para->buffing_min,
                      c_para->buffing_middle,
                      c_para->buffing_max
                     );
            p_para->buffering_threshhold_min = 0.001;
            p_para->buffering_threshhold_middle = 0.02;
            p_para->buffering_threshhold_max = 0.8;
            log_print("Auto changed  threadhold settings  for default buffering(must min=%f<middle=%f<max=%f)\n",
                      p_para->buffering_threshhold_min,
                      p_para->buffering_threshhold_middle,
                      p_para->buffering_threshhold_max
                     );
            p_para->buffering_enable = 0;
        }
    }
    player_get_vdec_profile(&p_para->vdec_profile, 0);
    log_print("pid[%d]::Init State: mute_on=%d black=%d t_pos:%ds read_max_cnt=%d\n",
              p_para->player_id,
              p_para->playctrl_info.audio_mute,
              p_para->playctrl_info.black_out,
              p_para->playctrl_info.time_point,
              p_para->playctrl_info.read_max_retry_cnt);
    if (am_getconfig_bool_def("media.amplayer.disp_url", 1) > 0) {
        log_print("file::::[%s],len=%d\n", c_para->file_name, strlen(c_para->file_name));
    }
}
static int check_start_cmd(play_para_t *player)
{
    int flag = -1;
    player_cmd_t *msg = NULL;
    msg = get_message(player);  //msg: pause, resume, timesearch,add file, rm file, move up, move down,...
    if (msg) {
        log_print("pid[%d]::[check_flag:%d]ctrl=%x mode=%x info=%x param=%d\n", player->player_id, __LINE__, msg->ctrl_cmd, msg->set_mode, msg->info_cmd, msg->param);
        if (msg->ctrl_cmd & CMD_START) {
            flag = 1;
        }
        if (msg->ctrl_cmd & CMD_SEARCH) {
            if ((msg->f_param < player->state.full_time && msg->f_param >= 0) ||
                (msg->f_param >= 0 && player->start_param->is_livemode == 1)
               ) {
                player->playctrl_info.time_point = msg->f_param;
                set_player_state(player, PLAYER_SEARCHOK);
                update_player_states(player, 1);
                player->pre_seek_flag = 1;
                log_print("pid[%d]::seek before start, set time_point to %f\n", player->player_id, player->playctrl_info.time_point);
                player->playctrl_info.end_flag = 0;
                message_free(msg);
                msg = NULL;
                return 0;
            }
        }
        check_msg(player, msg);
        message_free(msg);
        msg = NULL;
    }

    return flag;
}
static int check_stop_cmd(play_para_t *player)
{
    player_cmd_t *msg = NULL;
    int ret = -1;

    msg = get_message(player);
    while (msg && ret != 1) {
        log_print("pid[%d]::[player_thread:%d]cmd=%x set_mode=%x info=%x param=%d\n", player->player_id, __LINE__, msg->ctrl_cmd, msg->set_mode, msg->info_cmd, msg->param);
        if (msg->ctrl_cmd & CMD_STOP) {
            ret = 1;
        }
        message_free(msg);
        msg = NULL;
        if (ret != 1) { /*maybe we have more than one msg,and the last is STOP cmd*/
            msg = get_message(player);
        }
    }
    return ret;
}

static void player_para_init(play_para_t *para)
{
    para->state.start_time = -1;
    para->state.first_time = -1; // this is an invalid value, will be reseted later
    para->vstream_info.video_index = -1;

    para->vstream_info.start_time = -1;
    para->astream_info.start_time = -1;
    if (para->astream_info.audio_index < 0) {
        para->astream_info.audio_index = -1;

    } else {
        log_print("player init audio index:%d\n", para->astream_info.audio_index);
    }


    para->sstream_info.sub_index = -1;

    para->discontinue_point = 0;
    para->discontinue_flag = 0;
    para->first_index = -1;
    para->state.seek_point = 0;
    para->state.seek_delay = 0;
}

int player_force_enter_buffering(play_para_t *player, int nForce)
{
    int force_buf_enable =  am_getconfig_bool_def("media.amplayer.force_buf_enable", 1);
    if (player->pFormatCtx->pb == NULL || player->pFormatCtx->pb->local_playback == 0) {
        player->force_enter_buffering = force_buf_enable;
        // enter buffering here, for quick pause
        if (force_buf_enable) {
            codec_pause(player->codec);
            set_player_state(player, PLAYER_BUFFERING);
            update_player_states(player, 1);
            if (nForce == 0) {
                player->force_enter_buffering = 0;
                log_print("Force enter buffering!!!, but set player->force_enter_buffering=0\n");
            } else {
                if (player->codec->has_audio == 1) {
                    log_print("[%s:%d]mute audio before forcing codec_pause", __FUNCTION__, __LINE__);
                    codec_set_mute(player->codec, 1);
                }
                log_print("Force enter buffering!!!\n");
            }
        }
    }

    return 0;

}
static int check_and_modify_livemode(play_para_t *player);
static int check_and_modify_livemode(play_para_t *player)
{
    if (NULL == player) {
        log_print("Invalid input param\n");
        return -1;
    }

    int nLivemode = -1;
    int ret = ffmpeg_geturl_netstream_info(player, 5, &nLivemode);
    if (ret == 0 && nLivemode != player->start_param->is_livemode) {
        log_print("[%s:%d]livemode %d --> %d\n", __FUNCTION__, __LINE__, player->start_param->is_livemode, nLivemode);
        player->start_param->is_livemode = nLivemode;
    }

    return 0;
}
///////////////////*main function *//////////////////////////////////////
void *player_thread(play_para_t *player)
{
    am_packet_t am_pkt;
    AVPacket avpkt;
    am_packet_t *pkt = NULL;
    int ret;
    unsigned int exit_flag = 0;
    pkt = &am_pkt;
    player_file_type_t filetype;

    //#define SAVE_YUV_FILE

#ifdef SAVE_YUV_FILE
    int out_fp = -1;
#endif

    AVCodecContext *ic = NULL;
    AVCodec *codec = NULL;
    AVFrame *picture = NULL;
    int got_picture = 0;
    int AFORMAT_SW_Flag = 0;
    char *audio_out_buf = NULL;
    int  audio_out_size = 0;
    log_print("\npid[%d]::enter into player_thread\n", player->player_id);

    update_player_start_paras(player, player->start_param);
    player_para_init(player);
    av_packet_init(pkt);

    pkt->avpkt = &avpkt;
    av_init_packet(pkt->avpkt);
    player->p_pkt = pkt;

    //player_thread_wait(player, 100 * 1000);    //wait pid send finish
    set_player_state(player, PLAYER_INITING);
    update_playing_info(player);
    update_player_states(player, 1);

    /*start open file and get file type*/
    ret = ffmpeg_open_file(player);
    if (ret != FFMPEG_SUCCESS) {
        set_player_state(player, PLAYER_ERROR);
        send_event(player, PLAYER_EVENTS_ERROR, ret, "Open File failed");
        log_print("[player_dec_init]ffmpeg_open_file failed(%s)*****ret=%x!\n", player->file_name, ret);
        goto release0;
    }
    if (player->pFormatCtx && player->pFormatCtx->pb && player->pFormatCtx->pb->is_slowmedia) {
        url_set_seek_flags(player->pFormatCtx->pb, LESS_BUFF_DATA | NO_READ_RETRY);
    }
    ffmpeg_parse_file_type(player, &filetype);
    set_player_state(player, PLAYER_TYPE_REDY);
    send_event(player, PLAYER_EVENTS_STATE_CHANGED, PLAYER_TYPE_REDY, 0);
    send_event(player, PLAYER_EVENTS_FILE_TYPE, &filetype, 0);
    if (player->start_param->is_type_parser) {
        player_cmd_t *msg;
        player_thread_wait(player, 10 * 1000);
        msg = peek_message(player);
        if (msg && (msg->ctrl_cmd & (CMD_EXIT | CMD_STOP))) {
            goto release0;
        }
    }
    log_print("pid[%d]::parse ok , prepare parameters\n", player->player_id);
    ret = player_dec_init(player);
    if (ret != PLAYER_SUCCESS) {
        if (check_stop_cmd(player) == 1) {
            set_player_state(player, PLAYER_STOPED);
        } else {
            set_player_state(player, PLAYER_ERROR);
        }
        goto release0;
    }
    if (player->pFormatCtx != NULL && player->pFormatCtx->duration > 0 && player->pFormatCtx->pb != NULL && player->pFormatCtx->pb->opaque != NULL) {
        URLContext *h = (URLContext *)player->pFormatCtx->pb->opaque;
        if (h != NULL && h->prot != NULL && (strcmp(h->prot->name, "shttp") == 0 || strcmp(h->prot->name, "http") == 0)) {
            log_info("[player_thread]set to the network vod\n");
            //player->pFormatCtx->flags|=AVFMT_FLAG_NETWORK_VOD;
        }
    }
    const char * startsync_mode = "/sys/class/tsync/startsync_mode";
    const char * droppcm_prop = "sys.amplayer.drop_pcm";
    if (!strncmp(player->file_name, "rtp:", strlen("rtp:")) || player->pFormatCtx->flags & AVFMT_FLAG_NETWORK_VOD) {
        set_sysfs_int(startsync_mode, 1);
        am_setconfig_float(droppcm_prop, 0);
    } else {
        set_sysfs_int(startsync_mode, 2);
        am_setconfig_float(droppcm_prop, 1);
    }

    ret = set_media_info(player);
    if (ret != PLAYER_SUCCESS) {
        log_error("pid[%d::player_set_media_info failed!\n", player->player_id);
        set_player_state(player, PLAYER_ERROR);
        goto release0;
    }

    float config_value = 0.0;
    int maxbufsize = 2 * 1024 * 1024;
    int config_ret = am_getconfig_float("media.libplayer.startplaybuf", &config_value);
    int lpbufsize = MIN(config_value * 1024, maxbufsize);
    if (!config_ret && player->pFormatCtx->pb && player->pFormatCtx->pb->is_slowmedia) {
        do {
            if (url_interrupt_cb()) {
                break;
            }
            if (url_buffed_pos(player->pFormatCtx->pb) > 0 && url_buffed_pos(player->pFormatCtx->pb) < lpbufsize) {
                if (ffmpeg_buffering_data(player) < 0) {
                    player_thread_wait(player, 100 * 1000);
                }
            } else {
                break;
            }
        } while (1);
    }

    set_player_state(player, PLAYER_INITOK);
    update_playing_info(player);
    update_player_states(player, 1);
#if 0
    switch (player->pFormatCtx->drm.drm_check_value) {
    case 1: // unauthorized
        set_player_state(player, PLAYER_DIVX_AUTHORERR);
        //send_event(player, PLAYER_EVENTS_STATE_CHANGED,PLAYER_DIVX_AUTHORERR, "Divx Author Failed");
        update_playing_info(player);
        update_player_states(player, 1);
        //  goto release0;
        break;
    case 2: // expired
        //send_event(player, PLAYER_EVENTS_STATE_CHANGED, PLAYER_DIVX_RENTAL_EXPIRED, "Divx Author Expired");
        set_player_state(player, PLAYER_DIVX_RENTAL_EXPIRED);
        update_playing_info(player);
        update_player_states(player, 1);
        //  goto release0;
        break;
    case 3: // rental
        //send_event(player, PLAYER_EVENTS_STATE_CHANGED, PLAYER_DIVX_RENTAL_VIEW, player->pFormatCtx->drm.drm_rental_value);
        set_drm_rental(player, player->pFormatCtx->drm.drm_rental_value);
        set_player_state(player, PLAYER_DIVX_RENTAL_VIEW);
        update_playing_info(player);
        update_player_states(player, 1);
        break;
    default:
        break;
    }
#endif
    if (player->start_param->need_start) {
        int flag = 0;
        do {
            flag = check_start_cmd(player);
            if (flag == 1) {
                break;
            } else if (player->playctrl_info.end_flag == 1 && (!player->playctrl_info.search_flag)) {
                if (player->playctrl_info.request_end_flag) {
                    set_player_state(player, PLAYER_STOPED);
                } else {
                    set_player_state(player, PLAYER_PLAYEND);
                }
                update_playing_info(player);
                update_player_states(player, 1);
                goto release0;
            }

            if (ffmpeg_buffering_data(player) < 0) {
                player_thread_wait(player, 100 * 1000);
            }
        } while (1);
    }
#if 0
    /* if drm rental , continue playback here
     * caution:
     * 1. resume play, should not call drmCommitPlayback
     * 2.
     * */
    if (player->pFormatCtx->drm.drm_header && player->pFormatCtx->drm.drm_check_value == 3) {
        ret = drmCommitPlayback();
        if (ret != 0) {
            log_error(" not unauthorized 9:result=%d, err=%d\n", ret, drmGetLastError());
            player->pFormatCtx->drm.drm_check_value = 1; // unauthorized, should popup a dialog
        }
    }
#endif
    log_print("pid[%d]::start offset prepare\n", player->player_id);
    ret = player_offset_init(player);
    if (ret != PLAYER_SUCCESS) {
        log_error("pid[%d]::prepare offset failed!\n", player->player_id);
        //set_player_state(player, PLAYER_ERROR);
        //goto release;
        if (player->playctrl_info.raw_mode) {
            //log_print("*****data offset 0x%x\n", p_para->data_offset);
            url_fseek(player->pFormatCtx->pb, player->data_offset, SEEK_SET);
        }
    }

    log_print("pid[%d]::decoder prepare\n", player->player_id);
    ret = player_decoder_init(player);
    if (ret != PLAYER_SUCCESS) {
        log_error("pid[%d]::player_decoder_init failed!\n", player->player_id);
        set_player_state(player, PLAYER_ERROR);
        goto release;
    }

    set_cntl_mode(player, TRICKMODE_NONE);
    set_cntl_avthresh(player, AV_SYNC_THRESH);
    set_cntl_syncthresh(player);
    set_sysfs_int("/sys/class/tsync/vpause_flag", 0); // reset vpause flag -> 0

    set_player_state(player, PLAYER_START);
    update_playing_info(player);
    update_player_states(player, 1);
    player_mate_init(player, 1000 * 10);
    ffmpeg_seturl_buffered_level(player, 0);
    if (player->astream_info.has_audio == 1 &&
        player->vstream_info.has_video == 0 &&
        (player->astream_info.audio_format == AFORMAT_COOK || player->astream_info.audio_format == AFORMAT_SIPR)
       ) {
        AFORMAT_SW_Flag = 1;
    }
    if (player->vstream_info.video_format == VFORMAT_SW || AFORMAT_SW_Flag == 1) {
        log_print("Use SW video decoder\n");

#ifdef SAVE_YUV_FILE
        out_fp = open("./output.yuv", O_CREAT | O_RDWR);
        if (out_fp < 0) {
            log_print("Create output file failed! fd=%d\n", out_fp);
        }
#endif

        av_register_all();

        ic = avcodec_alloc_context();
        if (!ic) {
            log_print("AVCodec Memory error\n");
            ic = NULL;
            goto release;
        }

        if (player->vstream_info.video_format == VFORMAT_SW) {
            ic->codec_id = player->pFormatCtx->streams[player->vstream_info.video_index]->codec->codec_id;
            ic->codec_type = CODEC_TYPE_VIDEO;
            ic->pix_fmt = PIX_FMT_YUV420P;
            picture = avcodec_alloc_frame();
            if (!picture) {
                log_print("Could not allocate picture\n");
                goto release;
            }
        } else if (AFORMAT_SW_Flag == 1) {
            AVCodecContext  *pCodecCtx = player->pFormatCtx->streams[player->astream_info.audio_index]->codec;
            ic->bit_rate       = pCodecCtx->bit_rate;
            ic->sample_rate    = pCodecCtx->sample_rate;
            ic->channels       = pCodecCtx->channels;
            ic->block_align    = pCodecCtx->block_align;
            ic->extradata_size = pCodecCtx->extradata_size;
            ic->codec_id       = pCodecCtx->codec_id;
            ic->codec_type     = CODEC_TYPE_AUDIO;
            if (pCodecCtx->extradata_size > 0) {
                log_print("[%s %d]pCodecCtx->extradata_size/%d\n", __FUNCTION__, __LINE__, pCodecCtx->extradata_size);
                ic->extradata = malloc(pCodecCtx->extradata_size);
                if (ic->extradata != NULL) {
                    memcpy(ic->extradata, pCodecCtx->extradata, pCodecCtx->extradata_size);
                } else {
                    log_print("[%s %d]malloc failed!\n", __FUNCTION__, __LINE__);
                    goto release;
                }
            }
        }
        codec = avcodec_find_decoder(ic->codec_id);
        if (!codec) {
            log_print("Codec not found\n");
            goto release;
        }

        if (avcodec_open(ic, codec) < 0) {
            log_print("Could not open codec\n");
            goto release;
        }
    }
    player->play_start_systemtime_us = player_get_systemtime_ms();
    player->play_last_reset_systemtime_us = player->play_start_systemtime_us;
    log_print("pid[%d]::playback loop... pb=%x\n", player->player_id, player->pFormatCtx->pb);
    //player loop
    do {
        if ((!(player->vstream_info.video_format == VFORMAT_SW)
             && !(player->vstream_info.video_format == VFORMAT_VC1 && player->vstream_info.video_codec_type == VIDEO_DEC_FORMAT_WMV3)) || \
            (IS_AUIDO_NEED_PREFEED_HEADER(player->astream_info.audio_format) && player->astream_info.has_audio) ||
            (IS_SUB_NEED_PREFEED_HEADER(player->sstream_info.sub_type) && player->sstream_info.has_sub)) {
            if (!player->playctrl_info.trick_wait_flag) {
                pre_header_feeding(player);
            }
        }
        do {
            check_and_modify_livemode(player);
            ret = check_flag(player);
            if (ret == BREAK_FLAG) {
                //log_print("pid[%d]::[player_thread:%d]end=%d valid=%d new=%d pktsize=%d ff %d gettime %lld\n", player->player_id,
                //          __LINE__, player->playctrl_info.end_flag, pkt->avpkt_isvalid, pkt->avpkt_newflag, pkt->data_size, player->playctrl_info.fast_forward, gettime());

                if (!player->playctrl_info.end_flag) {
                    if (pkt->avpkt_isvalid) {
                        //player->playctrl_info.read_end_flag = 1;
                        goto write_packet;
                    } else {
                        player->playctrl_info.end_flag = 1;
                    }
                }
                if (pkt->avpkt) {
                    av_free_packet(pkt->avpkt);
                    pkt->avpkt_isvalid = 0;
                }
                break;
            } else if (ret == CONTINUE_FLAG) {


            }
            if (player->playctrl_info.trick_wait_time > gettime()) {
                continue;
            } else if (player->playctrl_info.trick_wait_flag) {
                break;
            }

            if (!pkt->avpkt_isvalid) {
                ret = read_av_packet(player);
                if (ret != PLAYER_SUCCESS && ret != PLAYER_RD_AGAIN) {
                    if (player->playctrl_info.hls_force_exit != 1 && (ret == PLAYER_RD_FAILED || ret == PLAYER_RD_TIMEOUT)) {
                        ret = (check_to_retry(player) == 0) ? PLAYER_RD_AGAIN : ret;
                    }
                    if (ret != PLAYER_RD_AGAIN || player->playctrl_info.hls_force_exit == 1) { // needn't to retry
                        log_error("pid[%d]::read_av_packet failed!\n", player->player_id);
                        set_player_state(player, PLAYER_ERROR);
                        goto release;
                    }
                    // need to retry
                } else {
                    player->retry_cnt = 0;
                }

                ret = set_header_info(player);
                if (ret != PLAYER_SUCCESS) {
                    log_error("pid[%d]::set_header_info failed! ret=%x\n", player->player_id, -ret);
                    set_player_state(player, PLAYER_ERROR);
                    goto release;
                }
            } else {
                /*low level buf is full ,do buffering or just do wait.*/
                if (player->enable_rw_on_pause) { /*enabled buffing on paused...*/
                    if (ffmpeg_buffering_data(player) <= 0) {
                        player_thread_wait(player, 100 * 1000); //100ms
                        ///continue;
                    }
                } else {
                    player_thread_wait(player, 100 * 1000); //100ms
                }
            }
            if ((player->playctrl_info.f_step == 0) &&
                (ret == PLAYER_SUCCESS) &&
                (get_player_state(player) != PLAYER_RUNNING) &&
                (get_player_state(player) != PLAYER_BUFFERING) &&
                (get_player_state(player) != PLAYER_PAUSE)) {
                set_player_state(player, PLAYER_RUNNING);
                update_playing_info(player);
                update_player_states(player, 1);
            }
write_packet:
            if ((player->vstream_info.video_format == VFORMAT_SW) && (pkt->type == CODEC_VIDEO)) {
                avcodec_decode_video2(ic, picture, &got_picture, pkt->avpkt);
                pkt->data_size = 0;

                if (got_picture) {
#ifdef SAVE_YUV_FILE
                    if (out_fp >= 0) {
                        int i;

                        for (i = 0 ; i < ic->height ; i++) {
                            write(out_fp, picture->data[0] + i * picture->linesize[0], ic->width);
                        }
                        for (i = 0 ; i < ic->height / 2 ; i++) {
                            write(out_fp, picture->data[1] + i * picture->linesize[1], ic->width / 2);
                        }
                        for (i = 0 ; i < ic->height / 2 ; i++) {
                            write(out_fp, picture->data[2] + i * picture->linesize[2], ic->width / 2);
                        }
                    }
#endif
                }
            } else if (AFORMAT_SW_Flag == 1) {
                int bytes_used = 0;
                audio_out_size = MAX(AVCODEC_MAX_AUDIO_FRAME_SIZE, ic->channels * ic->frame_size * sizeof(int16_t));
                audio_out_buf = malloc(audio_out_size);
                if (audio_out_buf != NULL && pkt->avpkt_isvalid && pkt->avpkt->size > 0 && get_player_state(player) == PLAYER_RUNNING) {
                    memset(audio_out_buf, 0, audio_out_size);
                    bytes_used = ic->codec->decode(ic, audio_out_buf, &audio_out_size, pkt->avpkt);
                    if (audio_out_size > 0) {
                        av_free_packet(pkt->avpkt);
                        pkt->data = audio_out_buf; //it will be free in write_av_packet()
                        pkt->data_size = audio_out_size;
                        write_av_packet(player);
                    }
                }
                if (pkt->avpkt) {
                    av_free_packet(pkt->avpkt);
                    pkt->avpkt_isvalid = 0;
                }
                if (audio_out_buf != NULL) {
                    free(audio_out_buf);
                    audio_out_buf = NULL;
                    audio_out_size = 0;
                }
                pkt->data = NULL;
                pkt->data_size = 0;
            } else {
                ret = write_av_packet(player);
                if (ret == PLAYER_WR_FINISH) {
                    if (player->playctrl_info.f_step == 0) {
                        log_print("[player_thread]write end!\n");
                        break;
                    }
                } else if (ret != PLAYER_SUCCESS) {
                    log_print("pid[%d]::write_av_packet failed!\n", player->player_id);
                    set_player_state(player, PLAYER_ERROR);
                    goto release;
                }
            }
            update_playing_info(player);
            update_player_states(player, 0);
            if (check_decoder_worksta(player) != PLAYER_SUCCESS) {
                log_error("pid[%d]::check decoder work status error!\n", player->player_id);
                set_player_state(player, PLAYER_ERROR);
                goto release;
            }

            check_subtitle_info(player);

            if ((player->vstream_info.video_format == VFORMAT_SW) && (pkt->type == CODEC_VIDEO)) {
                player->state.current_time = (int)(pkt->avpkt->dts / 1000);
                if (player->state.current_time > player->state.full_time) {
                    player->state.current_time = player->state.full_time;
                }
                if (player->state.current_time < player->state.last_time) {
                    log_print("[%s]curtime<lasttime curtime=%d lastime=%d\n", __FUNCTION__, player->state.current_time, player->state.last_time);
                    player->state.current_time = player->state.last_time;
                }
                player->state.last_time = player->state.current_time;
            }

            if (player->vstream_info.has_video
                && (player->playctrl_info.fast_forward
                    || player->playctrl_info.fast_backward)) {
                if (player->vstream_info.video_format != VFORMAT_SW) {
                    ret = get_cntl_state(pkt) | player->playctrl_info.seek_offset_same;
                    if (ret == 0) {
                        //log_print("more data needed, data size %d\n", pkt->data_size);
                        continue;
                    } else if (ret < 0) {
                        log_error("pid[%d]::get state exception\n", player->player_id);
                        continue;
                    } else {
                        unsigned int curvpts = get_cntl_vpts(pkt);
                        int64_t cur_us = (int64_t)curvpts * 1000 / 90;
                        int wait_time;
                        player->playctrl_info.end_flag = 1;
                        update_playing_info(player);
                        update_player_states(player, 1);
                        int64_t start_time = player->pFormatCtx->start_time;
                        /*
                            If the start_time is longer the 32bit, we use first_time.
                        */
                        if (start_time > 0xffffffff) {
                            if (player->state.first_time > 0) {
                                start_time = (int64_t)player->state.first_time * 1000 / 90;
                            } else {
                                log_print("[%s:%d]get first_time error\n", __FUNCTION__, __LINE__);
                                start_time = start_time & 0xffffffff;
                            }
                        }

                        if ((curvpts == 0) || (curvpts == 1)) {
                            log_print("[%s:%d]curvpts is 0, use timepoint\n", __FUNCTION__, __LINE__);
                            cur_us = player->playctrl_info.time_point * 1000 * 1000 + start_time;
                        }
                        if (0 == player->playctrl_info.trick_start_us) {
                            log_print("[%s:%d]current_ms %d, start_time %lld\n", __FUNCTION__, __LINE__, player->state.current_ms, start_time);
                            player->playctrl_info.trick_start_us = (int64_t)player->state.current_ms * 1000 + start_time;
                            /*
                                If the difference of cur_us and trick_start_us is longer then 20s or negative value, we are sure that the start time is not right.
                                So we assign trick_start_us to cur_us, and the first wait time after ff/fb is 0s.
                            */
                            if ((player->playctrl_info.fast_forward
                                 && (((cur_us - player->playctrl_info.trick_start_us) > 20000000) || ((cur_us - player->playctrl_info.trick_start_us) < 0)))
                                || (player->playctrl_info.fast_backward
                                    && (((player->playctrl_info.trick_start_us - cur_us) > 20000000) || ((player->playctrl_info.trick_start_us - cur_us) < 0)))) {
                                player->playctrl_info.trick_start_us = cur_us;
                            }
                            player->playctrl_info.trick_start_sysus = gettime();
                        }
                        int64_t discontinue_threshold = player->playctrl_info.f_step * 20000000;
                        if ((player->playctrl_info.fast_forward
                             && (((cur_us - player->playctrl_info.trick_start_us) > discontinue_threshold) || ((cur_us - player->playctrl_info.trick_start_us) < 0)))
                            || (player->playctrl_info.fast_backward
                                && (((player->playctrl_info.trick_start_us - cur_us) > discontinue_threshold) || ((player->playctrl_info.trick_start_us - cur_us) < 0)))) {
                            log_print("[%s:%d]reset player->playctrl_info.trick_start_us from %lld to %lld\n", __FUNCTION__, __LINE__, player->playctrl_info.trick_start_us, cur_us);
                            player->playctrl_info.trick_start_us = cur_us;
                        }
                        if (0 == player->playctrl_info.f_step) {
                            wait_time = 0;
                        } else {
                            if (cur_us > player->playctrl_info.trick_start_us) {
                                wait_time = (cur_us - player->playctrl_info.trick_start_us) / player->playctrl_info.f_step;
                            } else {
                                wait_time = (player->playctrl_info.trick_start_us - cur_us) / player->playctrl_info.f_step;
                            }
                        }
                        //player->playctrl_info.last_trick_us = cur_us;
                        player->playctrl_info.trick_wait_time = wait_time + player->playctrl_info.trick_start_sysus;
                        player->playctrl_info.trick_wait_flag = 1;
                        log_print("[%s:%d]f_step %d, curvpts 0x%x, cur %lld, start %lld, wait %d, trickwait %lld\n",
                                  __FUNCTION__, __LINE__, player->playctrl_info.f_step, curvpts, cur_us, player->playctrl_info.trick_start_us, wait_time, player->playctrl_info.trick_wait_time);
                        //player_thread_wait(player, wait_time);
                        //player_thread_wait(player, (32 >> player->playctrl_info.f_step) * 10 * 1000); //(32 >> player->playctrl_info.f_step)*10 ms
                        break;
                    }
                }
            }

#if 0  // no sync watchdog
            check_avdiff_status(player);
#endif
        } while (!player->playctrl_info.end_flag);

        if ((player->playctrl_info.trick_wait_time > gettime())  && (player->playctrl_info.f_step != 0)) {
            continue;
        }

        log_print("wait for play end...(sta:0x%x)\n", get_player_state(player));

        //wait for play end...
        while (!player->playctrl_info.end_flag) {
            ret = check_flag(player);
            if (ret == BREAK_FLAG) {
                if (player->playctrl_info.search_flag
                    || player->playctrl_info.fast_forward
                    || player->playctrl_info.fast_backward) {
                    log_print("[%s:%d] clear read_end_flag\n", __FUNCTION__, __LINE__);
                    player->playctrl_info.read_end_flag = 0;
                }
                break;
            } else if (ret == CONTINUE_FLAG) {
                continue;
            }
            if (!player->playctrl_info.reset_flag) {
                player_thread_wait(player, 50 * 1000);
            }
            check_decoder_worksta(player);
            if (update_playing_info(player) != PLAYER_SUCCESS) {
                break;
            }

            update_player_states(player, 0);
        }

        log_print("pid[%d]::loop=%d search=%d ff=%d fb=%d reset=%d step=%d switch_ts_program_flag:%d\n",
                  player->player_id,
                  player->playctrl_info.loop_flag, player->playctrl_info.search_flag,
                  player->playctrl_info.fast_forward, player->playctrl_info.fast_backward,
                  player->playctrl_info.reset_flag, player->playctrl_info.f_step, player->playctrl_info.switch_ts_program_flag);

        exit_flag = (!player->playctrl_info.loop_flag)   &&
                    (!player->playctrl_info.search_flag) &&
                    (!player->playctrl_info.fast_forward) &&
                    (!player->playctrl_info.fast_backward) &&
                    (!player->playctrl_info.reset_flag) &&
                    (!player->playctrl_info.switch_ts_program_flag);

        if (exit_flag) {
            break;
        } else {
            if (get_player_state(player) != PLAYER_SEARCHING && player->playctrl_info.switch_ts_program_flag == 0) {
                set_auto_refresh_rate(0);
                set_player_state(player, PLAYER_SEARCHING);
                update_playing_info(player);
                update_player_states(player, 1);
            }

            ret = player_reset(player);
            if (ret != PLAYER_SUCCESS) {
                log_error("pid[%d]::player reset failed(-0x%x)!", player->player_id, -ret);
                set_player_state(player, PLAYER_ERROR);
                break;
            }
            if (player->playctrl_info.end_flag) {
                set_player_state(player, PLAYER_PLAYEND);
                break;
            }

            if (player->playctrl_info.search_flag) {
                set_player_state(player, PLAYER_SEARCHOK);
                update_playing_info(player);
                update_player_states(player, 1);
                //reset thes contrl var
                player->div_buf_time = (int)am_getconfig_float_def("media.amplayer.divtime", 1);
                if (player->pFormatCtx->flags & AVFMT_FLAG_NETWORK_VOD) {
                    player->buffering_force_delay_s  = am_getconfig_float_def("media.amplayer.delaybuffering", 2);
                    log_print("set force delay buffering %f\n", player->buffering_force_delay_s);
                } else {
                    force_buffering_enter(player);
                }
                player->play_last_reset_systemtime_us = player_get_systemtime_ms();
                if (player->playctrl_info.f_step == 0) {
                    // set_black_policy(player->playctrl_info.black_out);
                }
                resume_auto_refresh_rate();
            }

            if (player->playctrl_info.reset_flag) {
                set_black_policy(player->playctrl_info.black_out);
            }

            player->playctrl_info.search_flag = 0;
            player->playctrl_info.reset_flag = 0;
            player->playctrl_info.end_flag = 0;
            player->playctrl_info.switch_ts_program_flag = 0;
            av_packet_release(pkt);
        }
    } while (1);
release:
    if (player->vstream_info.video_format == VFORMAT_SW || AFORMAT_SW_Flag == 1) {
#ifdef SAVE_YUV_FILE
        printf("Output file closing\n");
        if (out_fp >= 0) {
            close(out_fp);
        }
        printf("Output file closed\n");
#endif
        if (picture) {
            av_free(picture);
        }
        if (ic) {
            log_print("AVCodec close\n");
            avcodec_close(ic);
            av_free(ic);
        }
    }
    set_cntl_mode(player, TRICKMODE_NONE);
    set_sysfs_int("/sys/class/tsync/vpause_flag", 0);

release0:
    resume_auto_refresh_rate();
    player_mate_release(player);
    log_print("\npid[%d]player_thread release0 begin...(sta:0x%x)\n", player->player_id, get_player_state(player));

    if (get_player_state(player) == PLAYER_ERROR) {
        if (player->playctrl_info.request_end_flag || check_stop_cmd(player) == 1) {
            /*we have a player end msg,ignore the error*/
            set_player_state(player, PLAYER_STOPED);
            set_player_error_no(player, 0);
        } else {
            int64_t value = 0;
            int rv = ffmpeg_geturl_netstream_info(player, 3, &value);
            if (rv == 0) { //get http download errors for HLS streaming
                ret  = value;
            }
            set_player_error_no(player, ret);
        }

        log_print("player error,force video blackout\n");
        set_black_policy(1);
    }
    update_playing_info(player);
    update_player_states(player, 1);
    av_packet_release(&am_pkt);
    player_para_release(player);
    set_player_state(player, PLAYER_EXIT);
    update_player_states(player, 1);
    log_print("\npid[%d]::stop play, exit player thead!(sta:0x%x)\n", player->player_id, get_player_state(player));
    pthread_exit(NULL);

    return NULL;
}

