#include "app.h"

void ir_preprocessing(uint8_t *arr, uint16_t width, uint16_t height,
								uint8_t thr, float level)
{
	register uint32_t i;
	int pixel, contrast;

	for (i = 0; i < width*height; i++) {
		contrast = abs(arr[i] - thr) * level;

		if (arr[i] < thr) {
			arr[i] = (((int)arr[i] + contrast) < 0) ? 0 : (arr[i] - contrast);
		}
	}
}

void *camera_loop(void *arg)
{
	int ret;
	uint8_t *ptr;
	ssize_t size;
	rga_transform_t src, dst;
	device_ctx_t *ctx = (device_ctx_t*)arg;
	drm_dev_t *mode = ctx->modeset_list;

	CLEAR(src); CLEAR(dst);
	src = {	.data = ctx->npu_buf, .width = 480, .height = 640, 
				.format = RK_FORMAT_YCbCr_420_P, .direction = 0 };
	dst = {	.data = ptr, .width = 1080, .height = 1920, 
				.format = RK_FORMAT_BGRA_8888, .direction = 0 };

	ret = camera_streamon(ctx->cam_fd, 3);

	while(1) {
		ret = camera_get_frame_helper(ctx->cam_fd, &(ctx->usb_buf), &size);

//		ir_preprocessing(ctx->usb_buf, 640, 480, 120, 0.5);
		rknn_run_helper(ctx->rknn_ctx, ctx->usb_buf, 640*480, ctx->npu_buf);

		dst.data = mode->bufs[mode->front_buf ^ 1].map;
		rga_transform(&src, &dst);
		drm_flip(mode);

#if 0
		sigpending(&(ctx->pending));
		if (sigismember(&(ctx->pending), SIGINT)) {
			printf("\n\nSIGINT Called!\n\n\n");
			pthread_exit(NULL);
		}
#endif
	}
}

void *usb_loop(void *arg)
{
	int ret;
	device_ctx_t *ctx = (device_ctx_t*)arg;

	usb_device_wait_ready(&(ctx->usb_ctx));

	while(1) 
	{
		pthread_mutex_lock(&(ctx->mutex_lock));
		usb_device_send(&(ctx->usb_ctx), ctx->npu_buf, 307200);
		pthread_mutex_unlock(&(ctx->mutex_lock));
	}
}

void *uart_loop(void *arg)
{
	fd_set rfds;
	uint8_t tmp[100];
	int ret, fd_max;
	device_ctx_t *ctx = (device_ctx_t *)arg;

	fd_max = MAX(ctx->icd_ir.fd, ctx->icd_host.fd);

	while(1) 
	{
		FD_ZERO(&rfds);
		FD_SET(ctx->icd_ir.fd, &rfds);
		FD_SET(ctx->icd_host.fd, &rfds);

		select(fd_max + 1, &rfds, NULL, NULL, NULL);

		if (FD_ISSET(ctx->icd_host.fd, &rfds)) {
			ret = read(ctx->icd_host.fd, tmp, 100);
			if (ret > 0) {
				printf("HOST READ : ");
				for (int i = 0; i < ret; i++) {
					printf("0x%02X ", tmp[i]);
				}
				printf("\n");
			}
		}
		else if (FD_ISSET(ctx->icd_ir.fd, &rfds)) {
			ret = read(ctx->icd_ir.fd, tmp, 100);
			if (ret > 0) {
				printf("IR READ : ");
				for (int i = 0; i < ret; i++) {
					printf("0x%02X ", tmp[i]);
				}
				printf("\n");
			}
		}
	}


}

