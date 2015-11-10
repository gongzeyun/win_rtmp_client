#!/system/bin/sh
#!/bin/sh

#dumppath path 
dumppath()
{
 echo " " $1 " = "
 cat  $1
 echo "-------------------------------------"
}

dumpdir(){
##    echo "start dir $1"
    for filename in `ls $1`
        do
            if [ -d $1'/'$filename ] ; then
                  ##echo $1'/'$filename
                  dumpdir $1'/'$filename
            elif [ -f $1'/'$filename ]; then 
                  dumppath $1'/'$filename
            fi
        done    
        
}


#sysinfo
dumppath  /proc/version
dumppath  /proc/iomem
dumppath  /proc/interrupts      
dumppath  /proc/meminfo

#medioinfo
dumpdir  /sys/class/amstream/
dumpdir  /sys/class/video/ 
dumpdir  /sys/class/ppmgr/ 
dumpdir  /sys/class/tsync/   
dumpdir  /sys/class/vdec/   
dumpdir  /sys/class/astream/
dumpdir  /sys/class/audiodsp/  

dumpdir  /sys/module/amvdec_h264/
dumpdir  /sys/module/amvdec_avs/
dumpdir  /sys/module/amvdec_h264mvc/
dumpdir  /sys/module/amvdec_mjpeg/
dumpdir  /sys/module/amvdec_mpeg12/
dumpdir  /sys/module/amvdec_mpeg4/
dumpdir  /sys/module/amvdec_real/
dumpdir  /sys/module/amvdec_vc1/
dumpdir  /sys/module/amlvideodri/

echo "dump opened files info"
ls -al /proc/2570/fd/

#graphics:
dumpdir  /sys/class/display/
dumpdir  /sys/class/graphics/fb0/ 
dumpdir  /sys/class/graphics/fb1/
dumpdir  /sys/class/ge2d/


dumpsys media.player
getprop
