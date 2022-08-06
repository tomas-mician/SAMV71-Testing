/**
 * \file
 *
 * \brief SAM USBHS HPL
 *
 * Copyright (c) 2016-2019 Microchip Technology Inc. and its subsidiaries.
 *
 * \asf_license_start
 *
 * \page License
 *
 * Subject to your compliance with these terms, you may use Microchip
 * software and any derivatives exclusively with Microchip products.
 * It is your responsibility to comply with third party license terms applicable
 * to your use of third party software (including open source software) that
 * may accompany Microchip software.
 *
 * THIS SOFTWARE IS SUPPLIED BY MICROCHIP "AS IS". NO WARRANTIES,
 * WHETHER EXPRESS, IMPLIED OR STATUTORY, APPLY TO THIS SOFTWARE,
 * INCLUDING ANY IMPLIED WARRANTIES OF NON-INFRINGEMENT, MERCHANTABILITY,
 * AND FITNESS FOR A PARTICULAR PURPOSE. IN NO EVENT WILL MICROCHIP BE
 * LIABLE FOR ANY INDIRECT, SPECIAL, PUNITIVE, INCIDENTAL OR CONSEQUENTIAL
 * LOSS, DAMAGE, COST OR EXPENSE OF ANY KIND WHATSOEVER RELATED TO THE
 * SOFTWARE, HOWEVER CAUSED, EVEN IF MICROCHIP HAS BEEN ADVISED OF THE
 * POSSIBILITY OR THE DAMAGES ARE FORESEEABLE.  TO THE FULLEST EXTENT
 * ALLOWED BY LAW, MICROCHIP'S TOTAL LIABILITY ON ALL CLAIMS IN ANY WAY
 * RELATED TO THIS SOFTWARE WILL NOT EXCEED THE AMOUNT OF FEES, IF ANY,
 * THAT YOU HAVE PAID DIRECTLY TO MICROCHIP FOR THIS SOFTWARE.
 *
 * \asf_license_stop
 *
 */

#include <compiler.h>
#include <hal_atomic.h>
#include <string.h>

#include <utils_assert.h>
#include <peripheral_clk_config.h>
#include "hpl_usb_host.h"
#include <hpl_usbhs_host.h>

#undef DEPRECATED
#define DEPRECATED(macro, message)

#ifndef USB_SPEED_HIGH_SPEED
#define USB_SPEED_HIGH_SPEED 2
#endif

#ifndef USBHS_RAM_ADDR
#define USBHS_RAM_ADDR 0xA0100000u
#endif

/** Endpoint/pipe FIFO access range */
#define USBHS_RAM_EP_SIZE 0x8000

/** Total DPRAM for endpoint/pipe to allocate */
#define USBHS_DPRAM_SIZE 4096

/** Max DMA transfer size */
#define USBHS_DMA_TRANS_MAX 0x10000

/** Check if the address is 32-bit aligned. */
#define _usb_is_aligned(val) (((uint32_t)(val)&0x3) == 0)

#if (CONF_USBHS_SRC == CONF_SRC_USB_48M)
#endif

/** Check if the buffer supports USB DMA. */
#define _usbhs_buf_dma_sp(buf) (1) /* SRAM/Flash/EBI all supports DMA */

/** Timeout between control data packets : 500ms */
#define USB_CTRL_DPKT_TIMEOUT (500)
/** Timeout of status packet : 50ms */
#define USB_CTRL_STAT_TIMEOUT (50)

/** Get 64-, 32-, 16- or 8-bit access to FIFO data register of selected endpoint.
 * \param[in] epn   Endpoint of which to access FIFO data register
 * \param[in] scale Data scale in bits: 64, 32, 16 or 8
 * \return Volatile 64-, 32-, 16- or 8-bit data pointer to FIFO data register
 * \note It is up to the user of this macro to make sure that all accesses
 *       are aligned with their natural boundaries except 64-bit accesses which
 *       require only 32-bit alignment.
 *       It is up to the user of this macro to make sure that used HSB
 *       addresses are identical to the DPRAM internal pointer modulo 32 bits.
 */
