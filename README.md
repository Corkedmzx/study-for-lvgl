# LVGL for frame buffer device

LVGL configured to work with /dev/fb0 on Linux.

When cloning this repository, also make sure to download submodules (`git submodule update --init --recursive`) otherwise you will be missing key components.

Check out this blog post for a step by step tutorial:
https://blog.lvgl.io/2018-01-03/linux_fb

# 启动
CC=arm-linux-gnueabihf-gcc make

# 说明
核心源码来源:lv_port_linux
播放功能实现:基于mplayer
该仓库代码功能实现基于粤嵌GEC6818设备
主要功能:
相册
音/视频播放(卡顿问题处理中)
LED/蜂鸣器控制
天气获取(固定地址,修改需从源码修改)
密码锁(默认密码114514)

# 注意
此仓库创建时未设置许可证(使用默认),请根据lvgl官方的许可声明使用该仓库代码,如有侵权请及时联系删除
此仓库代码仅供学习,不做任何商业用途
