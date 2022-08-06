/**
 * \file
 *
 * \brief USB Host Stack CDC ACM Function Implementation.
 *
 * Copyright (c) 2017-2018 Microchip Technology Inc. and its subsidiaries.
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

#include "cdchf_acm.h"

static int32_t cdchf_acm_ctrl(struct usbhf_driver *func, enum usbhf_control ctrl, void *param);

/**
 * \brief Reset software instance
 *
 * \param cdc The pointer to software instance
 */
static void cdchf_acm_reset(struct cdchf_acm *cdc)
{
	memset(&cdc->iface, 0, sizeof(struct cdchf_acm) - sizeof(struct usbhf_driver));
	cdc->iface      = -1;
	cdc->iface_data = -1;
}

/** \internal
 *  \brief Reading done
 */
void cdchf_acm_read_done(struct usb_h_pipe *pipe)
{
	struct cdchf_acm *cdc = CDCHF_ACM_PTR(pipe->owner);
	cdc->is_reading       = false;
	if (pipe->x.general.status != USB_H_OK) {
		cdc->is_error = cdc->is_open;
	}
	if (cdc->read_cb) {
		cdc->read_cb(cdc, pipe->x.bii.count);
	}
}

/** \internal
 *  \brief Writing done
 */
void cdchf_acm_write_done(struct usb_h_pipe *pipe)
{
	struct cdchf_acm *cdc = CDCHF_ACM_PTR(pipe->owner);
	cdc->is_writing       = false;
	if (pipe->x.general.status != USB_H_OK) {
		cdc->is_error = cdc->is_open;
	}
	if (cdc->write_cb) {
		cdc->write_cb(cdc, pipe->x.bii.count);
	}
}

/** \internal
 *  \brief SOF callback
 *  \param core Pointer to USB Host Core instance
 *  \param hdl  Pointer to extended handler instance
 */
static void cdchf_acm_sof(struct usbhc_driver *core, struct usbhc_handler_ext *hdl)
{
	struct cdchf_acm *cdc;
	(void)core;
	if (hdl->func != (FUNC_PTR)cdchf_acm_sof) {
		return;
	}
	cdc = (struct cdchf_acm *)hdl->ext;

	if (cdc->is_reading) {
		if (cdc->rx_count == cdc->pipe_in->x.bii.count) {
			cdc->rx_to--;
			if (cdc->rx_to == 0) {
				cdc->is_reading = false;
				usb_h_pipe_abort(cdc->pipe_in);
			}
		} else {
			cdc->rx_count = cdc->pipe_in->x.bii.count;
			cdc->rx_to    = CONF_CDCHF_ACM_RD_TO;
		}
	}

	if (cdc->is_writing) {
		if (cdc->tx_count == cdc->pipe_out->x.bii.count) {
			cdc->tx_to--;
			if (cdc->tx_to == 0) {
				cdc->is_writing = false;
				usb_h_pipe_abort(cdc->pipe_out);
			}
		} else {
			cdc->tx_count = cdc->pipe_out->x.bii.count;
			cdc->tx_to    = CONF_CDCHF_ACM_WR_TO;
		}
	}

	if (!cdc->is_reading && !cdc->is_writing) {
		usbhc_unregister_handler(core, USBHC_HDL_SOF, USBHC_HDL_PTR(&cdc->sof_hdl));
	}
}

static int32_t cdchf_acm_set_line_coding(struct usbhc_driver *core, struct cdchf_acm *cdc,
                                         usb_cdc_line_coding_t *line_coding)
{
	struct usb_req *req = (struct usb_req *)core->ctrl_buf;
	usbhd_take_control(cdc->func.pdev, NULL);
	usb_fill_SetLineCoding_req(req, (uint8_t)cdc->iface);
	if (usbhc_request(core, (uint8_t *)req, (uint8_t *)line_coding) != ERR_NONE) {
		usbhc_release_control(core);
		cdc->is_error      = true;
		cdc->is_requesting = false;
		return ERR_IO;
	}
	return ERR_NONE;
}

