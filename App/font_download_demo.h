#ifndef __FONT_DOWNLOAD_DEMO_H
#define __FONT_DOWNLOAD_DEMO_H

#include <stdint.h>

typedef void (*font_download_idle_handler_t)(uint8_t data);

/* 初始化字库下载协议状态和外部 Flash。 */
void font_download_demo_init(void);

/* 注册字库协议空闲态的串口字节处理函数。 */
void font_download_demo_set_idle_handler(font_download_idle_handler_t handler);

/* 轮询并处理 USART1 中收到的字库数据。 */
void font_download_demo_run(void);

#endif /* __FONT_DOWNLOAD_DEMO_H */
