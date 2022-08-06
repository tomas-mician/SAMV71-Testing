#include "atmel_start.h"
#include "atmel_start_pins.h"
// #include "tc_lite.h"
#include <string.h>

#define TEST_SWITCH ('s')
#define TEST_1 ('1') /* Send 1 byte */
#define TEST_2 ('2') /* Send 64 bytes */
#define TEST_3 ('3') /* Send 512 bytes */
#define TEST_4 ('4') /* Send 2048 bytes */

#define WAIT_CONN 0
#define OPEN_PORT 1
#define SWITCH_PORT 2
#define DO_TEST 3
#define ERROR_WAIT 4


#define TC_CHANNEL_0 0
#define TC_CHANNEL_1 1
#define TC_CHANNEL_2 2
#define TC_CHANNEL_COUNT 3


static struct cdchf_acm *cdc_inst[] = {&USB_HOST_CDC_ACM_0_inst, &USB_HOST_CDC_ACM_1_inst};
#define N_CDC_INST (sizeof(cdc_inst) / sizeof(struct cdchf_acm *))
static uint8_t gport = 0;

COMPILER_ALIGNED(4)
static uint8_t wr_data[2048];

COMPILER_ALIGNED(4)
static uint8_t rd_data[2048];

static uint32_t wr_len   = 0;
static uint32_t wr_count = 0;
static uint32_t rd_count = 0;
static uint32_t rd_total = 0;
static uint8_t  chr;
static uint8_t  wr_chr = 'a';

const char *hints[] = {
    "Connect USB CDC ACM device to start or switch port to check.\r\n",
    "Connected. Try to open current port.\r\n",
    "",
    "Press s to switch port; 1~4 to send 1~2K bytes\r\n",
    "Press s to switch port; 1~4 to send 1~2K bytes\r\n",
};

bool read_key(uint8_t *key)
{
	if (usart_sync_is_rx_not_empty(&EDBG_COM)) {
		EDBG_COM.io.read(&EDBG_COM.io, key, 1);
		return true;
	}
	return false;
}

static void write_buf(uint8_t *buf, uint16_t len)
{
	EDBG_COM.io.write(&EDBG_COM.io, buf, len);
}

void port_read_complete(struct cdchf_acm *cdc, uint32_t count)
{
	if (count) {
		rd_count = count;
		rd_total += count;
	} else if (rd_total) {
		printf("\r\nRx total: %u\r\n", (unsigned)rd_total);
		rd_total = 0;
	}
}

void port_write_complete(struct cdchf_acm *cdc, uint32_t count)
{
	/* Possible timeout if bandwidth is not enough */
	wr_count += count;
	if (wr_count >= wr_len) {
		wr_len   = 0;
		wr_count = 0;
		cdchf_acm_write_flush(cdc);
	} else {
		cdchf_acm_write(cdc, &wr_data[wr_count], wr_len - wr_count);
	}
}

uint8_t port_open(uint8_t port)
{
	static usb_cdc_line_coding_t lncoding = {115200, CDC_STOP_BITS_1, CDC_PAR_NONE, 8};
	if (!cdchf_acm_is_enabled(cdc_inst[port])) {
		printf("Port %d not found\r\n", port);
		return ERROR_WAIT;
	}
	if (ERR_NONE != cdchf_acm_open(cdc_inst[port], &lncoding)) {
		printf("Failed to open port %d\r\n", port);
		return ERROR_WAIT;
	}
	while (1) {
		if (cdchf_acm_is_error(cdc_inst[port])) {
			printf("Error while openning port %d\r\n", port);
			return ERROR_WAIT;
		}
		if (!cdchf_acm_is_enabled(cdc_inst[port])) {
			printf("Detach while openning port %d\r\n", port);
			return WAIT_CONN;
		}
		if (cdchf_acm_is_open(cdc_inst[port])) {
			printf("Port %d openned\r\n", port);
			return DO_TEST;
		}
	}
	return ERROR_WAIT;
}

