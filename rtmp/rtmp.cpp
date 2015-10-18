// rtmp.cpp : 定义控制台应用程序的入口点。
//

#include "stdafx.h"
#include "RTMP_Core.h"

RTMP_Context g_context;

int _tmain(int argc, _TCHAR* argv[])
{
	RTMP_Open("rtmp://live.hkstv.hk.lxdns.com/live/hks");
	return 0;
}