static int32_t cdchf_acm_set_control(struct usbhc_driver *core, struct cdchf_acm *cdc, uint16_t val)
{
	struct usb_req *req = (struct usb_req *)core->ctrl_buf;
	usbhd_take_control(cdc->func.pdev, NULL);
	usb_fill_SetControlLineState_req(req, (uint8_t)cdc->iface, val);
	if (usbhc_request(core, (uint8_t *)req, NULL) != ERR_NONE) {
		usbhc_release_control(core);
		cdc->is_error      = true;
		cdc->is_requesting = false;
		return ERR_IO;
	}
	return ERR_NONE;
}

static void cdchf_acm_set_line_coding_done(struct usb_h_pipe *pipe, struct cdchf_acm *cdc)
{
	struct usbhc_driver *core = usbhf_get_core(cdc);
	if (pipe->x.general.status != USB_H_OK) {
		cdc->is_error      = true;
		cdc->is_requesting = false;
	} else if (ERR_NONE != cdchf_acm_set_control(core, cdc, CDC_CTRL_SIGNAL_DTE_PRESENT)) {
		/* Status modification already done */
	} else {
		return;
	}
	if (cdc->ctrl_cb) {
		cdc->ctrl_cb(cdc);
	}
}

static void cdchf_acm_set_control_done(struct usb_h_pipe *pipe, struct cdchf_acm *cdc, uint16_t val)
{
	if (pipe->x.general.status != USB_H_OK) {
		cdc->is_error = true;
	} else if (val & CDC_CTRL_SIGNAL_DTE_PRESENT) {
		cdc->is_open = true;
	} else {
		cdc->is_open = false;
	}
	cdc->is_requesting = false;
	if (cdc->ctrl_cb) {
		cdc->ctrl_cb(cdc);
	}
}

static void cdchf_acm_req_done(struct usbhd_driver *dev, struct usb_h_pipe *pipe)
{
	struct usb_req *  req = (struct usb_req *)pipe->x.ctrl.setup;
	struct cdchf_acm *cdc = CDCHF_ACM_PTR(dev->func.pfunc);
	if (dev->dev_addr != pipe->dev) {
		return;
	}
	if (!dev->status.bm.usable) {
		return;
	}
	if (req->bRequest != USB_REQ_CDC_SET_LINE_CODING && req->bRequest != USB_REQ_CDC_SET_CONTROL_LINE_STATE) {
		return;
	}
	while (cdc) {
		if (cdc->func.ctrl != cdchf_acm_ctrl) {
			cdc = CDCHF_ACM_PTR(cdc->func.next.pfunc);
			continue;
		}
		if (cdc->iface != req->wIndex && cdc->iface_data != req->wIndex) {
			cdc = CDCHF_ACM_PTR(cdc->func.next.pfunc);
			continue;
		}
		if (req->bRequest == USB_REQ_CDC_SET_LINE_CODING) {
			cdchf_acm_set_line_coding_done(pipe, cdc);
			return;
		} else { /* USB_REQ_CDC_SET_CONTROL_LINE_STATE */
			cdchf_acm_set_control_done(pipe, cdc, req->wValue);
			return;
		}
	}
}

static struct usbhc_handler cdchf_acm_req_hdl = {NULL, (FUNC_PTR)cdchf_acm_req_done};

/** \brief Uninstall the CDC function driver
 *  \param cdc Pointer to the CDC function driver instance
 */
static inline void cdchf_acm_uninstall(struct cdchf_acm *cdc)
{
	cdchf_acm_cb_t ctrl_cb;
	if (cdc->pipe_in) {
		usb_h_pipe_abort(cdc->pipe_in);
		usb_h_pipe_free(cdc->pipe_in);
		cdc->pipe_in = NULL;
	}
	if (cdc->pipe_out) {
		usb_h_pipe_abort(cdc->pipe_out);
		usb_h_pipe_free(cdc->pipe_out);
		cdc->pipe_out = NULL;
	}
	if (!cdc->is_enabled) {
		cdchf_acm_reset(cdc);
		return;
	}
	usbhc_unregister_handler(usbhf_get_core(cdc), USBHC_HDL_REQ, &cdchf_acm_req_hdl);
	usbhc_unregister_handler(usbhf_get_core(cdc), USBHC_HDL_SOF, USBHC_HDL_PTR(&cdc->sof_hdl));

	ctrl_cb = cdc->ctrl_cb;
	cdchf_acm_reset(cdc);
	if (ctrl_cb) {
		ctrl_cb(cdc);
	}
}