uint8_t port_switch(uint8_t port)
{
	if (ERR_NONE != cdchf_acm_close(cdc_inst[port])) {
		printf("Failed to close port %d\r\n", port);
		return ERROR_WAIT;
	}
	while (1) {
		if (cdchf_acm_is_error(cdc_inst[port])) {
			printf("Error while closing port %d\r\n", port);
			return ERROR_WAIT;
		}
		if (!cdchf_acm_is_enabled(cdc_inst[port])) {
			printf("Detach while closing port %d\r\n", port);
			if (++gport >= N_CDC_INST) {
				gport = 0;
			}
			printf("Switch port -> %u\r\n", (unsigned)gport);
			return WAIT_CONN;
		}
		if (!cdchf_acm_is_open(cdc_inst[port])) {
			printf("Port %d closed\r\n", port);
			if (++gport >= N_CDC_INST) {
				gport = 0;
			}
			printf("Switch port -> %u\r\n", (unsigned)gport);
			return WAIT_CONN;
		}
	}
	return ERROR_WAIT;
}

uint8_t do_test(uint8_t port, uint8_t *key)
{
	if (!cdchf_acm_is_enabled(cdc_inst[port])) {
		printf("\r\nDisconnected during test\r\n");
		rd_count = 0;
		return WAIT_CONN;
	}
	if (rd_count) {
		printf("\r\nRx %u:\r\n> ", (unsigned)rd_count);
		write_buf(rd_data, rd_count);
		printf("\r\n");
		rd_count = 0;
	}
	if (!cdchf_acm_is_reading(cdc_inst[port])) {
		cdchf_acm_read(cdc_inst[port], rd_data, sizeof(rd_data));
	}
	if (!cdchf_acm_is_writing(cdc_inst[port])) {
		uint8_t  cmd = *key;
		uint32_t i;
		*key = 0;
		if (cmd == 0) {
			return DO_TEST;
		}
		switch (cmd) {
		case TEST_1:
			wr_len = 1;
			break;
		case TEST_2:
			wr_len = 64;
			break;
		case TEST_3:
			wr_len = 512;
			break;
		case TEST_4:
			wr_len = 2048;
			break;
		default:
			return DO_TEST;
		}
		for (i = 0; i < wr_len; i++) {
			wr_data[i] = ((wr_chr - 'a' + i) % 26) + 'a';
		}
		wr_chr = ((wr_chr + 1 - 'a') % 26) + 'a';
		if (wr_len > 1) {
			printf("\r\nTx %u: %c .. %c\r\n", (unsigned)wr_len, wr_data[0], wr_data[wr_len - 1]);
		} else {
			printf("\r\nTx 1: %c\r\n", wr_data[0]);
		}
		wr_count = 0;
		cdchf_acm_write(cdc_inst[port], wr_data, wr_len);
	}

	return DO_TEST;
}

/**
 * \brief TC IRQ callback type
 */
typedef void (*tc_channel_ptr)(uint32_t);

/* TC2 channel interrupt callback array */
tc_channel_ptr tc2_channel_cb[TC_CHANNEL_COUNT];

/**
 * \brief Initialize TC interface
 */
