
#include "ammodule.h"
#include "libavformat/url.h"
#include <android/log.h>
#define  LOG_TAG    "libprhls_mod"
#define  LOGI(...)  __android_log_print(ANDROID_LOG_INFO,LOG_TAG,__VA_ARGS__)


ammodule_methods_t  libprhls_module_methods;
ammodule_t AMPLAYER_MODULE_INFO_SYM = {
tag:
    AMPLAYER_MODULE_TAG,
version_major:
    AMPLAYER_API_MAIOR,
version_minor:
    AMPLAYER_API_MINOR,
    id: 0,
name: "libprhls_mod"
    ,
author: "Amlogic"
    ,
descript: "libprhls module binding library"
    ,
methods:
    &libprhls_module_methods,
dso :
    NULL,
reserved :
    {0},
};

extern URLProtocol pr_crypto_protocol;


int libprhls_mod_init(const struct ammodule_t* module, int flags)
{
    LOGI("libprhls module init\n");
    av_register_protocol(&pr_crypto_protocol);
    return 0;
}


int libprhls_mod_release(const struct ammodule_t* module)
{
    LOGI("libprhls module release\n");
    return 0;
}


ammodule_methods_t  libprhls_module_methods = {
    .init =  libprhls_mod_init,
    .release =   libprhls_mod_release,
} ;

