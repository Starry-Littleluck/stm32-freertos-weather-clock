#ifndef __FONT_DOWNLOAD_DEMO_H
#define __FONT_DOWNLOAD_DEMO_H

/* 初始化字库下载协议状态和外部 Flash。 */
void font_download_demo_init(void);

/* 轮询并处理 USART1 中收到的字库数据。 */
void font_download_demo_run(void);

#endif /* __FONT_DOWNLOAD_DEMO_H */
