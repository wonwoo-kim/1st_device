#ifndef _APP_H_
#define _APP_H_

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdint.h>
#include <pthread.h>
#include <mqueue.h>

#include "rknn_helper.h"
#include "drm_module.h"
#include "rga_helper.h"
#include "uart_helper.h"
#include "camera_helper.h"
#include "usb_device_helper.h"

#define APP_ERR(x,arg...) \
	ERR_MSG("[APP Error] " x,##arg)

#define APP_DBG(x,arg...) \
	DBG_MSG("[APP Debug] " x,##arg)

/*
 * CL_Engine_Datasheet_Ver2.2(적외선검출기 640 17um).pdf 참조
 *	+----------------------------------------------------------------+
 * | Byte 1 | Byte 2 | Byte 3 | Byte 4 | Byte 5 | Byte 6 |  Byte 7  |
 *	+----------------------------------------------------------------+
 * |  0xFF  |  0x00  |     Address     |       Data      | Checksum |
 *	+----------------------------------------------------------------+
 *	Checksum = Add Byte2 ~ Byte 6 
 */ 

typedef struct {
	uint8_t sync;
	uint8_t address;
	uint8_t cmd;
	uint8_t cmd2;
	uint8_t data;
	uint8_t data2;
	uint8_t checksum;
} __attribute__((packed)) ir_pkt;

typedef struct {
	int				fd;
	mqd_t				mfd;
	const char*		dev_name;
} icd_ctx_t;

typedef struct {
	uint8_t *cam_buf;
	uint8_t *usb_buf;
	uint8_t *npu_buf;
	int cam_fd;
	usb_ctx_t			usb_ctx;
	icd_ctx_t			icd_host;
	icd_ctx_t			icd_ir;
	drm_dev_t			*modeset_list;
	rknn_context		rknn_ctx;
	pthread_cond_t		thread_cond;
	pthread_mutex_t	mutex_lock;
	pthread_t			cam_thread;
	pthread_t			usb_thread;
	pthread_t			uart_thread;
	sigset_t				pending;
	bool thread_status;
} device_ctx_t;

int app_init(device_ctx_t **ctx);
int app_run(device_ctx_t *ctx);
void app_exit(device_ctx_t *ctx);

#endif