int app_init(device_ctx_t **ctx)
{
	int ret;
	sigset_t set;
	device_ctx_t *tmp;

	/* Allocate memory to app context structure */
	tmp = (device_ctx_t*) calloc(1, sizeof(device_ctx_t));
	if (tmp == NULL) {	/* ENOMEM */
		APP_ERR("Failed to allocate memory to app ctx\n");
		return -1;
	}

	/* RK1808 Host <-> RK1808 Device */
	ret = uart_init_helper("/dev/ttyS1", B38400, NULL, &tmp->icd_host.fd);
	if (ret != 0) {
		APP_ERR("Failed to initialize UART\n");
		return -1;
	}

	/* RK1808 Device <-> IR RS-232 */
	ret = uart_init_helper("/dev/ttyS2", B38400, NULL, &tmp->icd_ir.fd);
	if (ret != 0) {
		APP_ERR("Failed to initialize UART\n");
		return -1;
	}

	/* Allocate memory to usb/camera buffer */
	tmp->cam_buf = (uint8_t*) calloc(1, 640*481*2);
	tmp->usb_buf = (uint8_t*) calloc(1, 640*481*2);
	tmp->npu_buf = (uint8_t*) calloc(1, 640*481*2);
	if ((tmp->cam_buf == NULL) || (tmp->usb_buf == NULL)) {	/* ENOMEM */
		APP_ERR("Failed to allocate memory to cam/usb buffer\n");
		return -1;
	}

	memset(tmp->cam_buf, 128, 640*481*2);
	memset(tmp->usb_buf, 128, 640*481*2);
	memset(tmp->npu_buf, 128, 640*481*2);

	ret = rga_init_helper();
	if (ret != 0) {
		APP_ERR("Failed to initialize RGA\n");
		return -1;
	}

	/* USB Device initialize(Gadget, UDC) */
	ret = usb_device_init("/dev/usb-ffs/test", &(tmp->usb_ctx));
	if (ret != 0) {
		APP_ERR("Failed to initialize usb device-side\n");
		return -1;
	}

	/* Camera Device initilize(Width/Height, Format, Buffer) */
	ret = camera_init_helper("/dev/video0", &(tmp->cam_fd), 3, 640, 481, V4L2_PIX_FMT_YUV422P);
	if (ret != 0) {
		APP_ERR("Failed to initialize camera device\n");
		return -1;
	}

	ret = drm_init("/dev/dri/card0", &(tmp->modeset_list));
	if (ret != 0) {
		APP_ERR("Failed to initialize DRM\n");
		return -1;
	}

	ret = rknn_init_helper("deepfusion_480p.rknn", &(tmp->rknn_ctx));
	if (ret != 0) {
		APP_ERR("Failed to initialize RKNN\n");
		return -1;
	}
#if 0
	sigemptyset(&(tmp->pending));
	/* Blocking SIGINT */
	sigemptyset(&set);
	sigaddset(&set, SIGINT);

	ret = pthread_sigmask(SIG_BLOCK, &set, NULL);
	if (ret != 0) {
		APP_ERR("pthread_sigmask() failed\n");
		return -1;
	}
#endif
	/* Initialize mutex */
	ret = pthread_mutex_init(&(tmp->mutex_lock), NULL);
	if (ret != 0) {
		APP_ERR("Failed to initialize mutex\n");
		return -1;
	}

	ret = pthread_cond_init(&(tmp->thread_cond), NULL);
	if (ret != 0) {
		APP_ERR("Failed to initialize conditional variable\n");
		return -1;
	}

	*ctx = tmp;

	APP_DBG("Application context initialized\n");

	return 0;
}

int app_run(device_ctx_t *ctx)
{
	int ret;

	ret = pthread_create(&(ctx->cam_thread), NULL, camera_loop, (void *)ctx);
	if (ret < 0) {
		perror("Failed to create thread");
		exit(EXIT_FAILURE);
	}

	ret = pthread_create(&(ctx->usb_thread), NULL, usb_loop, (void *)ctx);
	if (ret < 0) {
		perror("Failed to create thread");
		exit(EXIT_FAILURE);
	}

	ret = pthread_create(&(ctx->uart_thread), NULL, uart_loop, (void *)ctx);
	if (ret < 0) {
		perror("Failed to create thread");
		exit(EXIT_FAILURE);
	}

	pthread_join(ctx->cam_thread, NULL);
	pthread_join(ctx->usb_thread, NULL);

	return 0;
}

void app_exit(device_ctx_t *ctx)
{
	int ret;

	APP_DBG("Exit Application\n");

	ret = pthread_cancel(ctx->cam_thread);
	if (ret != 0) {
		printf("error \n");
	}

	ret = pthread_cancel(ctx->usb_thread);
	if (ret != 0) {
		printf("error \n");
	}

	usb_device_deinit(&(ctx->usb_ctx));

	free(ctx->cam_buf);
	free(ctx->usb_buf);

	free(ctx);

	return;
}
