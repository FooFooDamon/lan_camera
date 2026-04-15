<meta http-equiv="Content-Type" content="text/html; charset=utf-8" />
<base target="_blank" />
<!-- <font size=5> -->

# 局域网摄像头 | Local Area Network Camera

## 简介 | Introduction

这是一款适用于**局域网**的**轻量**化**广播**型视像监控程序。与常见的解决方案相比有以下不同：
> This is a **light-weight** **broadcast-based** camera program for **LAN** environment.
Compared with common solutions, it makes several differences in:

* `功能点`：仅支持最核心的实时图像传输、录像保存（录音暂未实现）、本机录像播放操作，无多余花哨功能；
数据不走公网、不上传云端，大大降低隐私风险。
    > `Capabilities`: It only supports the most core operations including real-time image transmission,
    video saving (sound recording not implemented yet) and local video playing.
    The data wouldn't go through Internet, and wouldn't be uploaded to cloud,
    which can lower down the risk of privacy leak.

* `资源消耗`：实测非常节能，即使对孤寒鬼每月的电费单也无明显影响。
按需录像（例如仅在有人经过时），则可极大节省硬碟或储存卡。
    > `Resource Consumption`: It's been tested to be so electricity-saving that
    there is no obvious influence on each month electricity bill of a miser.
    By saving video files only if necessary (e.g., when someone passing by),
    it helps reduce the capacity requirement on hard disk or flash card greatly.

* `复杂度`：操作界面简约至极，不带给用户丝毫心智负担。代码量少、逻辑简洁、业务流程短，
可大大减少第三方库的数量，同时也意味着大大增加代码的稳定性，更贯彻了“`代码更少（siu2），人生更妙（miu6）`”
的懒人理念。
    > `Complexity`: The operating GUI is very simple, which would not bring any difficulty to user.
    The code quantity is small, the software logic is concise, the flow is short,
    all these lead to great reduction of 3rd-party libraries and great stability enhancement of code.
    My opinion is unchanged all the time: `Less code, better world`.

* `定制化`：针对特定硬件平台，用户可重新实现部分函数，补全人工智能推理的业务逻辑，
或者利用专有的硬件加速接口来提升图像缩放或视频编解码的性能等等。
    > `Customization`: Based on a specific hareware platform, the user can re-implement some functions
    to complete the AI inference logic, or improve the image resizing or video encoding/decoding performance
    by help of specific hareware acceleration interfaces, etc.

* `目标用户`：喜欢简单、喜欢`Linux`、使用电脑多过手机的技术人。
    > `Target Users`: Developers who love simplicity, love `Linux` and prefer computer over cellphone.

## 定制提示 | Customization Tips

### 服务端示例 | An Example of Server Side

* 创建一个新项目，根据需要重新实现一个或多个带`__attribute__((weak))`标记的函数，
并能编译生成名为`liblancs_cust.so`的库文件。
    > Create a new project which implements one or more function(s) marked with `__attribute__((weak))`
    and can produce a library named `liblancs_cust.so`.

* 为该项目生成一个符号链接：
    > Make a symbolic link for this project:
    ````
    $ ln -s /PATH/TO/CUSTOM/PROJECT $PWD/src/server.priv
    ````

* 重新编译服务端程序，详见后面的服务端部署指引。
    > Recompile server program, see the server deployment guideline in next chapter for more details.

* 复制定制库文件到系统目录：
    > Copy the customized library to system directory:
    ````
    $ sudo cp /PATH/TO/CUSTOM/PROJECT/liblancs_cust.so /usr/lib/
    ````

### 客户端示例 | An Example of Client Side

与服务器定制类似，只是库文件名要改成`liblancc_cust.so`，符号链接名要改成`client.priv`。
> Similar to server customization except that the library name is `liblancc_cust.so`
and the symbolic link name is `client.priv`.

## 部署 | Deployment

### 服务端 | Server Side

````
$ sudo ln -s $PWD /opt/
$ mkdir -p ~/etc
$
$ sudo vim /etc/rsyncd.secrets # APPEND a new line like: lanc:<_YOUR_PASSWORD_>
$ sudo chown root:root /etc/rsyncd.secrets
$ sudo chmod 600 /etc/rsyncd.secrets
$ cp etc/rsyncd.conf ~/etc/
$ vim ~/etc/rsyncd.conf # Edit items in it based on your system, especially "address" and "hosts allow".
$ sudo rsync --daemon --config=${HOME}/etc/rsyncd.conf
$
$ cp etc/lan_camera.srv.json ~/etc/
$ vim ~/etc/lan_camera.srv.json # Edit items in it accordingly, especially /network/multicast/send_policy/interface.
$
$ sudo sysctl -w net.core.wmem_max=$((1024 * 1024 * 12)) # after EACH system REBOOT
$
$ make prepare-server && make server && sudo cp src/server/lanc_server /usr/local/bin/
$ source scripts/aliases.sh
$ lanc_server
````

### 客户端 | Client Side

````
$ sudo ln -s $PWD /opt/
$ mkdir -p ~/etc
$
$ sudo mkdir -p /usr/local/etc
$ sudo vim /usr/local/etc/rsync-lanc.pswd # Fill in the same PASSWORD as the one in /etc/rsyncd.secrets of server
$
$ jq -r '.save.enabled = false' --indent 4 etc/lan_camera.cli.json > ~/etc/lan_camera.cli.json
$ vim ~/etc/lan_camera.cli.json # Edit items in it accordingly, especially /network/connect/ip and /save/sync/ip.
$
$ make prepare-client && make client && sudo cp src/client/lanc_client /usr/local/bin/
$ source scripts/aliases.sh
$ lanc_client
````

## 简单演示 | Simple Demonstration

若对实际的运行效果（服务端已经过定制）感兴趣，可移步至[这个视频](https://www.bilibili.com/video/BV1TCPFeEEBv)。
**注意**：视频演示的是早前另一个未开源的个人项目（因与特定平台强绑定），
录制之时该项目仍是一个`单体应用程序`，但核心功能基本相同。
还需注意的是，若未经过任何定制，则本项目只是一个仅能传输实时图像（暂无声音）的工具。

> If you are interested in how this project (with the server side customized) works,
please take a look at [this video](https://b23.tv/MvECr8h).
**NOTE**: The video is about another personal project developed ealier
(and kept as private because of its strong binding to a specific platform),
and that project was still a `monolithic application` at that time,
but its core functionalities are basically the same with this project.
Also note that this project is just a tool that can only transfer real-time images
(no audio yet) without any customization.

<!-- </font> -->
<a target="_self" href="#" style="position: fixed; bottom: 20px; right: 20px; z-index: 1000;">返回顶部 | Back to Top</a>

