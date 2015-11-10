#include <stdio.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/poll.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>

#include "amstream.h"

#include "amsub_log.h"
#include "amsub_dec.h"



int subtitle_poll_sub_fd(int sub_fd, int timeout)
{
    struct pollfd sub_poll_fd[1];
    if (sub_fd <= 0)
    {
        return 0;
    }
    sub_poll_fd[0].fd = sub_fd;
    sub_poll_fd[0].events = POLLOUT;
    return poll(sub_poll_fd, 1, timeout);
}


int subtitle_get_sub_size_fd(int sub_fd)
{
    int sub_size, r;
    r = ioctl(sub_fd, AMSTREAM_IOC_SUB_LENGTH, (unsigned long)&sub_size);
    if (r < 0)
        return 0;
    else
        return sub_size;
}


int subtitle_read_sub_data_fd(int sub_fd, char *buf, unsigned int length)
{
    int data_size = length, r, read_done = 0;
    while (data_size)
    {
        //usleep(10000);
        r = read(sub_fd, buf + read_done, data_size);
        if (r <= 0)
        {
            break;
        }
        else
        {
            data_size -= r;
            read_done += r;
        }
    }
    //amsub_print("---read_done=%d-,data_size=%d,length=%d--\n",read_done,data_size,length);
    return 0;
}

int open_sub_device()
{
    int handle;
    int i = 0;
    do
    {
        handle = open(SUBTITLE_READ_DEVICE, O_RDONLY);
        if (handle < 0)
        {
            handle = -1;
            usleep(10000);
            i++;
            continue;
        }
        break;
    }
    while (i < 200);
    return handle;
}



int amsub_read_sub_data(amsub_para_t *amsub_para, amsub_info_t *amsub_info)
{
    int ret = 0;
    ret = amsub_dec_out_get(amsub_para, amsub_info);
    if (ret != 0)
    {
        amsub_print("amsub_read_sub_data failed! \n");
        return -1;
    }
    return 0;
}