/** \brief Try to install the CDC driver
 *  \retval ERR_NONE installed
 *  \retval ERR_NOT_FOUND not installed
 *  \retval ERR_NO_CHANGE already taken
 */
static inline int32_t cdchf_acm_install(struct cdchf_acm *cdc, struct usbh_descriptors *desc)
{
	struct usbhd_driver *  dev  = usbhf_get_dev(cdc);
	struct usbhc_driver *  core = usbhf_get_core(cdc);
	struct usb_h_desc *    hcd  = core->hcd;
	struct usb_iface_desc *piface, *piface1;
	struct usb_ep_desc *   pep = NULL;
	struct usb_h_pipe *    pipe;
	uint8_t *              pd, *eod;
	if (cdc->is_installed) {
		/* Driver already in use */
		return ERR_NO_CHANGE;
	}
	/* Find very first CDC interface */
	pd = usb_find_desc(desc->sod, desc->eod, USB_DT_INTERFACE);
	if (!pd) {
		/* No interface found */
		return ERR_NOT_FOUND;
	}
	piface = (struct usb_iface_desc *)pd;
	if (piface->bInterfaceClass != CDC_CLASS_COMM && piface->bInterfaceClass != CDC_CLASS_DATA) {
		/* No CDC interface found */
		return ERR_NOT_FOUND;
	}
	pd      = usb_find_desc(usb_desc_next(pd), desc->eod, USB_DT_INTERFACE);
	piface1 = (struct usb_iface_desc *)pd;
	/* Install case:
	 * COMM + DATA, DATA + COMM
	 * There must be COMM for setup requests
	 */
	if (piface1 == NULL || (piface1->bInterfaceClass != CDC_CLASS_COMM && piface1->bInterfaceClass != CDC_CLASS_DATA)) {
		/* CDC must have two interfaces */
		return ERR_NOT_FOUND;
	}
	/* Eod points to next iface */
	eod = usb_find_iface_after(pd, desc->eod, piface1->bInterfaceNumber);
	/* Both iface is ready, arrange classes */
	if (piface->bInterfaceClass == CDC_CLASS_DATA) {
		/* piface1 must be COMM, check CDC_CS_INTERFACE CALL Management */
		/* ... */
	} else {
		/* piface -> DATA, piface1 -> COMM */
		piface1 = piface;
		piface  = (struct usb_iface_desc *)pd;
	}
	if (piface1->bInterfaceSubClass != CDC_SUBCLASS_ACM || piface1->bInterfaceProtocol > CDC_PROTOCOL_V25TER) {
		return ERR_NOT_FOUND;
	}
	pd = (uint8_t *)piface;
	/* Install endpoints for DATA
	 */
	while (1) {
		pd  = usb_find_ep_desc(usb_desc_next((uint8_t *)pd), desc->eod);
		pep = (struct usb_ep_desc *)pd;
		if (pep == NULL) {
			break;
		}
		if ((pep->bmAttributes & USB_EP_TYPE_MASK) == USB_EP_TYPE_BULK) {
			pipe = usb_h_pipe_allocate(hcd,
			                           dev->dev_addr,
			                           pep->bEndpointAddress,
			                           pep->wMaxPacketSize,
			                           pep->bmAttributes,
			                           pep->bInterval,
			                           dev->speed,
			                           true);
			if (pipe == NULL) {
				cdchf_acm_uninstall(cdc);
				return ERR_NO_RESOURCE;
			}
			pipe->owner = (void *)cdc;
			if (pep->bEndpointAddress & 0x80) {
				cdc->pipe_in = pipe;
			} else {
				cdc->pipe_out = pipe;
			}
		}
	}
	/* Endpoints must be installed */
	if (!(cdc->pipe_in && cdc->pipe_out)) {
		cdchf_acm_uninstall(cdc);
		return ERR_NOT_FOUND;
	}
	usb_h_pipe_register_callback(cdc->pipe_in, cdchf_acm_read_done);
	usb_h_pipe_register_callback(cdc->pipe_out, cdchf_acm_write_done);
	/* Update descriptors pointers */
	desc->sod = eod;
	/* Update status */
	cdc->iface        = piface1->bInterfaceNumber;
	cdc->iface_data   = piface->bInterfaceNumber;
	cdc->is_installed = true;

	cdc->sof_hdl.func = (FUNC_PTR)cdchf_acm_sof;
	cdc->sof_hdl.ext  = (void *)cdc;

	usbhc_register_handler(core, USBHC_HDL_REQ, &cdchf_acm_req_hdl);
	return ERR_NONE;
}