#define _usbhs_get_pep_fifo_access(epn, scale)                                                                         \
	(((volatile uint##scale##_t(*)[USBHS_RAM_EP_SIZE / ((scale) / 8)]) USBHS_RAM_ADDR)[(epn)])

/**
 * \brief Dummy callback function
 */
static void _dummy_func_no_return(uint32_t unused0, uint32_t unused1)
{
	(void)unused0;
	(void)unused1;
}

#include "hpl_usbhs_host.h"

#if CONF_USB_H_NUM_HIGH_BW_PIPE_SP && (!CONF_USB_H_DMA_SP)
#warning High bandwidth must be enabled with DMA support, please check your settings
#endif

#define _HPL_USB_H_HBW_SP (CONF_USB_H_NUM_HIGH_BW_PIPE_SP && CONF_USB_H_DMA_SP)

#ifndef USBHS_HSTPIPISR_UNDERFI
#define USBHS_HSTPIPISR_UNDERFI (1u << 2)
#endif

#if _HPL_USB_H_HBW_SP
/** Number of DMA descriptors
 *  First two pipe: 3 descriptors for 3 banks.
 *  Others: 2 descriptors each.
 */
#define _HPL_USB_H_N_DMAD                                                                                              \
	((CONF_USB_H_NUM_HIGH_BW_PIPE_SP < 3) ? (CONF_USB_H_NUM_HIGH_BW_PIPE_SP * 3)                                       \
	                                      : (CONF_USB_H_NUM_HIGH_BW_PIPE_SP * 2 + 2))

/* DMA descriptors */
UsbhsHstdma _usbhs_dma_desc[_HPL_USB_H_N_DMAD];
/* DMA descriptors usage: pipe index, -1 for free */
int8_t _usbhs_dma_user[_HPL_USB_H_N_DMAD];

/** \internal
 *  \brief Free DMA descriptors for the pipe
 *  \param[in] pi   Pipe index, set to -1 to free all
 */
static void _usb_h_ll_free(int8_t pi);

/** \internal
 *  \brief Allocate DMA descriptors for the pipe, or get allocated one
 *  \param[in] pi   Pipe index
 *  \param[in] bank Number of banks
 *  \return Pointer to DMA descriptor link list
 */
static UsbhsHstdma *_usb_h_ll_get(int8_t pi, uint8_t bank);

/** \internal
 *  \brief Handles linked list DMA OUT
 *  \param pipe Pointer to pipe instance
 *  \param ll   Pointer to linked list used
 */
static void _usb_h_ll_dma_out(struct usb_h_pipe *pipe, UsbhsHstdma *ll);
#endif

/** Pointer to hpl device */
struct usb_h_desc *_usb_h_dev = NULL;

/**
 * \internal
 * \brief End the transfer: set state to idle and invoke callback
 * \param pipe Pointer to pipe instance
 * \param code Status code
 */
static void _usb_h_end_transfer(struct usb_h_pipe *pipe, int32_t code);

/** \internal
 *  \brief Abort on going transfer
 *  \param pipe Pointer to pipe instance
 *  \param code Status code
 */
static void _usb_h_abort_transfer(struct usb_h_pipe *pipe, int32_t code);

/** \internal
 *  \brief Load transfer buffer, size and count
 *  \param pipe Pointer to pipe instance
 *  \param x_buf   Pointer to location to put transfer buffer pointer
 *  \param x_size  Pointer to location to put transfer size
 *  \param x_count Pointer to location to put transfer count
 */
static inline void _usb_h_load_x_param(struct usb_h_pipe *pipe, uint8_t **x_buf, uint32_t *x_size, uint32_t *x_count);

/** \internal
 *  \brief Save modified transfer count
 *  \param pipe Pointer to pipe instance
 *  \param x_count Pointer to location to put transfer count
 */
static inline void _usb_h_save_x_param(struct usb_h_pipe *pipe, uint32_t x_count);

/** \brief Send ZLP OUT packet
 *  \param pipe Pointer to pipe instance
 */
static void _usb_h_out_zlp_ex(struct usb_h_pipe *pipe);

/** \internal
 *  \brief Start waiting IN packets
 *  \param pipe Pointer to pipe instance
 */
static void _usb_h_ctrl_in_req(struct usb_h_pipe *pipe);

/** \brief OUT packet process
 *  \param pipe Pointer to pipe instance
 */
static void _usb_h_out(struct usb_h_pipe *pipe);

/** \brief IN packet process
 *  \param pipe Pointer to pipe instance
 */
static void _usb_h_in(struct usb_h_pipe *pipe);

/** \brief Send control setup packet
 *  \param pipe Pointer to pipe instance
 */
static void _usb_h_setup_ex(struct usb_h_pipe *pipe);

#if CONF_USB_H_DMA_SP
/** \brief OUT packet process
 *  \param pipe Pointer to pipe instance
 *  \param end  Force to end the transfer
 */
static void _usb_h_dma(struct usb_h_pipe *pipe, bool end);
#endif

/** \internal
 *  \brief Apply pipe configuration to physical pipe
 *  \param     drv      Pointer to driver instance
 *  \param[in] phy      Physical pipe number (index)
 *  \param[in] dev      Device address
 *  \param[in] ep       Endpoint address
 *  \param[in] type     Transfer type
 *  \param[in] size     Endpoint max packet size
 *  \param[in] n_bank   Number of banks
 *  \param[in] interval Interval of ms
 *  \param[in] speed    Transfer speed
 *  \return \c true if pipe configuration is OK
 */
static bool _usbhs_pipe_configure(struct usb_h_desc *drv, uint8_t phy, uint8_t dev, uint8_t ep, uint8_t type,
                                  uint16_t size, uint8_t n_bank, uint8_t interval, uint8_t speed);

/**
 *  \brief Suspend device connected on a root hub port
 *  \param drv Pointer to driver instance
 */
static void _usbhs_suspend(struct usb_h_desc *drv);

/** \internal
 *  \brief Reset pipes
 *  \param     drv        Pointer to driver instance
 *  \param[in] warm_reset Handles runtime USB reset
 */
static void _usb_h_reset_pipes(struct usb_h_desc *drv, bool warm_reset)
{
	struct _usb_h_prvt *pd = (struct _usb_h_prvt *)drv->prvt;
	struct usb_h_pipe * p;
	uint8_t             i;
	/* Reset pipes */
	for (i = 0; i < pd->pipe_pool_size; i++) {
		p = &pd->pipe_pool[i];
		if (warm_reset) {
			/* Skip free pipes */
			if (p->x.general.state == USB_H_PIPE_S_FREE) {
				continue;
			}
			/* Restore physical pipe configurations */
			_usbhs_pipe_configure(drv, i, p->dev, p->ep, p->type, p->max_pkt_size, p->bank, p->interval, p->speed);
			hri_usbhs_set_HSTIMR_reg(drv->hw, USBHS_HSTIMR_PEP_0 << i);
			/* Abort transfer (due to reset) */
			if (p->type == 0 /* Control */) {
				/* Force a callback for control endpoints */
				p->x.general.state = USB_H_PIPE_S_SETUP;
			}
			_usb_h_end_transfer(p, USB_H_RESET);
		} else {
			p->done            = (usb_h_pipe_cb_xfer_t)_dummy_func_no_return;
			p->x.general.state = USB_H_PIPE_S_FREE;
		}
	}
	if (warm_reset) {
		return;
	}
#if _HPL_USB_H_HBW_SP
	/* Free all DMA allocates */
	_usb_h_ll_free(-1);
#endif
}

/** \internal
 *  \brief Add one Control Request user
 *  \param drv Pointer to driver instance
 */
static inline void _usb_h_add_req_user(struct usb_h_desc *drv)
{
	struct _usb_h_prvt *pd = (struct _usb_h_prvt *)drv->prvt;
	pd->n_ctrl_req_user++;
}

/** \internal
 *  \brief Remove one Control Request user
 *  \param drv Pointer to driver instance
 */
static inline void _usb_h_rm_req_user(struct usb_h_desc *drv)
{
	struct _usb_h_prvt *pd = (struct _usb_h_prvt *)drv->prvt;
	if (pd->n_ctrl_req_user) {
		pd->n_ctrl_req_user--;
	}
}

/** \internal
 *  \brief Add one SOF IRQ user and enable SOF interrupt
 *  \param drv Pointer to driver instance
 */
static inline void _usb_h_add_sof_user(struct usb_h_desc *drv)
{
	struct _usb_h_prvt *pd = (struct _usb_h_prvt *)drv->prvt;
	pd->n_sof_user++;
	hri_usbhs_set_HSTIMR_HSOFIE_bit(drv->hw);
}

/** \internal
 *  \brief Remove one SOF IRQ user
 *  \param drv Pointer to driver instance
 */
static inline void _usb_h_rm_sof_user(struct usb_h_desc *drv)
{
	struct _usb_h_prvt *pd = (struct _usb_h_prvt *)drv->prvt;
	if (pd->n_sof_user) {
		pd->n_sof_user--;
	}
}

/** \internal
 *  \brief Modify suspend delay (suspend_start)
 *  \param pd Pointer to private data instance
 */
static inline void _usb_h_set_suspend(struct usb_h_desc *drv, struct _usb_h_prvt *pd, int8_t start)
{
	if (pd->suspend_start > 0 && start == 0) {
		_usb_h_rm_sof_user(drv);
	}
	if (pd->suspend_start == 0 && start > 0) {
		_usb_h_add_sof_user(drv);
	}
	pd->suspend_start = start;
}

/** \internal
 *  \brief Modify resume delay (resume_start)
 *  \param pd Pointer to private data instance
 */
static inline void _usb_h_set_resume(struct usb_h_desc *drv, struct _usb_h_prvt *pd, int8_t start)
{
	if (pd->resume_start > 0 && start == 0) {
		_usb_h_rm_sof_user(drv);
	}
	if (pd->resume_start == 0 && start > 0) {
		_usb_h_add_sof_user(drv);
	}
	pd->resume_start = start;
}

/** \internal
 *  \brief Manage delayed suspend
 *  \param drv Pointer to driver instance
 *  \param pd Pointer to private data instance
 */
static inline void _usb_h_delayed_suspend(struct usb_h_desc *drv, struct _usb_h_prvt *pd)
{
	if (--pd->suspend_start == 0) {
		_usb_h_rm_sof_user(drv); /* SOF user: delayed suspend */

		/* In case of high CPU frequency,
		 * the current Keep-Alive/SOF can be always on-going
		 * then wait end of SOF generation
		 * to be sure that disable SOF has been accepted
		 */
		uint32_t sp  = hri_usbhs_get_SR_reg(drv->hw, USBHS_SR_SPEED_Msk);
		uint8_t  pos = (sp == USBHS_SR_SPEED_HIGH_SPEED) ? 13 : 114;
		while (pos < hri_usbhs_get_HSTFNUM_FLENHIGH_bf(drv->hw, 0xFFFF)) {
			if (hri_usbhs_get_HSTISR_reg(drv->hw, USBHS_HSTISR_DCONNI)) {
				break;
			}
		}
		hri_usbhs_clear_HSTCTRL_SOFE_bit(drv->hw);
		/* When SOF is disabled, the current transmitted packet may
		 * cause a resume.
		 * Wait for a while to check this resume status and clear it.
		 */
		for (pos = 0; pos < 15; pos++) {
			__DMB();
			if (hri_usbhs_get_HSTISR_reg(drv->hw, USBHS_HSTICR_HWUPIC | USBHS_HSTICR_RSMEDIC | USBHS_HSTICR_RXRSMIC)) {
				break;
			}
		}
		/* ACK wake up and resumes interrupts */
		hri_usbhs_write_HSTICR_reg(drv->hw, USBHS_HSTICR_HWUPIC | USBHS_HSTICR_RSMEDIC | USBHS_HSTICR_RXRSMIC);
		/* Enable wakeup/resumes interrupts
		 * Enable reset interrupt
		 * Enable disconnect interrupt
		 */
		hri_usbhs_set_HSTIMR_reg(drv->hw,
		                         USBHS_HSTIER_HWUPIES | USBHS_HSTIER_RSMEDIES | USBHS_HSTIER_RXRSMIES
		                             | USBHS_HSTIER_RSTIES | USBHS_HSTIER_DDISCIES);
		/* Freeze clock */
		hri_usbhs_set_CTRL_FRZCLK_bit(drv->hw);
		/* Notify suspend */
		drv->rh_cb(drv, 1, 2); /* Port 1 PORT_SUSPEND changed */
	}
}

/** \internal
 *  \brief Manage delayed resume
 *  \param drv Pointer to driver instance
 *  \param pd Pointer to private data instance
 */
static inline void _usb_h_delayed_resume(struct usb_h_desc *drv, struct _usb_h_prvt *pd)
{
	if (--pd->resume_start == 0) {
		_usb_h_rm_sof_user(drv); /* SOF user: delayed resume */

		/* Restore pipes unfrozen */
		for (uint8_t pi = 0; pi < CONF_USB_H_NUM_PIPE_SP; pi++) {
			if (pd->pipes_unfreeze & (1 << pi)) {
				hri_usbhs_write_HSTPIPIDR_reg(drv->hw, pi, USBHS_HSTPIPIMR_PFREEZE);
			}
		}
		/* Notify resume */
		drv->rh_cb(drv, 1, 2); /* Port 1 PORT_SUSPEND changed */
	}
}

/** \internal
 *  \brief Manage control endpoints timeouts
 */
static inline void _usb_h_ctrl_timeout(struct usb_h_desc *drv, struct _usb_h_prvt *pd)
{
	uint8_t i;
	for (i = 0; i < CONF_USB_H_NUM_PIPE_SP; i++) {
		struct usb_h_pipe *p = &pd->pipe_pool[i];
		/* Skip none control or not busy pipes */
		if (p->x.general.state <= USB_H_PIPE_S_SETUP || p->type != 0 /* Control */) {
			continue;
		}
		/* Check timeouts */
		if (p->x.ctrl.pkt_timeout > 0) {
			p->x.ctrl.pkt_timeout--;
		}
		if (p->x.ctrl.req_timeout > 0) {
			p->x.ctrl.req_timeout--;
		}
		if (p->x.ctrl.pkt_timeout == 0 || p->x.ctrl.req_timeout == 0) {
			/* Stop request */
			hri_usbhs_write_HSTPIPIER_reg(drv->hw, i, USBHS_HSTPIPIMR_PFREEZE);
			_usb_h_abort_transfer(p, USB_H_TIMEOUT);
		}
	}
}

/** \internal
 *  \brief Manage periodic endpoints starts
 *  \param drv Pointer to driver instance
 *  \param pd Pointer to private data instance
 */
static inline void _usb_h_periodic_start(struct usb_h_desc *drv, struct _usb_h_prvt *pd)
{
	uint8_t i;
	for (i = 0; i < pd->pipe_pool_size; i++) {
		struct usb_h_pipe *p = &pd->pipe_pool[i];
		/* Skip none periodic or not busy pipes */
		if (!p->periodic_start) {
			continue;
		}
		if (!hri_usbhs_read_HSTPIPISR_NBUSYBK_bf(drv->hw, i)) {
			continue;
		}
		_usb_h_rm_sof_user(drv); /* SOF User: periodic start */
		p->periodic_start = 0;
		hri_usbhs_write_HSTPIPIDR_reg(drv->hw, i, USBHS_HSTPIPIMR_PFREEZE);
	}
}

/** \internal
 *  \brief Handle SOF/MicroSOF interrupt
 *  \param drv Pointer to driver instance
 */
static inline void _usb_h_handle_sof(struct usb_h_desc *drv)
{
	struct _usb_h_prvt *pd = (struct _usb_h_prvt *)drv->prvt;
	hri_usbhs_write_HSTICR_reg(drv->hw, USBHS_HSTICR_HSOFIC);
	/* Micro SOF */
	if (hri_usbhs_get_HSTFNUM_reg(drv->hw, USBHS_HSTFNUM_MFNUM_Msk)) {
/* Micro SOF not handled or notified, since */
/* 1. Invokes callbacks too frequency increases CPU time for interrupt
 *    handling.
 * 2. It changes the frequency of invoking the same callback on FS or HS
 *    mode.
 */
#if 0
		if (pd->resume_start <= 0 || pd->suspend_start <= 0) {
			/* No resume and suspend on going, notify Micro SOF */
			drv->sof_cb(drv);
		}
#endif
		return;
	}
	/* Manage periodic starts */
	_usb_h_periodic_start(drv, pd);

	/* Manage a delay to enter in suspend */
	if (pd->suspend_start > 0) {
		_usb_h_delayed_suspend(drv, pd);
		return; /* Abort SOF events */
	}
	/* Manage a delay to exit of suspend */
	if (pd->resume_start > 0) {
		_usb_h_delayed_resume(drv, pd);
		return; /* Abort SOF events */
	}
	/* Manage the timeout on control endpoints */
	_usb_h_ctrl_timeout(drv, pd);

	/* Notify SOF */
	drv->sof_cb(drv);

	/* Disable SOF interrupt if no SOF users */
	if (pd->n_sof_user <= 0) {
		hri_usbhs_clear_HSTIMR_HSOFIE_bit(drv->hw);
	}
}

/** \internal
 *  \brief Return pipe error and ACK errors
 *  \param drv Pointer to driver instance
 *  \param[in] pi  Pipe index
 */
static int32_t _usb_h_pipe_get_error(struct usb_h_desc *drv, uint8_t pi)
{
	uint32_t error = hri_usbhs_read_HSTPIPERR_reg(drv->hw, pi);
	hri_usbhs_write_HSTPIPERR_reg(drv->hw, pi, 0);
	switch (error
	        & (USBHS_HSTPIPERR_DATATGL | USBHS_HSTPIPERR_TIMEOUT | USBHS_HSTPIPERR_PID | USBHS_HSTPIPERR_DATAPID)) {
	case USBHS_HSTPIPERR_DATATGL:
		return USB_H_ERR;
	case USBHS_HSTPIPERR_TIMEOUT:
		return USB_H_TIMEOUT;
	case USBHS_HSTPIPERR_DATAPID:
	case USBHS_HSTPIPERR_PID:
	default:
		return USB_H_ERR;
	}
}

/** \internal
 *  \brief Handle pipe interrupts
 *  \param drv Pointer to driver instance
 *  \param[in] isr Interrupt status
 */
static inline void _usb_h_handle_pipe(struct usb_h_desc *drv, uint32_t isr)
{
	struct _usb_h_prvt *pd = (struct _usb_h_prvt *)drv->prvt;
	struct usb_h_pipe * p;
	int8_t              pi = 23 - __CLZ(isr & USBHS_HSTISR_PEP__Msk);
	uint32_t            pipisr, pipimr;

	if (pi < 0) {
		return;
	}
	p      = &pd->pipe_pool[pi];
	pipisr = hri_usbhs_read_HSTPIPISR_reg(drv->hw, pi);
	pipimr = hri_usbhs_read_HSTPIPIMR_reg(drv->hw, pi);
	/* STALL */
	if (pipisr & USBHS_HSTPIPISR_RXSTALLDI) {
		hri_usbhs_write_HSTPIPICR_reg(drv->hw, pi, USBHS_HSTPIPISR_RXSTALLDI);
		hri_usbhs_write_HSTPIPIER_reg(drv->hw, pi, USBHS_HSTPIPIER_RSTDTS);
		_usb_h_abort_transfer(p, USB_H_STALL);
		return;
	}

	/* Error */
	if (pipisr & USBHS_HSTPIPISR_PERRI) {
		/* Get and ACK error */
		p->x.general.status = _usb_h_pipe_get_error(drv, pi);
		_usb_h_abort_transfer(p, USB_H_ERR);
		return;
	}

	if (!p->dma) {
		/* SHORTPACKETI: Short received */
		if (pipisr & pipimr & USBHS_HSTPIPISR_SHORTPACKETI) {
			hri_usbhs_write_HSTPIPICR_reg(drv->hw, pi, USBHS_HSTPIPISR_SHORTPACKETI);
			_usb_h_in(p);
			return;
		}
		/* RXIN: Full packet received */
		if (pipisr & pipimr & USBHS_HSTPIPISR_RXINI) {
			hri_usbhs_write_HSTPIPICR_reg(drv->hw, pi, USBHS_HSTPIPISR_RXINI | USBHS_HSTPIPISR_SHORTPACKETI);
			/* In case of low USB speed and with a high CPU frequency,
			 * a ACK from host can be always running on USB line
			 * then wait end of ACK on IN pipe.
			 */
			if (!hri_usbhs_read_HSTPIPINRQ_reg(drv->hw, pi)) {
				while (!hri_usbhs_get_HSTPIPIMR_reg(drv->hw, pi, USBHS_HSTPIPIMR_PFREEZE)) {
				}
			}
			_usb_h_in(p);
			return;
		}
		/* TXOUT: packet sent */
		if (pipisr & pipimr & USBHS_HSTPIPISR_TXOUTI) {
			_usb_h_out(p);
			return;
		}
		/* OUT: all banks sent */
		if (pipimr & USBHS_HSTPIPIMR_NBUSYBKE && (pipisr & USBHS_HSTPIPISR_NBUSYBK_Msk) == 0) {
			hri_usbhs_write_HSTPIPIER_reg(drv->hw, pi, USBHS_HSTPIPIER_PFREEZES);
			hri_usbhs_write_HSTPIPIDR_reg(drv->hw, pi, USBHS_HSTPIPIDR_NBUSYBKEC);
			_usb_h_end_transfer(p, USB_H_OK);
			return;
		}
		/* SETUP: packet sent */
		if (pipisr & pipimr & USBHS_HSTPIPISR_TXSTPI) {
			hri_usbhs_write_HSTPIPICR_reg(drv->hw, pi, USBHS_HSTPIPISR_TXSTPI);
			hri_usbhs_write_HSTPIPIDR_reg(drv->hw, pi, USBHS_HSTPIPISR_TXSTPI);
			/* Start DATA phase */
			if (p->x.ctrl.setup[0] & 0x80) { /* IN */
				p->x.ctrl.state = USB_H_PIPE_S_DATI;
				/* Start IN requests */
				_usb_h_ctrl_in_req(p);
			} else {                                            /* OUT */
				if (p->x.ctrl.setup[6] || p->x.ctrl.setup[7]) { /* wLength */
					p->x.ctrl.state = USB_H_PIPE_S_DATO;
					/* Start OUT */
					hri_usbhs_write_HSTPIPCFG_PTOKEN_bf(drv->hw, pi, USBHS_HSTPIPCFG_PTOKEN_OUT_Val);
					hri_usbhs_write_HSTPIPIER_reg(drv->hw, pi, USBHS_HSTPIPIMR_TXOUTE);
					_usb_h_out(p);
				} else { /* No DATA phase */
					p->x.ctrl.state = USB_H_PIPE_S_STATI;
					/* Start IN ZLP request */
					_usb_h_ctrl_in_req(p);
				}
			}
			return;
		}
	} /* !p->dma */

#if CONF_USB_H_DMA_SP
	/* BG transfer for OUT */
	if (pipimr & USBHS_HSTPIPIMR_NBUSYBKE && (pipisr & USBHS_HSTPIPISR_NBUSYBK_Msk) == 0) {
		hri_usbhs_write_HSTPIPIER_reg(drv->hw, pi, USBHS_HSTPIPIER_PFREEZES);
		hri_usbhs_write_HSTPIPIDR_reg(drv->hw, pi, USBHS_HSTPIPIDR_NBUSYBKEC);
		/* For ISO, no ACK, finished when DMA done */
		if (p->type != 1 /* ISO */) {
			_usb_h_end_transfer(p, USB_H_OK);
		}
		return;
	}
	/* TXOUT is used to send ZLP */
	if (pipisr & pipimr & USBHS_HSTPIPISR_TXOUTI) {
		hri_usbhs_write_HSTPIPIDR_reg(drv->hw, pi, USBHS_HSTPIPISR_TXOUTI);
		/* One bank is free then send a ZLP */
		hri_usbhs_write_HSTPIPICR_reg(drv->hw, pi, USBHS_HSTPIPISR_TXOUTI);
		hri_usbhs_write_HSTPIPIDR_reg(drv->hw, pi, USBHS_HSTPIPIDR_FIFOCONC | USBHS_HSTPIPIDR_PFREEZEC);
		return;
	}
#endif

	ASSERT(false); /* Error system */
}

#if CONF_USB_H_DMA_SP
/** \internal
 *  \brief Handle DMA interrupts
 *  \param drv Pointer to driver instance
 *  \param[in] isr Interrupt status
 */
static inline void _usb_h_handle_dma(struct usb_h_desc *drv, uint32_t isr)
{
	struct _usb_h_prvt *pd = (struct _usb_h_prvt *)drv->prvt;
	struct usb_h_pipe * p;
	uint32_t            imr = hri_usbhs_read_HSTIMR_reg(drv->hw);
	int8_t              pi  = 7 - __CLZ(isr & imr & USBHS_HSTISR_DMA__Msk);

	uint32_t dmastat;
	uint8_t *buf;
	uint32_t size, count;
	uint32_t n_remain;
	if (pi < 0) {
		return;
	}

	dmastat = hri_usbhs_read_HSTDMASTATUS_reg(drv->hw, pi - 1);

	if (dmastat & USBHS_HSTDMASTATUS_CHANN_ENB) {
		return; /* Ignore EOT_STA interrupt */
	}
	p = &pd->pipe_pool[pi];

#if _HPL_USB_H_HBW_SP
	if (p->high_bw_out) {
		/* Streaming out, no ACK, assume everything sent */
		_usb_h_ll_dma_out(p, _usb_h_ll_get(pi, p->bank));
		return;
	}
#endif

	/* Save number of data no transfered */
	n_remain = (dmastat & USBHS_HSTDMASTATUS_BUFF_COUNT_Msk) >> USBHS_HSTDMASTATUS_BUFF_COUNT_Pos;
	if (n_remain) {
		_usb_h_load_x_param(p, &buf, &size, &count);
		(void)buf;
		(void)size;
		/* Transfer no complete (short packet or ZLP) then:
		 * Update number of transfered data
		 */
		count -= n_remain;
		_usb_h_save_x_param(p, count);
	}
	/* Pipe IN: freeze status may delayed */
	if (p->ep & 0x80) {
		if (!hri_usbhs_get_HSTPIPIMR_reg(drv->hw, pi, USBHS_HSTPIPIMR_PFREEZE)) {
			/* Pipe is not frozen in case of :
			 * - incomplete transfer when the request number INRQ is not
			 *   complete.
			 * - low USB speed and with a high CPU frequency,
			 *   a ACK from host can be always running on USB line.
			 */
			if (n_remain) {
				/* Freeze pipe in case of incomplete transfer */
				hri_usbhs_write_HSTPIPIER_reg(drv->hw, pi, USBHS_HSTPIPIMR_PFREEZE);
			} else {
				/* Wait freeze in case of ACK on going */
				while (!hri_usbhs_get_HSTPIPIMR_reg(drv->hw, pi, USBHS_HSTPIPIMR_PFREEZE)) {
				}
			}
		}
	}
	_usb_h_dma(p, (bool)n_remain);
}
#endif

#ifndef USBHS_SR_VBUSRQ
#define USBHS_SR_VBUSRQ (1u << 9)
#endif
/**
 * \brief Enable/disable VBus
 */
static inline void _usbhs_enable_vbus(void *hw, bool en)
{
	if (en) {
		hri_usbhs_write_SFR_reg(hw, USBHS_SR_VBUSRQ);
	} else {
		hri_usbhs_write_SCR_reg(hw, USBHS_SR_VBUSRQ);
	}
}

/** \internal
 *  \brief Handle root hub changes
 *  \param drv Pointer to driver instance
 *  \param[in] isr Interrupt status
 */
static inline void _usb_h_handle_rhc(struct usb_h_desc *drv, uint32_t isr)
{
	struct _usb_h_prvt *pd  = (struct _usb_h_prvt *)drv->prvt;
	uint32_t            imr = hri_usbhs_read_HSTIMR_reg(drv->hw);

	/* Bus reset sent */
	if (isr & USBHS_HSTISR_RSTI) {
		hri_usbhs_write_HSTICR_reg(drv->hw, USBHS_HSTISR_RSTI);
		_usb_h_reset_pipes(drv, true);
		drv->rh_cb(drv, 1, 4); /* PORT_RESET: reset complete */
		return;
	} else if (isr & imr & USBHS_HSTISR_DDISCI) {
		/* Disconnect */
		hri_usbhs_write_HSTICR_reg(drv->hw, USBHS_HSTISR_DDISCI | USBHS_HSTISR_DCONNI);
		/* Disable disconnect interrupt.
		 * Disable wakeup/resumes interrupts,
		 * in case of disconnection during suspend mode
		 */
		hri_usbhs_clear_HSTIMR_reg(
		    drv->hw, USBHS_HSTISR_DDISCI | USBHS_HSTISR_HWUPI | USBHS_HSTISR_RSMEDI | USBHS_HSTISR_RXRSMI);
		/* Stop reset signal, in case of disconnection during reset */
		hri_usbhs_clear_HSTCTRL_RESET_bit(drv->hw);
		/* Enable connection and wakeup interrupts */
		hri_usbhs_write_HSTICR_reg(
		    drv->hw, USBHS_HSTISR_DCONNI | USBHS_HSTISR_HWUPI | USBHS_HSTISR_RSMEDI | USBHS_HSTISR_RXRSMI);
		hri_usbhs_set_HSTIMR_reg(drv->hw,
		                         USBHS_HSTISR_DCONNI | USBHS_HSTISR_HWUPI | USBHS_HSTISR_RSMEDI | USBHS_HSTISR_RXRSMI);
		_usb_h_set_suspend(drv, pd, 0);
		_usb_h_set_resume(drv, pd, 0);
		drv->rh_cb(drv, 1, 0); /* PORT_CONNECTION: connect status changed */
		return;
	} else if (isr & imr & USBHS_HSTISR_DCONNI) {
		/* Keep connection status for check */
		hri_usbhs_clear_HSTIMR_reg(drv->hw, USBHS_HSTISR_DCONNI);
		/* Enable disconnection interrupt */
		hri_usbhs_write_HSTICR_reg(drv->hw, USBHS_HSTISR_DDISCI);
		hri_usbhs_set_HSTIMR_reg(drv->hw, USBHS_HSTISR_DDISCI);
		/* Enable SOF */
		hri_usbhs_set_HSTCTRL_SOFE_bit(drv->hw);
		_usb_h_set_suspend(drv, pd, 0);
		_usb_h_set_resume(drv, pd, 0);
		drv->rh_cb(drv, 1, 0); /* PORT_CONNECTION: connect status changed */
		return;
	}
	/* Wake up to power */
	if ((isr & USBHS_HSTISR_HWUPI) && (imr & USBHS_HSTISR_DCONNI)) {
		while (!hri_usbhs_get_SR_reg(drv->hw, USBHS_SR_CLKUSABLE)) {
			/* Check USB clock ready after asynchronous interrupt */
		}
		hri_usbhs_clear_CTRL_FRZCLK_bit(drv->hw);
		/* Here the wakeup interrupt has been used to detect connection
		 * with an asynchronous interrupt.
		 */
		hri_usbhs_clear_HSTIMR_reg(drv->hw, USBHS_HSTISR_HWUPI);
#if CONF_USB_H_VBUS_CTRL
		CONF_USB_H_VBUS_CTRL_FUNC(drv, 1, true);
#endif
		/* Enable VBus if controlled in driver */
		_usbhs_enable_vbus(drv->hw, true);
	}
	/* Resume */
	if (isr & imr & (USBHS_HSTISR_HWUPI | USBHS_HSTISR_RXRSMI | USBHS_HSTISR_RSMEDI)) {
		while (!hri_usbhs_get_SR_reg(drv->hw, USBHS_SR_CLKUSABLE)) {
			/* Check USB clock ready after asynchronous interrupt */
		}
		hri_usbhs_clear_CTRL_FRZCLK_bit(drv->hw);
		hri_usbhs_write_HSTICR_reg(drv->hw, USBHS_HSTISR_HWUPI | USBHS_HSTISR_RXRSMI | USBHS_HSTISR_RSMEDI);
		hri_usbhs_clear_HSTIMR_reg(drv->hw, USBHS_HSTISR_HWUPI | USBHS_HSTISR_RXRSMI | USBHS_HSTISR_RSMEDI);
		hri_usbhs_set_HSTIMR_reg(drv->hw, USBHS_HSTISR_RSTI | USBHS_HSTISR_DDISCI);

		/* Enable SOF */
		hri_usbhs_set_HSTCTRL_SOFE_bit(drv->hw);

		if (!(isr & USBHS_HSTISR_RSMEDI) && !(isr & USBHS_HSTISR_DDISCI)) {
			/* It is a upstream resume
			 * Note: When the CPU exits from a deep sleep mode, the event
			 * USBHS_HSTISR_RXRSMI can be not detected
			 * because the USB clock are not available.
			 *
			 * In High speed mode a downstream resume must be sent
			 * after a upstream to avoid a disconnection.
			 */
			if (hri_usbhs_get_SR_reg(drv->hw, USBHS_SR_SPEED_Msk) == USBHS_SR_SPEED_HIGH_SPEED) {
				hri_usbhs_set_HSTCTRL_RESUME_bit(drv->hw);
			}
		}
		/* Wait 50ms before restarting transfer */
		_usb_h_set_resume(drv, pd, 50);
		return;
	}
#if 0
	ASSERT(false); /* Unexpected interrupt */
#else
	/* Just ignore unexpected interrupts */
	hri_usbhs_write_HSTICR_reg(drv->hw, isr);
#endif
}

/** \internal
 *  \brief USB Host interrupt handler
 */
void USBHS_Handler(void)
{
	uint32_t isr = hri_usbhs_read_HSTISR_reg(_usb_h_dev->hw);

#if CONF_USBHS_SRC == CONF_SRC_USB_480M
	/* Low speed, switch to low power mode to use 48MHz clock */
	if (hri_usbhs_get_SR_reg(_usb_h_dev->hw, USBHS_SR_SPEED_Msk) == USBHS_SR_SPEED(2)) {
		if (!hri_usbhs_get_HSTCTRL_reg(_usb_h_dev->hw, USBHS_HSTCTRL_SPDCONF_Msk)) {
			hri_usbhs_set_HSTCTRL_reg(_usb_h_dev->hw, USBHS_HSTCTRL_SPDCONF_LOW_POWER);
		}
	}
#endif

	/* SOF */
	if (isr & USBHS_HSTISR_HSOFI) {
		_usb_h_handle_sof(_usb_h_dev);
		return;
	}

	/* Pipe interrupts */
	if (isr & USBHS_HSTISR_PEP__Msk) {
		_usb_h_handle_pipe(_usb_h_dev, isr);
		return;
	}
#if CONF_USB_H_DMA_SP
	/* DMA interrupts */
	if (isr & USBHS_HSTISR_DMA__Msk) {
		_usb_h_handle_dma(_usb_h_dev, isr);
		return;
	}
#endif
	/* Reset sent, connect/disconnect, wake up */
	if (isr
	    & (USBHS_HSTISR_RSTI | USBHS_HSTISR_DCONNI | USBHS_HSTISR_DDISCI | USBHS_HSTISR_HWUPI | USBHS_HSTISR_RXRSMI
	       | USBHS_HSTISR_RXRSMI)) {
		/* Root hub change detected */
		_usb_h_handle_rhc(_usb_h_dev, isr);
		return;
	}
}

int32_t _usb_h_init(struct usb_h_desc *drv, void *hw, void *prvt)
{
	struct _usb_h_prvt *pd = (struct _usb_h_prvt *)prvt;

	ASSERT(drv && hw && prvt && pd->pipe_pool && pd->pipe_pool_size);
	if (hri_usbhs_get_CTRL_reg(hw, USBHS_CTRL_USBE)) {
		return USB_H_DENIED;
	}

#if CONF_USB_H_VBUS_CTRL
	CONF_USB_H_VBUS_CTRL_FUNC(drv, 1, false);
#endif

	drv->sof_cb = (usb_h_cb_sof_t)_dummy_func_no_return;
	drv->rh_cb  = (usb_h_cb_roothub_t)_dummy_func_no_return;

	drv->prvt = prvt;
	drv->hw   = hw;

	_usb_h_reset_pipes(drv, false);

	pd->suspend_start   = 0;
	pd->resume_start    = 0;
	pd->pipes_unfreeze  = 0;
	pd->n_ctrl_req_user = 0;
	pd->n_sof_user      = 0;
	pd->dpram_used      = 0;

	_usb_h_dev = drv;

	NVIC_EnableIRQ(USBHS_IRQn);

#define USBHS_CTRL_UIDE 0x01000000 /* UOTGID pin enable */
	hri_usbhs_clear_CTRL_reg(hw, USBHS_CTRL_UIMOD | USBHS_CTRL_UIDE);
	hri_usbhs_set_CTRL_reg(hw, USBHS_CTRL_FRZCLK);

#if CONF_USBHS_SRC == CONF_SRC_USB_48M
	hri_usbhs_write_HSTCTRL_SPDCONF_bf(hw, 1);
#else
	hri_usbhs_clear_HSTCTRL_reg(hw, USBHS_HSTCTRL_SPDCONF_Msk);
#endif

	/* Force re-connection on initialization */
	hri_usbhs_write_HSTIFR_reg(drv->hw, USBHS_HSTIMR_DDISCIE | USBHS_HSTIMR_HWUPIE);

	return USB_H_OK;
}

void _usb_h_deinit(struct usb_h_desc *drv)
{
	_usb_h_disable(drv);
	NVIC_DisableIRQ(USBHS_IRQn);
	NVIC_ClearPendingIRQ(USBHS_IRQn);
}

void _usb_h_enable(struct usb_h_desc *drv)
{
	ASSERT(drv && drv->hw);
	hri_usbhs_set_CTRL_reg(drv->hw, USBHS_CTRL_USBE);
	/* Check USB clock */
	hri_usbhs_clear_CTRL_reg(drv->hw, USBHS_CTRL_FRZCLK);
	while (!hri_usbhs_get_SR_reg(drv->hw, USBHS_SR_CLKUSABLE))
		;

	/* Clear all interrupts that may have been set by a previous host mode */
	hri_usbhs_write_HSTICR_reg(drv->hw,
	                           USBHS_HSTICR_DCONNIC | USBHS_HSTICR_HSOFIC | USBHS_HSTICR_RSMEDIC | USBHS_HSTICR_RSTIC
	                               | USBHS_HSTICR_RXRSMIC);

	/* VBus Hardware Control! */
	hri_usbhs_set_CTRL_reg(drv->hw, 1u << 8);
#if CONF_USB_H_VBUS_CTRL
	CONF_USB_H_VBUS_CTRL_FUNC(drv, 1, true);
#endif

	/* Enable interrupts to detect connection */
	hri_usbhs_set_HSTIMR_reg(drv->hw,
	                         USBHS_HSTIMR_DCONNIE | USBHS_HSTIMR_RSTIE | USBHS_HSTIMR_HSOFIE | USBHS_HSTIMR_HWUPIE);
}

void _usb_h_disable(struct usb_h_desc *drv)
{
	ASSERT(drv && drv->hw);
	hri_usbhs_set_CTRL_reg(drv->hw, USBHS_CTRL_FRZCLK);
	hri_usbhs_clear_CTRL_reg(drv->hw, USBHS_CTRL_USBE);
#if CONF_USB_H_VBUS_CTRL
	CONF_USB_H_VBUS_CTRL_FUNC(drv, 1, false);
#endif
}

int32_t _usb_h_register_callback(struct usb_h_desc *drv, enum usb_h_cb_type type, FUNC_PTR cb)
{
	FUNC_PTR f;
	ASSERT(drv && drv->prvt && drv->hw);

	f = (cb == NULL) ? (FUNC_PTR)_dummy_func_no_return : (FUNC_PTR)cb;
	switch (type) {
	case USB_H_CB_SOF:
		if ((FUNC_PTR)drv->sof_cb != f) {
			hal_atomic_t flags;
			atomic_enter_critical(&flags);
			drv->sof_cb = (usb_h_cb_sof_t)f;
			if (cb) {
				_usb_h_add_sof_user(drv); /* SOF user: callback */
			} else {
				_usb_h_rm_sof_user(drv); /* SOF user: callback */
			}
			atomic_leave_critical(&flags);
		}
		break;
	case USB_H_CB_ROOTHUB_CHANGE:
		drv->rh_cb = (usb_h_cb_roothub_t)f;
		break;
	default:
		return USB_H_ERR_ARG;
	}
	return USB_H_OK;
}

uint16_t _usb_h_get_frame_n(struct usb_h_desc *drv)
{
	ASSERT(drv && drv->hw);
	return hri_usbhs_read_HSTFNUM_FNUM_bf(drv->hw);
}

uint8_t _usb_h_get_microframe_n(struct usb_h_desc *drv)
{
	ASSERT(drv && drv->hw);
	return hri_usbhs_read_HSTFNUM_MFNUM_bf(drv->hw);
}

/** \internal
 *  \brief Check if a physical pipe is control pipe
 *  \param[in] hw   Pointer to hardware instance
 *  \param[in] pipe Physical pipe number (index)
 */
static inline bool _usbhs_is_ctrl_pipe(void *hw, uint8_t pipe)
{
	uint32_t cfg = hri_usbhs_get_HSTPIPCFG_reg(hw, pipe, USBHS_HSTPIPCFG_PTYPE_Msk);
	return (cfg == USBHS_HSTPIPCFG_PTYPE_CTRL);
}

/** \internal
 *  \brief Return true if physical pipe supports DMA
 *  \param[in] phy Physical pipe number (index)
 *  \return \c true if the pipe supports DMA
 */
static inline bool _usbhs_is_pipe_sp_dma(uint8_t phy)
{
	if (CONF_USB_H_DMA_SP) {
		return (phy >= 1 && phy <= 7);
	}
	return false;
}

/** Look up table PSIZE -> size of bytes */
static const uint16_t psize_2_size[] = {8, 16, 32, 64, 128, 256, 512, 1024};

/** \internal
 *  \brief Convert bank size of bytes to PIPCFG.PSIZE
 *  \param[in] size Size of bytes
 */
static int8_t _usbhs_get_psize(uint16_t size)
{
	uint8_t i;
	for (i = 0; i < sizeof(psize_2_size) / sizeof(uint16_t); i++) {
		/* Size should be exactly PSIZE values */
		if (size <= psize_2_size[i]) {
			return i;
		}
	}
	return 7;
}

/** \brief Return max number of banks
 *  \param[in] phy Physical pipe number (index)
 */
static uint8_t _usbhs_get_pbk_max(uint8_t phy)
{
	return ((phy == 0) ? 1 : ((phy > 2) ? 2 : 3));
}

static void _usbhs_suspend(struct usb_h_desc *drv)
{
	struct _usb_h_prvt *pd = (struct _usb_h_prvt *)drv->prvt;
	uint8_t             i;
	if (pd->n_ctrl_req_user) {
		/* Delay suspend after setup requests */
		pd->suspend_start = -1;
		return;
	}
	/* Save pipe freeze states and freeze pipes */
	pd->pipes_unfreeze = 0;
	for (i = 0; i < CONF_USB_H_NUM_PIPE_SP; i++) {
		/* Skip frozen pipes */
		if (hri_usbhs_get_HSTPIPIMR_reg(drv->hw, i, USBHS_HSTPIPIMR_PFREEZE)) {
			continue;
		}
		/* Log unfrozen pipes */
		pd->pipes_unfreeze |= 1 << i;
		/* Freeze it to suspend */
		hri_usbhs_write_HSTPIPIER_reg(drv->hw, i, USBHS_HSTPIPIMR_PFREEZE);
	}
	/* Wait 3 SOFs before entering in suspend state */
	_usb_h_set_suspend(drv, pd, 3); /* SOF user: delayed suspend */
}

/**
 *  \brief Wakeup device connected on a root hub port
 *  \param drv Pointer to driver instance
 */
static inline void _usbhs_wakeup(struct usb_h_desc *drv)
{
	struct _usb_h_prvt *pd = (struct _usb_h_prvt *)drv->prvt;
	if (hri_usbhs_get_HSTCTRL_reg(drv->hw, USBHS_HSTCTRL_SOFE)) {
		/* Currently in IDLE mode (!=Suspend) */
		if (pd->suspend_start != 0) {
			/* Suspend mode on going
			 * Stop it and start resume event */
			_usb_h_set_suspend(drv, pd, 0);
			_usb_h_set_resume(drv, pd, 1); /* SOF user: delayed resume */
		}
		return;
	}
	while (!hri_usbhs_get_SR_reg(drv->hw, USBHS_SR_CLKUSABLE)) {
		/* Wait USB clock */
	}
	/* Unfreeze clock */
	hri_usbhs_clear_CTRL_FRZCLK_bit(drv->hw);

	_usb_h_add_sof_user(drv); /* SOF user: delayed resume */

	/* Enable SOF */
	hri_usbhs_set_HSTCTRL_SOFE_bit(drv->hw);
	/* Do resume to downstream */
	hri_usbhs_set_HSTCTRL_RESUME_bit(drv->hw);
	/* Force a wakeup interrupt to do delayed resume */
	hri_usbhs_write_HSTIFR_reg(drv->hw, USBHS_HSTIFR_HWUPIS);
}

void _usb_h_rh_reset(struct usb_h_desc *drv, uint8_t port)
{
	Usbhs *hw = (Usbhs *)drv->hw;
	(void)port;
#if CONF_USBHS_SRC == CONF_SRC_USB_480M
	hri_usbhs_clear_HSTCTRL_reg(hw, USBHS_HSTCTRL_SPDCONF_Msk);
#endif
	hri_usbhs_set_HSTCTRL_RESET_bit(hw);
}

void _usb_h_rh_suspend(struct usb_h_desc *drv, uint8_t port)
{
	(void)port;
	_usbhs_suspend(drv);
}

void _usb_h_rh_resume(struct usb_h_desc *drv, uint8_t port)
{
	(void)port;
	_usbhs_wakeup(drv);
}

bool _usb_h_rh_check_status(struct usb_h_desc *drv, uint8_t port, uint8_t ftr)
{
	struct _usb_h_prvt *pd = (struct _usb_h_prvt *)drv->prvt;
	Usbhs *             hw = (Usbhs *)drv->hw;

	uint32_t hstctrl = hri_usbhs_read_HSTCTRL_reg(hw);
	uint32_t hstisr  = hri_usbhs_read_HSTISR_reg(hw);
	uint32_t sr      = hri_usbhs_read_SR_reg(hw);

	switch (ftr) {
	case 0: /* CONNECTION */
	case 1: /* ENABLE */
	case 8: /* POWER */
		return (hstisr & (USBHS_HSTISR_DCONNI | USBHS_HSTISR_DDISCI)) == USBHS_HSTISR_DCONNI;
	case 2: /* SUSPEND */
		if (pd->suspend_start) {
			return true;
		} else {
			return !(hstctrl & USBHS_HSTCTRL_SOFE);
		}
	case 4: /* RESET */
		return (hstctrl & USBHS_HSTCTRL_RESET);
	case 9: /* LS */
		return (sr & USBHS_SR_SPEED_Msk) == USBHS_SR_SPEED_LOW_SPEED;
	case 10: /* HS */
		return (sr & USBHS_SR_SPEED_Msk) == USBHS_SR_SPEED_HIGH_SPEED;
	default:
		return false;
	}
}

/** \internal
 *  \brief Return pipe index
 *  \param[in] pipe Pointer to pipe instance
 */
static int8_t _usb_h_pipe_i(struct usb_h_pipe *pipe)
{
	struct _usb_h_prvt *pd = (struct _usb_h_prvt *)pipe->hcd->prvt;
	ASSERT(pipe >= pd->pipe_pool && pipe <= &pd->pipe_pool[pd->pipe_pool_size]);
	return (pipe - pd->pipe_pool);
}

/** \internal
 *  \brief Return allocated pipe index
 *  \param     drv Pointer to driver instance
 *  \param[in] dev Device address
 *  \param[in] ep  Endpoint address
 *  \return pipe index or -1 if error
 */
static int8_t _usb_h_find_pipe(struct usb_h_desc *drv, uint8_t dev, uint8_t ep)
{
	uint8_t             i;
	struct _usb_h_prvt *pd = (struct _usb_h_prvt *)drv->prvt;
	for (i = 0; i < pd->pipe_pool_size; i++) {
		if (pd->pipe_pool[i].x.general.state == USB_H_PIPE_S_FREE) {
			/* Skip free pipes */
			continue;
		}
		/* Check pipe allocation */
		if (pd->pipe_pool[i].dev == dev && pd->pipe_pool[i].ep == ep) {
			return i;
		}
	}
	return -1;
}

/**
 *  \brief Return pipe index of a free pipe
 *  \param     drv     Pointer to driver instance
 *  \param[in] start_i Search start index
 *  \param[in] n_bank  Expected number of banks
 */
static int8_t _usb_h_find_free_pipe(struct usb_h_desc *drv, uint8_t start_i, uint8_t n_bank)
{
	uint8_t             i;
	int8_t              second_choise = -1, second_diff, n_bank_diff;
	struct _usb_h_prvt *pd            = (struct _usb_h_prvt *)drv->prvt;
	for (i = start_i; i < pd->pipe_pool_size; i++) {
		if (pd->pipe_pool[i].x.general.state != USB_H_PIPE_S_FREE) {
			continue;
		}
		n_bank_diff = _usbhs_get_pbk_max(i) - n_bank;
		/* First choice is same n_bank */
		if (n_bank_diff == 0) {
			/* It's the right one! */
			return i;
		} else if (n_bank_diff > 0) {
			/* Second choice is more n_bank */
			if (second_choise < 0) {
				/* Found the first second choice */
				second_choise = i;
				second_diff   = n_bank_diff;
			} else if (second_diff > n_bank_diff) {
				/* Use this since less bank difference */
				second_choise = i;
				second_diff   = n_bank_diff;
			}
		}
	}
	return second_choise;
}

/** \internal
 *  \brief Return decoded interval in ms from bInterval
 *  \param[in] speed    USB speed
 *  \param[in] interval bInterval in endpoint descriptor
 */
static uint16_t _usb_h_int_interval(uint8_t speed, uint8_t interval)
{
	if (speed != USB_SPEED_HS) {
		return interval;
	}
	if (interval > 16) {
		interval = 16;
	}
	return (2 << (interval - 1));
}

/** \internal
 *  \brief Set device address of a physical pipe
 *  \param     hw       Pointer to hardware register base
 *  \param[in] pipe     Pipe number(index)
 *  \param[in] dev_addr Device address
 */
static void _usbhs_pipe_set_addr(Usbhs *hw, uint8_t pipe, uint8_t dev_addr)
{
	uint8_t  reg_i = pipe >> 2;
	uint8_t  pos   = (pipe & 0x3) << 3;
	uint32_t reg;
	reg = (&hw->USBHS_HSTADDR1)[reg_i];
	reg &= ~(0x7F << pos);
	reg |= (dev_addr & 0x7F) << pos;
	(&hw->USBHS_HSTADDR1)[reg_i] = reg;
}

#ifndef USBHS_HSTPIPCFG_PINGEN
#define USBHS_HSTPIPCFG_PINGEN (1 << 20)
#endif

static bool _usbhs_pipe_configure(struct usb_h_desc *drv, uint8_t phy, uint8_t dev, uint8_t ep, uint8_t type,
                                  uint16_t size, uint8_t n_bank, uint8_t interval, uint8_t speed)
{
	Usbhs *  hs   = (Usbhs *)drv->hw;
	bool     dir  = ep & 0x80;
	bool     ping = (speed == USB_SPEED_HS) && (type == 0x0 /* CTRL */ || (type == 0x2 /* Bulk */ && !dir));
	uint32_t cfg  = USBHS_HSTPIPCFG_INTFRQ(interval) | (ping ? USBHS_HSTPIPCFG_PINGEN : 0)
	               | USBHS_HSTPIPCFG_PEPNUM(ep & 0xF) | USBHS_HSTPIPCFG_PTYPE(type)
	               | USBHS_HSTPIPCFG_PSIZE(_usbhs_get_psize(size)) | USBHS_HSTPIPCFG_PBK(n_bank - 1)
	               | (type == 0 /* CTRL */ ? 0 : (dir ? USBHS_HSTPIPCFG_PTOKEN_IN : USBHS_HSTPIPCFG_PTOKEN_OUT));
	hri_usbhs_set_HSTPIP_reg(drv->hw, USBHS_HSTPIP_PEN0 << phy);
	hri_usbhs_write_HSTPIPCFG_reg(drv->hw, phy, cfg);
	cfg |= USBHS_HSTPIPCFG_ALLOC;
	hri_usbhs_write_HSTPIPCFG_reg(drv->hw, phy, cfg);
	if (!hri_usbhs_get_HSTPIPISR_reg(drv->hw, phy, USBHS_HSTPIPISR_CFGOK)) {
		hri_usbhs_clear_HSTPIP_reg(drv->hw, USBHS_HSTPIP_PEN0 << phy);
		return false;
	}
	_usbhs_pipe_set_addr(hs, phy, dev);
	return true;
}

/**
 * \brief Update the DPRAM used bytes (allocate/free)
 * \param p     Pointer to pipe instance
 * \param alloc Set to \c true to allocate, \c false to free
 * \return \c true if used bytes updated success
 */
static bool _usb_h_update_dpram_used(struct usb_h_pipe *p, bool alloc)
{
	struct _usb_h_prvt *pd   = (struct _usb_h_prvt *)p->hcd->prvt;
	uint32_t            size = p->bank * p->max_pkt_size;

	if (!alloc) {
		pd->dpram_used -= size;
		return true;
	}
	if (pd->dpram_used + size > USBHS_DPRAM_SIZE) {
		return false;
	}
	pd->dpram_used += size;
	return true;
}

struct usb_h_pipe *_usb_h_pipe_allocate(struct usb_h_desc *drv, uint8_t dev, uint8_t ep, uint16_t max_pkt_size,
                                        uint8_t attr, uint8_t interval, uint8_t speed, bool minimum_rsc)
{
	struct _usb_h_prvt *pd = (struct _usb_h_prvt *)drv->prvt;
	struct usb_h_pipe * p;
	uint8_t             i;

	uint8_t      type       = attr & 0x3;
	uint8_t      n_bank     = ((max_pkt_size >> 11) & 0x3) + 1;
	uint16_t     mps        = max_pkt_size & 0x3FF;
	uint16_t     actual_mps = psize_2_size[_usbhs_get_psize(mps)];
	uint8_t      epn        = ep & 0x0F;
	int8_t       pipe;
	hal_atomic_t flags;
	bool         high_bw_out = false;

	/* Calculate number of banks */
	if (n_bank > 1) {
		if (type != 1 /*ISO*/ && type != 3 /*INT*/) {
			return NULL;               /* Invalid argument */
		} else if ((ep & 0x80) == 0) { /* High BandWidth OUT */
			high_bw_out = true;
		}
	} else {
		if (!minimum_rsc && (type == 2 /* Bulk */ || type == 1 /* ISO */)) {
			/* Allocate more banks for performance */
			n_bank = 2;
		} else {
			/* In this case: n_bank = 1; high_bw = false; */
		}
	}
	/* Check parameter */
	if (epn > CONF_USB_H_MAX_EP_N || n_bank > 3 || mps > 1024) {
		return NULL;
	}
	atomic_enter_critical(&flags);
	/* Re-allocate is not allowed, use usb_h_pipe_set_control_param() */
	if (_usb_h_find_pipe(drv, dev, ep) >= 0) {
		atomic_leave_critical(&flags);
		return NULL;
	} else {
		/* Find free pipe */
		pipe = _usb_h_find_free_pipe(drv, 0, n_bank);
		if (pipe < 0) {
			atomic_leave_critical(&flags);
			return NULL;
		}
	}
	/* Physical pipe 0 is for control only */
	if (pipe == 0) {
		/* Pipe 0 is for default endpoint only */
		if (ep == 0) {
			if (mps > 64) {
				atomic_leave_critical(&flags);
				return NULL;
			}
		} else {
			/* Find free in other pipes */
			pipe = _usb_h_find_free_pipe(drv, 1, n_bank);
		}
	}
	/* Allocate pipe */
	p      = &pd->pipe_pool[pipe];
	p->hcd = drv;

	p->x.general.state = USB_H_PIPE_S_CFG;
	atomic_leave_critical(&flags);

	/* Fill pipe information */
	p->dev          = dev;
	p->ep           = ep;
	p->max_pkt_size = mps;
	p->type         = attr & 0x3;
	p->speed        = speed;
	p->bank         = n_bank;
	p->high_bw_out  = high_bw_out;
	if (speed == USB_SPEED_HS && (type == 0x01 /* ISO */ || type == 0x11 /* INT */)) {
		uint16_t ms = _usb_h_int_interval(speed, interval);
		p->interval = (ms > 0xFF) ? 0xFF : (uint8_t)ms;
	} else {
		/* Set minimal bulk interval to 1ms to avoid taking all bandwidth */
		if (type == 0x02 && ((ep & 0x80) == 0) /* Bulk OUT */ && interval < 1) {
			p->interval = 1;
		} else {
			p->interval = interval;
		}
	}
	p->zlp    = 0;
	p->toggle = 0;
	/* Pipe DMA is supported except:
	 * 1. Control pipe
	 * 2. Max packet size is aligned with hardware settings
	 *    (except HighBandWidth OUT)
	 */
	p->dma = _usbhs_is_pipe_sp_dma(pipe) && (p->type != 0) && ((mps == actual_mps) || !high_bw_out);
	/* Allocate DPRAM */
	if (!_usb_h_update_dpram_used(p, true)) {
		p->x.general.state = USB_H_PIPE_S_FREE;
		return NULL;
	}

	/* Try to allocate physical pipe */
	if (_usbhs_pipe_configure(drv, pipe, p->dev, p->ep, p->type, p->max_pkt_size, p->bank, p->interval, p->speed)) {
		/* Initialize pipe state */
		p->x.general.status = 0;

		/* Abort and re-allocate memory conflict pipes
		 * (pipe allocate cause pipe + 1 slides up, conflicts pipe + 2) */
		for (i = pipe + 2; i < pd->pipe_pool_size; i++) {
			if (!hri_usbhs_get_HSTPIPCFG_reg(drv->hw, i, USBHS_HSTPIPCFG_ALLOC)) {
				continue;
			}
			/* Abort transfer due to memory reset */
			_usb_h_abort_transfer(&pd->pipe_pool[i], USB_H_RESET);
			/* Do re-allocate if new pipe configured OK */
			hri_usbhs_clear_HSTPIPCFG_ALLOC_bit(drv->hw, i);
			hri_usbhs_set_HSTPIPCFG_ALLOC_bit(drv->hw, i);
		}

		/* Enable general error and stall interrupts */
		hri_usbhs_write_HSTPIPICR_reg(drv->hw,
		                              pipe,
		                              USBHS_HSTPIPISR_RXSTALLDI | USBHS_HSTPIPISR_OVERFI | USBHS_HSTPIPISR_PERRI
		                                  | USBHS_HSTPIPISR_UNDERFI);
		hri_usbhs_write_HSTPIPIER_reg(
		    drv->hw, pipe, USBHS_HSTPIPISR_RXSTALLDI | USBHS_HSTPIPISR_OVERFI | USBHS_HSTPIPISR_PERRI);
		hri_usbhs_set_HSTIMR_reg(drv->hw, (USBHS_HSTISR_PEP_0 | (USBHS_HSTISR_DMA_0 >> 1)) << pipe);
		p->x.general.state = USB_H_PIPE_S_IDLE;
		return p;
	}

	/* Not allocated, restore state */
	pd->pipe_pool[pipe].x.general.state = USB_H_PIPE_S_FREE;
	return NULL;
}

int32_t _usb_h_pipe_free(struct usb_h_pipe *pipe)
{
	hal_atomic_t flags;
	uint8_t      pi;

	ASSERT(pipe && pipe->hcd && pipe->hcd->hw && pipe->hcd->prvt);

	/* Not able to free a busy pipe */
	atomic_enter_critical(&flags);
	if (pipe->x.general.state == USB_H_PIPE_S_FREE) {
		/* Already free, no error */
		atomic_leave_critical(&flags);
		return USB_H_OK;
	} else if (pipe->x.general.state != USB_H_PIPE_S_IDLE) {
		atomic_leave_critical(&flags);
		return USB_H_BUSY;
	}
	pipe->x.general.state = USB_H_PIPE_S_CFG;
	atomic_leave_critical(&flags);

	pi = _usb_h_pipe_i(pipe);

	/* Disable interrupts */
	hri_usbhs_clear_HSTIMR_reg(pipe->hcd->hw, (USBHS_HSTIMR_PEP_0 | (USBHS_HSTIMR_DMA_0 >> 1)) << pi);
	/* Free physical pipe */
	hri_usbhs_clear_HSTPIP_reg(pipe->hcd->hw, USBHS_HSTPIP_PEN0 << pi);
	hri_usbhs_clear_HSTPIPCFG_ALLOC_bit(pipe->hcd->hw, pi);
	/* Update DPRAM used */
	_usb_h_update_dpram_used(pipe, false);

#if _HPL_USB_H_HBW_SP
	/* Free allocated DMA for pipe */
	if (pipe->high_bw_out) {
		_usb_h_ll_free(pi);
	}
#endif

	/* Update state */
	pipe->x.general.state = USB_H_PIPE_S_FREE;
	return USB_H_OK;
}

int32_t _usb_h_pipe_set_control_param(struct usb_h_pipe *pipe, uint8_t dev, uint8_t ep, uint16_t max_pkt_size,
                                      uint8_t speed)
{
	hal_atomic_t flags;
	uint8_t      pi, psize;
	uint16_t     size;
	uint32_t     cfg;
	uint8_t      epn = ep & 0xF;

	ASSERT(pipe && pipe->hcd && pipe->hcd->hw && pipe->hcd->prvt);

	/* Check pipe states */
	atomic_enter_critical(&flags);
	if (pipe->x.general.state == USB_H_PIPE_S_FREE) {
		atomic_leave_critical(&flags);
		return USB_H_ERR_NOT_INIT;
	}
	if (pipe->x.general.state != USB_H_PIPE_S_IDLE) {
		atomic_leave_critical(&flags);
		return USB_H_BUSY;
	}
	pipe->x.general.state = USB_H_PIPE_S_CFG;
	atomic_leave_critical(&flags);

	pi    = _usb_h_pipe_i(pipe);
	cfg   = hri_usbhs_read_HSTPIPCFG_reg(pipe->hcd->hw, pi);
	psize = (cfg & USBHS_HSTPIPCFG_PSIZE_Msk) >> USBHS_HSTPIPCFG_PSIZE_Pos;
	size  = psize_2_size[psize];
	/* Check endpoint number and packet size */
	if (epn > CONF_USB_H_MAX_EP_N || size < max_pkt_size) {
		pipe->x.general.state = USB_H_PIPE_S_IDLE;
		return USB_H_ERR_ARG;
	}
	/* Operation not supported by none-control endpoints */
	if ((cfg & USBHS_HSTPIPCFG_PTYPE_Msk) != USBHS_HSTPIPCFG_PTYPE_CTRL) {
		pipe->x.general.state = USB_H_PIPE_S_IDLE;
		return USB_H_ERR_UNSP_OP;
	}
	/* Modify HSTPIPCFG */
	if (speed == USB_SPEED_HS)
		cfg |= USBHS_HSTPIPCFG_PINGEN;
	else
		cfg &= ~USBHS_HSTPIPCFG_PINGEN;
	cfg &= ~(USBHS_HSTPIPCFG_PEPNUM_Msk);
	cfg |= USBHS_HSTPIPCFG_PEPNUM(epn);
	hri_usbhs_write_HSTPIPCFG_reg(pipe->hcd->hw, pi, cfg);
	/* Modify dev, ep, max packet size */
	pipe->dev             = dev;
	pipe->speed           = speed;
	pipe->ep              = ep;
	pipe->x.ctrl.pkt_size = max_pkt_size;
	/* Modify device address */
	_usbhs_pipe_set_addr((Usbhs *)pipe->hcd->hw, pi, dev);
	/* Reset pipe */
	hri_usbhs_set_HSTPIP_reg(pipe->hcd->hw, USBHS_HSTPIP_PRST0 << pi);
	hri_usbhs_clear_HSTPIP_reg(pipe->hcd->hw, USBHS_HSTPIP_PRST0 << pi);
	/* Re enable interrupts */
	hri_usbhs_write_HSTPIPIER_reg(
	    pipe->hcd->hw, pi, USBHS_HSTPIPISR_RXSTALLDI | USBHS_HSTPIPISR_OVERFI | USBHS_HSTPIPISR_PERRI);

	pipe->x.general.state = USB_H_PIPE_S_IDLE;
	return USB_H_OK;
}

int32_t _usb_h_pipe_register_callback(struct usb_h_pipe *pipe, usb_h_pipe_cb_xfer_t cb)
{
	ASSERT(pipe);
	pipe->done = cb;
	return 0;
}

static void _usb_h_out_zlp_ex(struct usb_h_pipe *pipe)
{
	struct usb_h_desc *drv = pipe->hcd;
	uint8_t            pi  = _usb_h_pipe_i(pipe);
	hri_usbhs_write_HSTPIPCFG_PTOKEN_bf(drv->hw, pi, USBHS_HSTPIPCFG_PTOKEN_OUT_Val);
	hri_usbhs_write_HSTPIPICR_reg(drv->hw, pi, USBHS_HSTPIPISR_TXOUTI);

	hri_usbhs_write_HSTPIPIER_reg(drv->hw, pi, USBHS_HSTPIPIMR_TXOUTE);
	hri_usbhs_write_HSTPIPIDR_reg(drv->hw, pi, USBHS_HSTPIPIMR_FIFOCON | USBHS_HSTPIPIMR_PFREEZE);
}

static void _usb_h_ctrl_in_req(struct usb_h_pipe *pipe)
{
	struct usb_h_desc *drv = pipe->hcd;
	uint8_t            pi  = _usb_h_pipe_i(pipe);
	hri_usbhs_write_HSTPIPCFG_PTOKEN_bf(drv->hw, pi, USBHS_HSTPIPCFG_PTOKEN_IN_Val);
	hri_usbhs_write_HSTPIPICR_reg(drv->hw, pi, USBHS_HSTPIPISR_RXINI | USBHS_HSTPIPISR_SHORTPACKETI);
	hri_usbhs_write_HSTPIPIER_reg(drv->hw, pi, USBHS_HSTPIPIER_RXINES);
	hri_usbhs_write_HSTPIPIDR_reg(drv->hw, pi, USBHS_HSTPIPIMR_FIFOCON | USBHS_HSTPIPIMR_PFREEZE);
}

static void _usb_h_setup_ex(struct usb_h_pipe *pipe)
{
	struct usb_h_desc *drv  = pipe->hcd;
	uint8_t            pi   = _usb_h_pipe_i(pipe);
	uint8_t *          src8 = pipe->x.ctrl.setup;
	volatile uint8_t * dst8 = (volatile uint8_t *)&_usbhs_get_pep_fifo_access(pi, 8);
	uint8_t            i;

	hri_usbhs_write_HSTPIPCFG_PTOKEN_bf(drv->hw, pi, USBHS_HSTPIPCFG_PTOKEN_SETUP_Val);
	hri_usbhs_write_HSTPIPICR_reg(drv->hw, pi, USBHS_HSTPIPISR_TXSTPI);
	for (i = 0; i < 8; i++) {
		*dst8++ = *src8++;
	}
	hri_usbhs_write_HSTPIPIER_reg(drv->hw, pi, USBHS_HSTPIPIMR_TXSTPE);
	hri_usbhs_write_HSTPIPIDR_reg(drv->hw, pi, USBHS_HSTPIPIMR_FIFOCON | USBHS_HSTPIPIMR_PFREEZE);
}

static inline void _usb_h_load_x_param(struct usb_h_pipe *pipe, uint8_t **buf, uint32_t *x_size, uint32_t *x_count)
{
#if _HPL_USB_H_HBW_SP
	if (pipe->high_bw_out) {
		*buf     = pipe->x.hbw.data;
		*x_size  = pipe->x.hbw.size;
		*x_count = pipe->x.hbw.count;
		return;
	}
#endif
	if (pipe->type != 0 /* None Control */) {
		*buf     = pipe->x.bii.data;
		*x_size  = pipe->x.bii.size;
		*x_count = pipe->x.bii.count;
	} else {
		*buf     = pipe->x.ctrl.data;
		*x_size  = pipe->x.ctrl.size;
		*x_count = pipe->x.ctrl.count;
	}
}

static inline void _usb_h_save_x_param(struct usb_h_pipe *pipe, uint32_t x_count)
{
#if _HPL_USB_H_HBW_SP
	if (pipe->high_bw_out) {
		pipe->x.hbw.count = x_count;
		return;
	}
#endif
	if (pipe->type != 0 /* None Control */) {
		pipe->x.bii.count = x_count;
	} else {
		pipe->x.ctrl.count = x_count;
	}
}

static void _usb_h_in(struct usb_h_pipe *pipe)
{
	uint8_t            pi  = _usb_h_pipe_i(pipe);
	struct usb_h_desc *drv = (struct usb_h_desc *)pipe->hcd;
	uint8_t *          src, *dst;
	uint32_t           size, count, i;
	uint32_t           n_rx = 0;
	uint32_t           n_remain;
	uint16_t           max_pkt_size = pipe->type == 0 ? pipe->x.ctrl.pkt_size : pipe->max_pkt_size;
	bool               shortpkt = false, full = false;

	if (pipe->x.general.state == USB_H_PIPE_S_STATI) {
		/* Control status : ZLP IN done */
		hri_usbhs_write_HSTPIPIER_reg(drv->hw, pi, USBHS_HSTPIPIER_PFREEZES);
		hri_usbhs_write_HSTPIPIDR_reg(drv->hw, pi, USBHS_HSTPIPIDR_SHORTPACKETIEC | USBHS_HSTPIPIDR_RXINEC);
		hri_usbhs_write_HSTPIPINRQ_reg(drv->hw, pi, 0);
		_usb_h_end_transfer(pipe, USB_H_OK);
		return;
	} else if (pipe->x.general.state != USB_H_PIPE_S_DATI) {
		return;
	}
	/* Read byte count */
	n_rx = hri_usbhs_read_HSTPIPISR_PBYCT_bf(drv->hw, pi);
	if (n_rx < max_pkt_size) {
		shortpkt = true;
	}
	if (n_rx) {
		_usb_h_load_x_param(pipe, &dst, &size, &count);
		n_remain = size - count;
		src      = (uint8_t *)&_usbhs_get_pep_fifo_access(pi, 8);
		dst      = &dst[count];
		if (n_rx >= n_remain) {
			n_rx = n_remain;
			full = true;
		}
		count += n_rx;
		for (i = 0; i < n_rx; i++) {
			*dst++ = *src++;
		}
		_usb_h_save_x_param(pipe, count);
	}
	/* Clear FIFO status */
	hri_usbhs_write_HSTPIPIDR_reg(drv->hw, pi, USBHS_HSTPIPIDR_FIFOCONC);
	/* Reset timeout for control pipes */
	if (pipe->type == 0) {
		pipe->x.ctrl.pkt_timeout = USB_CTRL_DPKT_TIMEOUT;
	}
	/* Finish on error or short packet */
	if (full || shortpkt) {
		hri_usbhs_write_HSTPIPIDR_reg(drv->hw, pi, USBHS_HSTPIPIDR_SHORTPACKETIEC | USBHS_HSTPIPIDR_RXINEC);
		if (pipe->type == 0) { /* Control transfer: DatI -> StatO */
			pipe->x.ctrl.state       = USB_H_PIPE_S_STATO;
			pipe->x.ctrl.pkt_timeout = USB_CTRL_STAT_TIMEOUT;
			_usb_h_out_zlp_ex(pipe);
		} else {
			hri_usbhs_write_HSTPIPIER_reg(drv->hw, pi, USBHS_HSTPIPIER_PFREEZES);
			hri_usbhs_write_HSTPIPINRQ_reg(drv->hw, pi, 0);
			_usb_h_end_transfer(pipe, USB_H_OK);
		}
	} else if (!hri_usbhs_read_HSTPIPINRQ_reg(drv->hw, pi)
	           && hri_usbhs_get_HSTPIPIMR_reg(drv->hw, pi, USBHS_HSTPIPIMR_PFREEZE)) {
		/* Unfreeze if request packet by packet */
		hri_usbhs_write_HSTPIPIDR_reg(drv->hw, pi, USBHS_HSTPIPIMR_PFREEZE);
	} else {
		/* Just wait another packet */
	}
}

static void _usb_h_out(struct usb_h_pipe *pipe)
{
	uint8_t            pi  = _usb_h_pipe_i(pipe);
	struct usb_h_desc *drv = (struct usb_h_desc *)pipe->hcd;
	uint8_t *          src, *dst;
	uint32_t           size, count, i;
	uint32_t           n_tx = 0;
	uint32_t           n_remain;
	uint16_t           max_pkt_size = pipe->type == 0 ? pipe->x.ctrl.pkt_size : pipe->max_pkt_size;
	bool               shortpkt;

	if (pipe->x.general.state == USB_H_PIPE_S_STATO) {
		/* Control status : ZLP OUT done */
		hri_usbhs_write_HSTPIPIER_reg(drv->hw, pi, USBHS_HSTPIPIER_PFREEZES);
		hri_usbhs_write_HSTPIPICR_reg(drv->hw, pi, USBHS_HSTPIPISR_TXOUTI);
		hri_usbhs_write_HSTPIPIDR_reg(drv->hw, pi, USBHS_HSTPIPISR_TXOUTI);
		_usb_h_end_transfer(pipe, USB_H_OK);
		return;
	} else if (pipe->x.general.state != USB_H_PIPE_S_DATO) {
		return;
	}
	/* ACK OUT */
	hri_usbhs_write_HSTPIPICR_reg(drv->hw, pi, USBHS_HSTPIPISR_TXOUTI);
	/* For Control, all data is done, to STATUS stage */
	if (pipe->type == 0 /* Control */ && pipe->x.ctrl.count >= pipe->x.ctrl.size && !pipe->zlp) {
		pipe->x.ctrl.state = USB_H_PIPE_S_STATI;
		/* Start IN ZLP request */
		pipe->x.ctrl.pkt_timeout = USB_CTRL_STAT_TIMEOUT;
		_usb_h_ctrl_in_req(pipe);
		return;
	}

	/* Reset packet timeout for control pipes */
	if (pipe->type == 0 /* Control */) {
		pipe->x.ctrl.pkt_timeout = USB_CTRL_DPKT_TIMEOUT;
	}

	_usb_h_load_x_param(pipe, &src, &size, &count);
	n_remain = size - count;
	shortpkt = n_remain < max_pkt_size;
	n_tx     = shortpkt ? n_remain : max_pkt_size;

	if (n_tx) {
		dst = (uint8_t *)&_usbhs_get_pep_fifo_access(pi, 8);
		src = &src[count];
		count += n_tx;
		for (i = 0; i < n_tx; i++) {
			*dst++ = *src++;
		}
	}
	/* Switch to next bank */
	hri_usbhs_write_HSTPIPIDR_reg(
	    drv->hw, pi, USBHS_HSTPIPIDR_FIFOCONC | (pipe->periodic_start ? 0 : USBHS_HSTPIPIMR_PFREEZE));
	/* ZLP cleared if it's short packet */
	if (shortpkt) {
		pipe->zlp = 0;
	}
	if (n_tx) {
		_usb_h_save_x_param(pipe, count);
	}

	/* All transfer done, including ZLP */
	if (count >= size && !pipe->zlp) {
		/* At least one bank there, wait to freeze pipe */
		if (pipe->type != 0 /* None CTRL */) {
			/* Busy interrupt when all banks are empty */
			hri_usbhs_write_HSTPIPIDR_reg(drv->hw, pi, USBHS_HSTPIPIDR_TXOUTEC);
			hri_usbhs_write_HSTPIPIER_reg(drv->hw, pi, USBHS_HSTPIPIER_NBUSYBKES);
			if (pipe->type == 1 /* ISO */) {
				/* No need to wait ack */
				_usb_h_end_transfer(pipe, USB_H_OK);
			}
		} else {
			/* No busy interrupt for control EPs */
		}
	}
}

#if CONF_USB_H_DMA_SP

static void _usb_h_dma(struct usb_h_pipe *pipe, bool end)
{
	uint8_t            pi   = _usb_h_pipe_i(pipe);
	uint8_t            dmai = pi - 1;
	struct usb_h_desc *drv  = (struct usb_h_desc *)pipe->hcd;
	uint16_t           mps  = psize_2_size[hri_usbhs_read_HSTPIPCFG_PSIZE_bf(drv->hw, pi)];
	hal_atomic_t       flags;
	uint8_t *          buf;
	uint32_t           size, count;
	uint32_t           n_next, n_max;
	uint32_t           dma_ctrl = 0;
	bool               dir      = pipe->ep & 0x80;

	if (pipe->x.general.state != USB_H_PIPE_S_DATO && pipe->x.general.state != USB_H_PIPE_S_DATI) {
		return; /* No DMA is running */
	}
	_usb_h_load_x_param(pipe, &buf, &size, &count);
	if (count < size && !end) {
		/* Need to send or receive other data */
		n_next = size - count;
		n_max  = USBHS_DMA_TRANS_MAX;
		if (dir) {
			/* 256 is the maximum of IN requests via UPINRQ */
			if (256L * mps < n_max) {
				n_max = 256L * mps;
			}
		}
		if (n_max < n_next) {
			/* HW maximum transfer size */
			n_next = n_max;
		}
		/* Set 0 to transfer the maximum */
		dma_ctrl = USBHS_HSTDMACONTROL_BUFF_LENGTH((n_next == USBHS_DMA_TRANS_MAX) ? 0 : n_next);
		if (dir) {
			/* Enable short packet reception */
			dma_ctrl |= USBHS_HSTDMACONTROL_END_TR_IT | USBHS_HSTDMACONTROL_END_TR_EN;
		} else if ((n_next & (mps - 1)) != 0) {
			/* Enable short packet option
			 * else the DMA transfer is accepted
			 * and interrupt DMA valid but nothing is sent.
			 */
			dma_ctrl |= USBHS_HSTDMACONTROL_END_B_EN;
			/* No need to request another ZLP */
			pipe->zlp = 0;
		}
		/* Start USB DMA to fill or read FIFO of the selected endpoint */
		hri_usbhs_write_HSTDMAADDRESS_reg(drv->hw, dmai, (uint32_t)&buf[count]);
		dma_ctrl |= USBHS_HSTDMACONTROL_END_BUFFIT | USBHS_HSTDMACONTROL_CHANN_ENB;
		/* Disable IRQs to have a short sequence
		 * between read of EOT_STA and DMA enable
		 */
		atomic_enter_critical(&flags);
		if (!hri_usbhs_get_HSTDMASTATUS_reg(drv->hw, dmai, USBHS_HSTDMASTATUS_END_TR_ST)) {
			if (dir) {
				hri_usbhs_write_HSTPIPINRQ_reg(drv->hw, pi, (n_next + mps - 1) / mps - 1);
			}
			if (pipe->periodic_start) {
				/* Still packets in FIFO, just start */
				if (hri_usbhs_get_HSTPIPIMR_reg(drv->hw, pi, USBHS_HSTPIPIMR_NBUSYBKE)) {
					pipe->periodic_start = 0;
					hri_usbhs_write_HSTPIPIDR_reg(drv->hw, pi, USBHS_HSTPIPIMR_NBUSYBKE);
				} else {
					/* Wait SOF to start */
					_usb_h_add_sof_user(drv); /* SOF User: periodic start */
				}
			} else {
				/* Just start */
				hri_usbhs_write_HSTPIPIDR_reg(drv->hw, pi, USBHS_HSTPIPIMR_NBUSYBKE | USBHS_HSTPIPIMR_PFREEZE);
			}
			/* Start DMA */
			hri_usbhs_write_HSTDMACONTROL_reg(drv->hw, dmai, dma_ctrl);
			count += n_next;
			_usb_h_save_x_param(pipe, count);
			atomic_leave_critical(&flags);
			return;
		}
		atomic_leave_critical(&flags);
	}

	/* OUT pipe */
	if ((pipe->ep & 0x80) == 0) {
		if (pipe->zlp) {
			/* Need to send a ZLP (No possible with USB DMA)
			 * enable interrupt to wait a free bank to sent ZLP
			 */
			hri_usbhs_write_HSTPIPICR_reg(drv->hw, pi, USBHS_HSTPIPISR_TXOUTI);
			if (hri_usbhs_get_HSTPIPISR_reg(drv->hw, pi, USBHS_HSTPIPISR_RWALL)) {
				/* Force interrupt in case of pipe already free */
				hri_usbhs_write_HSTPIPIFR_reg(drv->hw, pi, USBHS_HSTPIPISR_TXOUTI);
			}
			hri_usbhs_write_HSTPIPIER_reg(drv->hw, pi, USBHS_HSTPIPIMR_TXOUTE);
		} else {
			/* Wait that all banks are free to freeze clock of OUT endpoint
			 * and call callback except ISO. */
			/* For ISO out, start another DMA transfer since no ACK needed */
			hri_usbhs_write_HSTPIPIER_reg(drv->hw, pi, USBHS_HSTPIPIER_NBUSYBKES);
			if (pipe->type != 1) {
				/* Callback on BE transfer done */
				return;
			}
		}
	}
	/* Finish transfer */
	_usb_h_end_transfer(pipe, USB_H_OK);
}
#endif

int32_t _usb_h_control_xfer(struct usb_h_pipe *pipe, uint8_t *setup, uint8_t *data, uint16_t length, int16_t timeout)
{
	hal_atomic_t flags;
	int8_t       pi = _usb_h_pipe_i(pipe);

	ASSERT(pipe && pipe->hcd && pipe->hcd->hw && pipe->hcd->prvt);
	ASSERT(pi >= 0 && pi < CONF_USB_H_NUM_PIPE_SP);

	/* Check parameters */
	if (pipe->type != 0) { /* Not control */
		return USB_H_ERR_UNSP_OP;
	}
	/* Check state */
	atomic_enter_critical(&flags);
	if (pipe->x.general.state == USB_H_PIPE_S_FREE) {
		atomic_leave_critical(&flags);
		return USB_H_ERR_NOT_INIT;
	} else if (pipe->x.general.state != USB_H_PIPE_S_IDLE) {
		atomic_leave_critical(&flags);
		return USB_H_BUSY;
	}
	pipe->x.general.state = USB_H_PIPE_S_SETUP;
	_usb_h_add_sof_user(pipe->hcd); /* SOF user: control timeout */
	_usb_h_add_req_user(pipe->hcd);
	atomic_leave_critical(&flags);

	pipe->x.ctrl.setup       = setup;
	pipe->x.ctrl.data        = data;
	pipe->x.ctrl.size        = length;
	pipe->x.ctrl.count       = 0;
	pipe->x.ctrl.req_timeout = timeout;
	pipe->x.ctrl.pkt_timeout = length ? USB_CTRL_DPKT_TIMEOUT : (-1);
	pipe->x.ctrl.status      = 0;

	/* Start with setup packet */
	_usb_h_setup_ex(pipe);
	return USB_H_OK;
}

int32_t _usb_h_bulk_int_iso_xfer(struct usb_h_pipe *pipe, uint8_t *data, uint32_t length, bool auto_zlp)
{
	hal_atomic_t flags;
	int8_t       pi       = _usb_h_pipe_i(pipe);
	bool         dir      = pipe->ep & 0x80;
	bool         iso_pipe = pipe->type == 1;
#if CONF_USB_H_DMA_SP
	bool int_pipe = pipe->type == 3;
	bool use_dma  = pipe->dma && _usbhs_buf_dma_sp(data);
#endif

	ASSERT(pipe && pipe->hcd && pipe->hcd->hw && pipe->hcd->prvt);
	ASSERT(pi >= 0 && pi < CONF_USB_H_NUM_PIPE_SP);

	/* Check parameters */
	if (pipe->type == 0) { /* Control */
		return USB_H_ERR_UNSP_OP;
	}
	/* Check state */
	atomic_enter_critical(&flags);
	if (pipe->x.general.state == USB_H_PIPE_S_FREE) {
		atomic_leave_critical(&flags);
		return USB_H_ERR_NOT_INIT;
	} else if (pipe->x.general.state != USB_H_PIPE_S_IDLE) {
		atomic_leave_critical(&flags);
		return USB_H_BUSY;
	}
	pipe->x.general.state = dir ? USB_H_PIPE_S_DATI : USB_H_PIPE_S_DATO;
	atomic_leave_critical(&flags);

	pipe->x.bii.data   = data;
	pipe->x.bii.size   = length;
	pipe->x.bii.count  = 0;
	pipe->x.bii.status = 0;
	pipe->zlp          = auto_zlp;

#if CONF_USB_H_DMA_SP
	if (use_dma) {
		pipe->periodic_start = (!dir) && (iso_pipe || int_pipe);

		/* Start DMA */
		hri_usbhs_set_HSTPIPCFG_AUTOSW_bit(pipe->hcd->hw, pi);
		_usb_h_dma(pipe, false);
		return USB_H_OK;
	}
#endif

	/* Clear auto switch mode */
	hri_usbhs_clear_HSTPIPCFG_AUTOSW_bit(pipe->hcd->hw, pi);
	if (dir) { /* IN */
		pipe->periodic_start = false;
		hri_usbhs_write_HSTPIPIDR_reg(pipe->hcd->hw, pi, USBHS_HSTPIPIDR_PFREEZEC);
		hri_usbhs_write_HSTPIPINRQ_reg(pipe->hcd->hw, pi, USBHS_HSTPIPINRQ_INMODE);
		hri_usbhs_write_HSTPIPIER_reg(pipe->hcd->hw, pi, USBHS_HSTPIPIER_RXINES);
	} else {
		pipe->periodic_start = iso_pipe;
		hri_usbhs_write_HSTPIPIER_reg(pipe->hcd->hw, pi, USBHS_HSTPIPIER_TXOUTES);
		hri_usbhs_write_HSTPIPIDR_reg(pipe->hcd->hw, pi, USBHS_HSTPIPIDR_NBUSYBKEC);
	}
	return USB_H_OK;
}

#if _HPL_USB_H_HBW_SP

/** \internal
 *  \brief Check if link list available (exists or have enough free)
 *  \param[in] pi   Pipe index
 *  \param[in] bank Number of banks
 */
static bool _usb_h_ll_available(int8_t pi, uint8_t bank)
{
	uint8_t i, n;
	for (i = 0, n = 0; i < _HPL_USB_H_N_DMAD; i++) {
		if (_usbhs_dma_user[i] == pi) { /* Already exist */
			return true;
		} else if (_usbhs_dma_user[i] < 0) { /* Check free */
			if (++n >= bank) {
				return true;
			}
		}
	}
	/* Not exist, free < n_bank */
	return false;
}

/** \internal
 *  \brief Free DMA descriptors for the pipe
 *  \param[in] pi   Pipe index, set to -1 to free all
 */
static void _usb_h_ll_free(int8_t pi)
{
	uint8_t i;
	for (i = 0; i < _HPL_USB_H_N_DMAD; i++) {
		if (pi == -1) {
			_usbhs_dma_user[i] = -1;
		} else if (_usbhs_dma_user[i] == pi) {
			_usbhs_dma_user[i] = -1;
		}
	}
}

/** \internal
 *  \brief Allocate DMA descriptors for the pipe, or get allocated one
 *  \param[in] pi   Pipe index
 *  \param[in] bank Number of banks
 *  \return Pointer to DMA descriptor link list
 */
static inline UsbhsHstdma *_usb_h_ll_get(int8_t pi, uint8_t bank)
{
	uint8_t      i, n = 0;
	UsbhsHstdma *d = NULL, *l = NULL;
	for (i = 0; i < _HPL_USB_H_N_DMAD; i++) {
		if (_usbhs_dma_user[i] == pi) {
			/* Already allocated, just use it (first found is list header) */
			l = &_usbhs_dma_desc[i];
			break;
		}
		if (_usbhs_dma_user[i] < 0) {
			_usbhs_dma_user[i] = pi;
			if (d == NULL) {
				/* List head */
				d = &_usbhs_dma_desc[i];
				l = d;
			} else {
				/* Link to the end */
				d->USBHS_HSTDMANXTDSC = (uint32_t)&_usbhs_dma_desc[i];
				d                     = &_usbhs_dma_desc[i];
			}
			if (++n >= bank) {
				/* Loop descriptors */
				d->USBHS_HSTDMANXTDSC = (uint32_t)l;
				break;
			}
		}
	}
	return l;
}

/** \internal
 *  \brief Handles linked list DMA OUT
 *  \param pipe Pointer to pipe instance
 *  \param ll   Pointer to linked list used
 */
static void _usb_h_ll_dma_out(struct usb_h_pipe *pipe, UsbhsHstdma *ll)
{
	struct usb_h_high_bw_xfer *x   = &pipe->x.hbw;
	struct usb_h_desc *        drv = pipe->hcd;
	uint8_t                    pi  = _usb_h_pipe_i(pipe);
	UsbhsHstdma *              l   = ll;
	uint8_t                    i;
	uint32_t                   n_next = x->size - x->count;
	hal_atomic_t               flags;

	if (x->state != USB_H_PIPE_S_DATO) {
		return;
	}
	if (n_next == 0) {
		/* DMA is done, enable busy bank interrupt */
		hri_usbhs_write_HSTPIPIER_reg(drv->hw, pi, USBHS_HSTPIPIMR_NBUSYBKE);
		/* No need ACK, invoke callback when transactions in progress */
		_usb_h_end_transfer(pipe, USB_H_OK);
		return;
	}
	/* Fill into linked list */
	for (i = 0; i < pipe->bank; i++) {
		l->USBHS_HSTDMASTATUS  = 0;
		l->USBHS_HSTDMAADDRESS = (uint32_t)&x->data[x->count];
		/* Size always < UHD_PIPE_MAX_TRANS */
		if (n_next > x->pkt_size[i]) {
			n_next = x->pkt_size[i];
		}

		if (i == pipe->bank - 1) {
			/* Last bank: no descriptor load, generate interrupt */
			l->USBHS_HSTDMACONTROL = USBHS_HSTDMACONTROL_CHANN_ENB | USBHS_HSTDMACONTROL_END_B_EN
			                         | USBHS_HSTDMACONTROL_END_BUFFIT | USBHS_HSTDMACONTROL_BUFF_LENGTH(n_next);
		} else {
			/* Not last: load descriptor, no interrupt */
			l->USBHS_HSTDMACONTROL = USBHS_HSTDMACONTROL_CHANN_ENB | USBHS_HSTDMACONTROL_LDNXT_DSC
			                         | USBHS_HSTDMACONTROL_END_B_EN | USBHS_HSTDMACONTROL_BUFF_LENGTH(n_next);
		}
		x->count += x->pkt_size[i];
		l = (UsbhsHstdma *)l->USBHS_HSTDMANXTDSC;
	}
	atomic_enter_critical(&flags);
	if (hri_usbhs_get_HSTPIPIMR_reg(drv->hw, pi, USBHS_HSTPIPIMR_NBUSYBKE)) {
		/* Transactions in progress, just continue */
		hri_usbhs_write_HSTPIPIDR_reg(drv->hw, pi, USBHS_HSTPIPIMR_NBUSYBKE);
		/* No need to wait SOF to start */
		pipe->periodic_start = 0;
	} else {
		/* Wait SOF to start */
		_usb_h_add_sof_user(drv); /* SOF User: periodic start (HBW) */
	}
	/* Load descriptor list */
	hri_usbhs_write_HSTDMACONTROL_reg(drv->hw, pi, USBHS_HSTDMACONTROL_LDNXT_DSC | USBHS_HSTDMACONTROL_CHANN_ENB);
	atomic_leave_critical(&flags);
}
#endif

int32_t _usb_h_high_bw_out(struct usb_h_pipe *pipe, uint8_t *data, uint32_t length, uint16_t trans_pkt_size[3])
{
#if _HPL_USB_H_HBW_SP
	hal_atomic_t flags;
	int8_t       pi = _usb_h_pipe_i(pipe);

	ASSERT(pipe && pipe->hcd && pipe->hcd->hw && pipe->hcd->prvt);

	/* Check parameters */
	if (!pipe->high_bw_out) {
		return USB_H_ERR_UNSP_OP;
	}
	if (trans_pkt_size[0] > pipe->max_pkt_size || trans_pkt_size[1] > pipe->max_pkt_size
	    || trans_pkt_size[2] > pipe->max_pkt_size) {
		return USB_H_ERR_ARG;
	}
	if (!_usb_h_ll_available(pi, pipe->bank)) {
		return USB_H_ERR_NO_RSC;
	}
	/* Check state */
	atomic_enter_critical(&flags);
	if (pipe->x.general.state == USB_H_PIPE_S_FREE) {
		atomic_leave_critical(&flags);
		return USB_H_ERR_NOT_INIT;
	} else if (pipe->x.general.state != USB_H_PIPE_S_IDLE) {
		atomic_leave_critical(&flags);
		return USB_H_BUSY;
	}
	pipe->x.general.state = USB_H_PIPE_S_DATO;
	atomic_leave_critical(&flags);

	pipe->x.hbw.data        = data;
	pipe->x.hbw.size        = length;
	pipe->x.hbw.count       = 0;
	pipe->x.hbw.pkt_size[0] = trans_pkt_size[0] ? trans_pkt_size[0] : pipe->max_pkt_size;
	pipe->x.hbw.pkt_size[1] = trans_pkt_size[1] ? trans_pkt_size[1] : pipe->max_pkt_size;
	pipe->x.hbw.pkt_size[2] = trans_pkt_size[2] ? trans_pkt_size[2] : pipe->max_pkt_size;
	pipe->periodic_start    = true;

	/* Enable auto switch for DMA */
	hri_usbhs_set_HSTPIPCFG_AUTOSW_bit(pipe->hcd->hw, pi);
	if (trans_pkt_size[0] || trans_pkt_size[1] || trans_pkt_size[2]) {
		/* Do linked list with transaction packets */
		_usb_h_ll_dma_out(pipe, _usb_h_ll_get(pi, pipe->bank));
	} else {
		/* Do single DMA to handle multiple packets */
		_usb_h_dma(pipe, false);
	}
	return USB_H_OK;
#else
	return USB_H_ERR_UNSP_OP;
#endif
}

bool _usb_h_pipe_is_busy(struct usb_h_pipe *pipe)
{
	uint8_t s;
	ASSERT(pipe);
	s = pipe->x.general.state;
	return !(s == USB_H_PIPE_S_IDLE || s == USB_H_PIPE_S_FREE);
}

static void _usb_h_end_transfer(struct usb_h_pipe *pipe, int32_t code)
{
	uint8_t             s  = pipe->x.general.state;
	struct _usb_h_prvt *pd = (struct _usb_h_prvt *)pipe->hcd->prvt;
	if (s < USB_H_PIPE_S_SETUP || s > USB_H_PIPE_S_STATO) {
		return; /* Not busy */
	}
	if (pipe->type == 0 /* Control */) {
		_usb_h_rm_req_user(pipe->hcd);
		_usb_h_rm_sof_user(pipe->hcd); /* SOF user: control timeout */
	}
	pipe->x.general.state  = USB_H_PIPE_S_IDLE;
	pipe->x.general.status = code;
	if (pipe->done) {
		pipe->done(pipe);
	}
	/* Suspend delayed due to control request: start it */
	if (pd->n_ctrl_req_user == 0 && pd->suspend_start < 0) {
		_usbhs_suspend(pipe->hcd);
	}
}

static void _usb_h_abort_transfer(struct usb_h_pipe *pipe, int32_t code)
{
	struct usb_h_desc *drv = (struct usb_h_desc *)pipe->hcd;
	uint8_t            pi  = _usb_h_pipe_i(pipe);

#if CONF_USB_H_DMA_SP
	if (pipe->dma) {
		/* Freeze pipe */
		hri_usbhs_write_HSTPIPIER_reg(drv->hw, pi, USBHS_HSTPIPIMR_PFREEZE);
		/* Update transfer count */
		pipe->x.bii.count = (uint32_t)hri_usbhs_read_HSTDMAADDRESS_reg(drv->hw, pi - 1) - (uint32_t)pipe->x.bii.data;
		/* For OUT there is data in FIFO to be discarded */
		if ((pipe->ep & 0x80) == 0) {
			uint8_t bnk;
			for (bnk = 0; bnk < pipe->bank; bnk++) {
				if (pipe->x.bii.count >= pipe->max_pkt_size) {
					pipe->x.bii.count -= pipe->max_pkt_size;
				}
			}
		}
		/* Stop DMA */
		hri_usbhs_write_HSTDMACONTROL_reg(drv->hw, pi - 1, 0);
	}
#endif

	/* Reset pipe to stop transfer */
	hri_usbhs_set_HSTPIP_reg(drv->hw, USBHS_HSTPIP_PRST0 << pi);
	hri_usbhs_clear_HSTPIP_reg(drv->hw, USBHS_HSTPIP_PRST0 << pi);

	/* Disable interrupts */
	hri_usbhs_write_HSTPIPIDR_reg(drv->hw,
	                              pi,
	                              USBHS_HSTPIPIMR_RXINE | USBHS_HSTPIPIMR_TXOUTE | USBHS_HSTPIPIMR_TXSTPE
	                                  | USBHS_HSTPIPIMR_RXSTALLDE | USBHS_HSTPIPIMR_SHORTPACKETIE);
	_usb_h_end_transfer(pipe, code);
}

void _usb_h_pipe_abort(struct usb_h_pipe *pipe)
{
	ASSERT(pipe && pipe->hcd && pipe->hcd->hw && pipe->hcd->prvt);
	_usb_h_abort_transfer(pipe, USB_H_ABORT);
}

void _usb_h_suspend(struct usb_h_desc *drv)
{
	ASSERT(drv);
	_usbhs_suspend(drv);
}

void _usb_h_resume(struct usb_h_desc *drv)
{
	ASSERT(drv);
	_usbhs_wakeup(drv);
}

#undef DEPRECATED
