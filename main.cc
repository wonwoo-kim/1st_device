#include "app.h"

int uart_fd;

void uart_callback(int status)
{
	int ret, i;
	ssize_t bytes;
	uint8_t tmp[255];

	bytes = read(uart_fd, tmp, 255);

	printf("UART : ");
	for(i = 0; i < bytes; i++)
		printf("%02x ", tmp[i]);
	printf("\n");
}

int main(int argc, char** argv)
{
	device_ctx_t *app_ctx;
	ir_pkt pkt;
	int ret;

	pkt = (ir_pkt) {
				.sync = 0xFF, .address = 0x00,
				.cmd = 0x00, .cmd2 = 0x5E,
				.data = 0x00, .data2 = 0x01,
				.checksum = (0x00 + 0x00 + 0x5E + 0x00 + 0x01)
			};

	ret = app_init(&app_ctx);
	if (ret != 0) {
		APP_ERR("Failed to initialize application context\n");
		exit(EXIT_FAILURE);
	}

	write(app_ctx->icd_ir.fd, &pkt, sizeof(ir_pkt));

	app_run(app_ctx);
	app_exit(app_ctx);

	printf("Done\n");

	return 0;
}
