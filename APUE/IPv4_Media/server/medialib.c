#include <errno.h>
#include <error.h>
#include <fcntl.h>
#include <glob.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <syslog.h>
#include <unistd.h>

#include "../include/proto.h"
#include "medialib.h"
#include "mytbf.h"
#include "server_conf.h"

//#define DEBUG

#define PATHSIZE 1024
#define LINEBUFSIZE 1024
#define MP3_BITRATE 320 * 1024 // 采样率,等于320kbit,换算成字节时要除以8
// 一个频道的存储结构
struct channel_context_st {
  chnid_t chnid;  // 频道id
  char *desc;   // 频道描述
  glob_t mp3glob; // 目录项
  int pos;        // current song
  int fd;         // current song fd
  off_t offset;
  mytbf_t *tbf; // 流控器
};

static struct channel_context_st channel[MAXCHNID + 1]; // 全部频道的数组,下标加一是为了空出0号元素

// 将某个目录下的所有文件转为一个频道,并打开第一个音乐文件
static struct channel_context_st *path2entry(const char *path) {
  syslog(LOG_INFO, "current path: %s\n", path);
  // 将字符数组 pathstr 中的所有元素被初始化为 \0（空字符）
  char pathstr[PATHSIZE] = {'\0'};
  char linebuf[LINEBUFSIZE];
  FILE *fp;
  struct channel_context_st *me;

  // 由于是一个静态变量所以相当于一直在操作同一块内存 有叠加效果
  static chnid_t curr_id = MINCHNID; 
  // 检测目录合法性
  strcat(pathstr, path);
  // strcat函数用于连接两个字符串。此例中，
  // 它将字符串"/desc.txt"连接到pathstr原字符串的末尾。
  // 注意pathstr必须预先分配足够空间以容纳新内容及终止符\0。
  strcat(pathstr, "/desc.txt");
  // 以只读方式打开频道描述文件
  fp = fopen(pathstr, "r"); 
  syslog(LOG_INFO, "channel dir:%s\n", pathstr);
  if (fp == NULL) {
    syslog(LOG_INFO, "%s is not a channe; dir (can not find desc.txt)", path);
    return NULL;
  }
  // 从文件指针fp指向的文件中读取一行文本
  if (fgets(linebuf, LINEBUFSIZE, fp) == NULL) {
    syslog(LOG_INFO, "%s is not a channel dir(cant get the desc.text)", path);
    fclose(fp);
    return NULL;
  }
  fclose(fp); // 关闭频道描述文件

  // 初始化上下文
  me = malloc(sizeof(*me));
  if (me == NULL) {
    syslog(LOG_ERR, "malloc():%s", strerror(errno));
    return NULL;
  }
  // 初始化流量控制器
  me->tbf = mytbf_init(MP3_BITRATE / 8, MP3_BITRATE / 8 * 5); 
  if (me->tbf == NULL) {
    syslog(LOG_ERR, "mytbf_init():%s", strerror(errno));
    free(me);
    return NULL;
  }

  // 将读取到的频道描述写入到 me->desc 中
  me->desc = strdup(linebuf);
  // strncpy 函数会从 path 复制最多 PATHSIZE 个字符到 pathstr 中。
  // 若 path 长度小于 PATHSIZE，pathstr 末尾可能不以空字符结束。使用时需注意避免缓冲区溢出风险。
  strncpy(pathstr, path, PATHSIZE);
  // 拼接出通配符路径
  strncat(pathstr, "/*.mp3", PATHSIZE-1);
  // 解析出目录下的所有音乐文件. On successful completion, glob() and glob_b() return zero.
  if (glob(pathstr, 0, NULL, &me->mp3glob) != 0) {
    // 如果没成功,就遍历去下一个频道
    curr_id++;
    syslog(LOG_ERR, "%s is not a channel dir(can not find mp3 files", path);
    free(me);
    return NULL;
  }
  me->pos = 0;
  me->offset = 0;
  // 打开第一个音乐文件
  me->fd = open(me->mp3glob.gl_pathv[me->pos], O_RDONLY); 
  if (me->fd < 0) {
    syslog(LOG_WARNING, "%s open failed.", me->mp3glob.gl_pathv[me->pos]);
    free(me);
    return NULL;
  }
  // 记录下当前频道的id
  me->chnid = curr_id;
  curr_id++;
  return me;
}