/** \brief Callback invoked on install/uninstall the function driver
 *  \param func  Pointer to the function driver instance
 *  \param ctrl  Control operation code
 *  \param param Parameter for install/uninstall
 */
static int32_t cdchf_acm_ctrl(struct usbhf_driver *func, enum usbhf_control ctrl, void *param)
{
	switch (ctrl) {
	case USBHF_INSTALL:
		return cdchf_acm_install(CDCHF_ACM_PTR(func), (struct usbh_descriptors *)param);
	case USBHF_UNINSTALL:
		cdchf_acm_uninstall(CDCHF_ACM_PTR(func));
		break;
	case USBHF_ENABLE:
		CDCHF_ACM_PTR(func)->is_enabled = true;
		break;
	default:
		return ERR_INVALID_ARG;
	}
	return ERR_NONE;
}

int32_t cdchf_acm_init(struct usbhc_driver *core, struct cdchf_acm *cdc)
{
	int32_t rc = 0;
	if (cdc->is_installed) {
		return ERR_DENIED;
	}
	rc = usbhc_register_funcd(core, USBHF_PTR(cdc));
	if (rc) {
		return rc;
	}
	cdchf_acm_reset(cdc);
	cdc->func.ctrl = cdchf_acm_ctrl;

	return ERR_NONE;
}

int32_t cdchf_acm_deinit(struct usbhc_driver *core, struct cdchf_acm *cdc)
{
	int32_t rc = 0;
	if (cdc->is_installed) {
		return ERR_DENIED;
	}
	rc = usbhc_unregister_funcd(core, USBHF_PTR(cdc));
	if (rc) {
		return rc;
	}
	return ERR_NONE;
}

void cdchf_acm_register_callback(struct cdchf_acm *cdc, enum cdchf_acm_cb_type cb_type, FUNC_PTR cb)
{
	ASSERT(cdc);
	switch (cb_type) {
	case CDCHF_ACM_CTRL_CB:
		cdc->ctrl_cb = (cdchf_acm_cb_t)cb;
		break;
	case CDCHF_ACM_READ_CB:
		cdc->read_cb = (cdchf_acm_data_cb_t)cb;
		break;
	case CDCHF_ACM_WRITE_CB:
		cdc->write_cb = (cdchf_acm_data_cb_t)cb;
		break;
	default:
		return;
	}
}

int32_t cdchf_acm_open(struct cdchf_acm *cdc, usb_cdc_line_coding_t *line_coding)
{
	struct usbhc_driver *core = NULL;
	hal_atomic_t         flags;

	ASSERT(cdc);

	if (cdc->is_open) {
		return ERR_NONE;
	}

	atomic_enter_critical(&flags);
	if (cdc->is_requesting) {
		atomic_leave_critical(&flags);
		return ERR_BUSY;
	}
	cdc->is_requesting = true;
	atomic_leave_critical(&flags);

	/* Obtain control access */
	if (usbhd_take_control(cdc->func.pdev, &core) != ERR_NONE) {
		cdc->is_requesting = false;
		return ERR_BUSY;
	}
	cdc->is_error = false;
	if (line_coding) {
		/* Set LineCoding and open */
		return cdchf_acm_set_line_coding(core, cdc, line_coding);
	}
	/* Set control line state to open */
	return cdchf_acm_set_control(core, cdc, CDC_CTRL_SIGNAL_DTE_PRESENT);
}