int8_t TIMER_0_init()
{

	hri_tc_write_BCR_reg(TC2, 1 << TC_BCR_SYNC_Pos);

	hri_tc_write_BMR_reg(TC2, 3 << TC_BMR_TC0XC0S_Pos | 3 << TC_BMR_TC1XC1S_Pos);

	/* TC2 Channel 0 configuration */

	hri_tc_write_CMR_reg(TC2, TC_CHANNEL_0, 5 << TC_CMR_TCCLKS_Pos | 1 << TC_CMR_CAPTURE_CPCTRG_Pos);

	hri_tc_write_RC_reg(TC2, TC_CHANNEL_0, 0x1f4 << TC_RC_RC_Pos);

	hri_tc_write_IMR_reg(TC2, TC_CHANNEL_0, 1 << TC_IMR_CPCS_Pos);

	/* TC2 Channel 1 configuration */

	hri_tc_write_CMR_reg(TC2,
	                     TC_CHANNEL_1,
	                     6 << TC_CMR_TCCLKS_Pos | 1 << TC_CMR_WAVEFORM_EEVT_Pos | 2 << TC_CMR_WAVEFORM_WAVSEL_Pos
	                         | 1 << TC_CMR_WAVE_Pos | 3 << TC_CMR_WAVEFORM_BCPC_Pos);

	hri_tc_write_RC_reg(TC2, TC_CHANNEL_1, 0x1f4 << TC_RC_RC_Pos);

	/* TC2 Channel 2 configuration */

	hri_tc_write_CMR_reg(TC2,
	                     TC_CHANNEL_2,
	                     3 << TC_CMR_TCCLKS_Pos | 2 << TC_CMR_WAVEFORM_WAVSEL_Pos | 1 << TC_CMR_WAVE_Pos
	                         | 3 << TC_CMR_WAVEFORM_ACPC_Pos);

	hri_tc_write_RC_reg(TC2, TC_CHANNEL_2, 0x24a << TC_RC_RC_Pos);

	tc2_channel_cb[TC_CHANNEL_0] = NULL;
	NVIC_DisableIRQ(TC6_IRQn);
	NVIC_ClearPendingIRQ(TC6_IRQn);
	NVIC_EnableIRQ(TC6_IRQn);

	return 0;
}

void start_timer(const void *hw, uint8_t channel)
{
	if (channel < TC_CHANNEL_COUNT) {
		hri_tc_write_CCR_reg(hw, channel, TC_CCR_CLKEN | TC_CCR_SWTRG);
	}
}

void stop_timer(const void *hw, uint8_t channel)
{
	if (channel < TC_CHANNEL_COUNT) {
		hri_tc_write_CCR_reg(hw, channel, TC_CCR_CLKDIS);
	}
}

void tc_register_callback(void *hw, uint8_t channel, void *cb)
{
	ASSERT(hw && (channel < TC_CHANNEL_COUNT));
	if (hw == TC2) {
		tc2_channel_cb[channel] = cb;
	}
}

/* TC2 Channel 0 interrupt handler */
void TC6_Handler(void)
{
	uint32_t status;
	status = hri_tc_get_SR_reg(TC2, TC_CHANNEL_0, TC_SR_Msk);
	if (tc2_channel_cb[TC_CHANNEL_0] != NULL) {
		tc2_channel_cb[TC_CHANNEL_0](status);
	}
}


int main(void)
{
	int8_t step = 0, last = -1;

	atmel_start_init();

	usbhc_start(&USB_HOST_CORE_INSTANCE_inst);

	while (1) {
		if (step != last) {
			last = step;
			printf("Current port: %u\r\n", (unsigned)gport);
			printf(hints[step]);
		}

		if (read_key(&chr)) {
			if (chr == TEST_SWITCH) {
				step = SWITCH_PORT;
			}
		}

		switch (step) {
		case WAIT_CONN:
			if (cdchf_acm_is_enabled(cdc_inst[gport])) {
				printf("Port %d connected\r\n", gport);

				cdchf_acm_register_callback(cdc_inst[gport], CDCHF_ACM_READ_CB, (FUNC_PTR)port_read_complete);
				cdchf_acm_register_callback(cdc_inst[gport], CDCHF_ACM_WRITE_CB, (FUNC_PTR)port_write_complete);
				step = OPEN_PORT;
			}
			break;
		case OPEN_PORT:
			step = port_open(gport);
			break;
		case SWITCH_PORT:
			step = port_switch(gport);
			break;
		case DO_TEST:
			step = do_test(gport, &chr);
			break;
		default:
			break;
		}
	}
}