int mlib_getchnlist(struct mlib_listentry_st **result, int *resnum) {
  int num = 0;
  int i = 0;
  char path[PATHSIZE];
  glob_t globres;
  struct mlib_listentry_st *ptr;
  struct channel_context_st *res;

  // 下标从1开始遍历
  for (int i = 0; i < MAXCHNID; ++i) {
    // -1表示当前频道未启用
    channel[i].chnid = -1;
  }
  // 将字符串格式化后写入到指定的字符串数组中.PATHSIZE: 目标字符串的最大容量（包括空终止符）。这个函数永远不会越界
  // 获取频道目录,并拼接出一个能代表其所有文件的通配符路径
  snprintf(path, PATHSIZE, "%s/*", server_conf.media_dir);
#ifdef DEBUG
  printf("current path:%s\n", path);
#endif
// glob用来解析路径通配符. 根据给定的模式匹配文件系统中的文件路径. 匹配到的文件名列表存入globres中
// On successful completion, glob() and glob_b() return zero.
  if (glob(path, 0, NULL, &globres)) { // 成功返回0
    return -1;
  }
#ifdef DEBUG
// 打印匹配到的文件名
  printf("globres.gl_pathv[0]:%s\n", globres.gl_pathv[0]);
  printf("globres.gl_pathv[1]:%s\n", globres.gl_pathv[1]);
  printf("globres.gl_pathv[2]:%s\n", globres.gl_pathv[2]);
#endif
// 为节目单分配内存
  ptr = malloc(sizeof(struct mlib_listentry_st) * globres.gl_pathc);
  if (ptr == NULL) {
    syslog(LOG_ERR, "malloc() error.");
    exit(1);
  }
  // 遍历目录
  for (i = 0; i < globres.gl_pathc; ++i) {
    // 解析子目录,并打开其下的第一个音乐文件
    res = path2entry(globres.gl_pathv[i]);
    if (res != NULL) {
      syslog(LOG_ERR, "path2entry() return : %d %s.", res->chnid, res->desc);
      // 将 res 所指向的数据块复制到了以 channel[res->chnid] 开始的内存区域。
      memcpy(channel + res->chnid, res, sizeof(*res));
      // 覆写频道号和频道描述
      ptr[num].chnid = res->chnid;
      ptr[num].desc = strdup(res->desc);
      // 解析出来的子目录数量
      num++;
    }
  }
  // 下面是两个输出参数
  // realloc函数尝试重新分配指定大小的新内存，并将原来内存的内容复制到新内存中。
  *result = realloc(ptr, sizeof(struct mlib_listentry_st) * num);
  if (*result == NULL) {
    syslog(LOG_ERR, "realloc() failed.");
  }
  // 回写最后解析出来的频道数量
  *resnum = num;
  return 0;
}

int mlib_freechnlist(struct mlib_listentry_st *ptr) {
  free(ptr);
  return 0;
}

// 打开下一首音乐
static int open_next(chnid_t chnid) {
  for (int i = 0; i < channel[chnid].mp3glob.gl_pathc; ++i) {
    channel[chnid].pos++; // 更新偏移
    if (channel[chnid].pos == channel[chnid].mp3glob.gl_pathc) {
      printf("没有新文件了 列表循环\n");
      channel[chnid].pos = 0;
    }
    close(channel[chnid].fd);

    // 尝试打开新文件
    channel[chnid].fd =
        open(channel[chnid].mp3glob.gl_pathv[channel[chnid].pos], O_RDONLY);
    if (channel[chnid].fd < 0) {
      syslog(LOG_WARNING, "open(%s):%s", channel[chnid].mp3glob.gl_pathv[chnid],
             strerror(errno));
    } else {
      printf("打开新文件了\n");
      channel[chnid].offset = 0;
      return 0;
    }
  }
  syslog(LOG_ERR, "None of mp3 in channel %d id available.", chnid);
  return -1;
}

// 读取指定数量的数据
ssize_t mlib_readchn(chnid_t chnid, void *buf, size_t size) {
  int tbfsize;
  int len;
  int next_ret = 0;
  // 申请令牌数
  tbfsize = mytbf_fetchtoken(channel[chnid].tbf, size);
  syslog(LOG_INFO, "current tbf():%d", mytbf_checktoken(channel[chnid].tbf));

  while (1) {
     // 读取tbfsize数据到从offset处开始的buf,len等于实际读取到的数据长度
    len = pread(channel[chnid].fd, buf, tbfsize, channel[chnid].offset);
    /*current song open failed*/
    if (len < 0) {
      // 当前这首歌可能有问题，错误不至于退出，就读取下一首歌
      syslog(LOG_WARNING, "media file %s pread():%s",
             channel[chnid].mp3glob.gl_pathv[channel[chnid].pos],
             strerror(errno));
      open_next(chnid);
    } else if (len == 0) {
      syslog(LOG_DEBUG, "media %s file is over",
             channel[chnid].mp3glob.gl_pathv[channel[chnid].pos]);
#ifdef DEBUG
      printf("current chnid :%d\n", chnid);
#endif
      next_ret = open_next(chnid);
      break;
    } else /*len > 0*/ //真正读取到了数据
    {
      channel[chnid].offset += len;
      struct stat buf;
      // 获取文件状态信息。&buf是指向用于存储文件状态信息的结构体的指针。
      fstat(channel[chnid].fd, &buf);
      syslog(LOG_DEBUG, "epoch : %f%%",
             (channel[chnid].offset) / (1.0*buf.st_size)*100);
      break;
    }
  }
  // remain some token
  if (tbfsize - len > 0)
    // 归还多余的令牌
    mytbf_returntoken(channel[chnid].tbf, tbfsize - len);
  printf("current chnid :%d\n", chnid);
  return len; //返回读取到的长度
}
