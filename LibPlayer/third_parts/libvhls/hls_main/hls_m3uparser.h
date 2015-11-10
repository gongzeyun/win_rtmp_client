#ifndef _HLS_M3UPARSER_H
#define _HLS_M3UPARSER_H

#include <pthread.h>
#include "hls_list.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifndef MAX_URL_SIZE
#define MAX_URL_SIZE 4096
#endif


#define DISCONTINUE_FLAG                (1<<0)
#define ALLOW_CACHE_FLAG                (1<<1)
#define CIPHER_INFO_FLAG                (1<<5)


typedef struct _M3uKeyInfo {
    char keyUrl[MAX_URL_SIZE];
    char method[10];
    char iv[35];
    char * extDrminfo;
} M3uKeyInfo;
typedef struct _M3uBaseNode {
    int index;
    char fileUrl[MAX_URL_SIZE];
    int bandwidth;
    int program_id;
    int64_t startUs;
    int64_t durationUs;
    int64_t range_offset;
    int64_t range_length;
    int64_t dataTime;
    int media_sequence;
    int flags;
    M3uKeyInfo* key;
    struct list_head  list;
} M3uBaseNode;

int parseInt32(const char *s, int32_t *x);
int parseInt64(const char *s, int64_t *x);

int m3u_parse(const char *baseURI, const void *data, size_t size, void** hParse);
int m3u_is_extm3u(void* hParse);
int m3u_is_variant_playlist(void* hParse);
int m3u_is_complete(void* hParse);
int m3u_get_node_num(void* hParse);
int64_t m3u_get_durationUs(void* hParse);
int m3u_get_target_duration(void* hParse);
M3uBaseNode* m3u_get_node_by_index(void* hParse, int index);
M3uBaseNode* m3u_get_node_by_time(void* hParse, int64_t timeUs);
M3uBaseNode* m3u_get_node_by_datatime(void* hParse, int64_t dataTime);
M3uBaseNode* m3u_get_node_by_url(void* hParse, char *srcurl);
int64_t m3u_get_node_span_size(void* hParse, int start_index, int end_index);
int m3u_release(void* hParse);
M3uKeyInfo * dup_keyInfo(M3uKeyInfo *scrkeyinfo);
int  m3u_install_keyinfo2playlist(void* hParse, M3uKeyInfo *baseScriptkeyinfo);


#ifdef __cplusplus
}
#endif


#endif