int32_t cdchf_acm_close(struct cdchf_acm *cdc)
{
	struct usbhc_driver *core = NULL;
	hal_atomic_t         flags;

	ASSERT(cdc);

	if (!cdc->is_open) {
		return ERR_NONE;
	}

	atomic_enter_critical(&flags);
	if (cdc->is_requesting) {
		atomic_leave_critical(&flags);
		return ERR_BUSY;
	}
	cdc->is_requesting = true;
	atomic_leave_critical(&flags);

	/* Obtain control access */
	if (usbhd_take_control(cdc->func.pdev, &core) != ERR_NONE) {
		cdc->is_requesting = false;
		return ERR_BUSY;
	}

	usb_h_pipe_abort(cdc->pipe_in);
	usb_h_pipe_abort(cdc->pipe_out);

	cdc->is_writing = false;
	cdc->is_reading = false;
	cdc->need_flush = false;
	cdc->is_error   = false;

	/* Set control line state to close */
	return cdchf_acm_set_control(core, cdc, 0);
}

int32_t cdchf_acm_read(struct cdchf_acm *cdc, uint8_t *buf, uint32_t size)
{
	hal_atomic_t flags;
	int32_t      rc;

	ASSERT(cdc && buf && size);

	if (!cdc->is_open) {
		return ERR_DENIED;
	}
	atomic_enter_critical(&flags);
	if (cdc->is_reading) {
		atomic_leave_critical(&flags);
		return ERR_BUSY;
	}
	cdc->is_reading = true;
	atomic_leave_critical(&flags);

	cdc->is_error = false;
	cdc->rx_to    = CONF_CDCHF_ACM_RD_TO;
	usbhc_register_handler(usbhf_get_core(cdc), USBHC_HDL_SOF, USBHC_HDL_PTR(&cdc->sof_hdl));
	rc = usb_h_bulk_int_iso_xfer(cdc->pipe_in, buf, size, false);
	if (rc) {
		return rc;
	}
	return ERR_NONE;
}

int32_t cdchf_acm_write(struct cdchf_acm *cdc, uint8_t *buf, uint32_t size)
{
	hal_atomic_t flags;
	int32_t      rc;

	ASSERT(cdc && buf && size);

	if (!cdc->is_open) {
		return ERR_DENIED;
	}
	atomic_enter_critical(&flags);
	if (cdc->is_writing) {
		atomic_leave_critical(&flags);
		return ERR_BUSY;
	}
	cdc->is_writing = true;
	atomic_leave_critical(&flags);

	cdc->is_error = false;
	cdc->tx_to    = CONF_CDCHF_ACM_WR_TO;
	usbhc_register_handler(usbhf_get_core(cdc), USBHC_HDL_SOF, USBHC_HDL_PTR(&cdc->sof_hdl));
	cdc->need_flush = (size & (cdc->pipe_out->max_pkt_size - 1)) == 0;
	rc              = usb_h_bulk_int_iso_xfer(cdc->pipe_out, buf, size, false);
	if (rc) {
		return rc;
	}
	return ERR_NONE;
}

void cdchf_acm_write_flush(struct cdchf_acm *cdc)
{
	hal_atomic_t flags;
	if (!cdc->is_open) {
		return;
	}
	if (!cdc->need_flush) {
		return;
	}
	atomic_enter_critical(&flags);
	if (cdc->is_writing) {
		atomic_leave_critical(&flags);
		return;
	}
	cdc->is_error   = false;
	cdc->is_writing = true;
	cdc->need_flush = false;
	atomic_leave_critical(&flags);
	/* Send ZLP */
	usb_h_bulk_int_iso_xfer(cdc->pipe_out, NULL, 0, true);
}
