/**
 * \file
 *
 * \brief USB Host Stack CDC ACM Function Definition.
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
 */

#ifndef _CDCHF_ACM_H_
#define _CDCHF_ACM_H_

#include "usbhf.h"
#include "usb_protocol_cdc.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Time out if two data packets exceeds the period on reading */
#define CONF_CDCHF_ACM_RD_TO 100
/** Time out if two data packets exceeds the period on writing */
#define CONF_CDCHF_ACM_WR_TO 100

struct cdchf_acm;

/** \brief Mouse callback types */
enum cdchf_acm_cb_type {
	CDCHF_ACM_CTRL_CB,  /**< CDC_ACM control callback (en/dis/open/close/err) */
	CDCHF_ACM_READ_CB,  /**< CDC_ACM data read callback */
	CDCHF_ACM_WRITE_CB, /**< CDC_ACM data write callback */
	CDCHF_ACM_CB_N
};

/** \brief CDC Serial control callback */
typedef void (*cdchf_acm_cb_t)(struct cdchf_acm *func);
/** \brief CDC Serial data callback */
typedef void (*cdchf_acm_data_cb_t)(struct cdchf_acm *func, uint32_t count);

/** USB CDC Host ACM serial Function Driver support */
struct cdchf_acm {
	/** General data for USB Host function driver. */
	struct usbhf_driver func;
	/** Interface for the function */
	int8_t iface;
	/** Function is installed */
	uint8_t is_installed : 1;
	/** Function is enabled */
	volatile uint8_t is_enabled : 1;
	/** Requesting */
	uint8_t is_requesting : 1;
	/** Virtual serial port is open */
	uint8_t is_open : 1;
	/** Pending read in background */
	uint8_t is_reading : 1;
	/** Pending write in background */
	uint8_t is_writing : 1;
	/** Need flush on writing */
	uint8_t need_flush : 1;
	/** There is error during operation */
	uint8_t is_error : 1;
	/** Data interface for the function */
	int8_t iface_data;
	/** Pipes - IN/OUT */
	struct usb_h_pipe *pipe_in;
	struct usb_h_pipe *pipe_out;
	/** Control callback */
	cdchf_acm_cb_t ctrl_cb;
	/** Read callback */
	cdchf_acm_data_cb_t read_cb;
	/** Write callback */
	cdchf_acm_data_cb_t write_cb;
	/** SOF handler */
	struct usbhc_handler_ext sof_hdl;
	/** RX count */
	uint32_t rx_count;
	/** TX count */
	uint32_t tx_count;
	/** RX timeout */
	uint16_t rx_to;
	/** TX timeout */
	uint16_t tx_to;
};

/** Cast a pointer to USB CDC Host ACM Function Driver */
#define CDCHF_ACM_PTR(p) ((struct cdchf_acm *)(p))

/** \brief Initialize the CDC ACM function driver and attach it to core
 *  \param core Pointer to the USB host core instance
 *  \param func Pointer to the CDC ACM function driver instance
 *  \return Operation result status
 */
int32_t cdchf_acm_init(struct usbhc_driver *core, struct cdchf_acm *func);

/** \brief Deinitialize the CDC ACM function driver and detach it from core
 *  \param core Pointer to the USB host core instance
 *  \param func Pointer to the CDC ACM function driver instance
 *  \return Operation result status
 */
int32_t cdchf_acm_deinit(struct usbhc_driver *core, struct cdchf_acm *func);

/** \brief Return true if CDC_ACM function driver is installed
 *  \param[in, out] cdc     Pointer to CDC ACM instance
 *  \return \c true if the function driver is installed
 */
static inline bool cdcfh_acm_is_installed(struct cdchf_acm *cdc)
{
	return cdc->is_installed;
}

/** \brief Return true if CDC_ACM function driver is enabled
 *  \param[in, out] cdc     Pointer to CDC ACM instance
 *  \return \c true if the function driver is enabled
 */
static inline bool cdchf_acm_is_enabled(struct cdchf_acm *cdc)
{
	return cdc->is_enabled;
}

/** \brief Return true if CDC_ACM serial port is open
 *  \param[in, out] cdc     Pointer to CDC ACM instance
 *  \return \c true if the CDC_ACM serial port is open
 */
static inline bool cdchf_acm_is_open(struct cdchf_acm *cdc)
{
	return cdc->is_open;
}

/** \brief Return true if there is error on last operation
 *  \param[in, out] cdc     Pointer to CDC ACM instance
 *  \return \c true if there is error
 */
static inline bool cdchf_acm_is_error(struct cdchf_acm *cdc)
{
	return cdc->is_error;
}

/** \brief Return true if the serial port is busy on writing
 *  \param[in, out] cdc     Pointer to CDC ACM instance
 *  \return \c true if writing is pending in background
 */
static inline bool cdchf_acm_is_writing(struct cdchf_acm *cdc)
{
	return cdc->is_writing;
}

/** \brief Return true if the serial port is busy on reading
 *  \param[in, out] cdc     Pointer to CDC ACM instance
 *  \return \c true if reading is pending in background
 */
static inline bool cdchf_acm_is_reading(struct cdchf_acm *cdc)
{
	return cdc->is_reading;
}

/** \brief Register callback for CDC ACM serial port
 *  \param[in, out] cdc     Pointer to CDC ACM instance
 *  \param[in]      cb_type Callback type (\ref cdchf_acm_cb_type)
 *  \param[in]      cb      Pointer to the callback function
 */
void cdchf_acm_register_callback(struct cdchf_acm *cdc, enum cdchf_acm_cb_type cb_type, FUNC_PTR cb);

/** \brief Open the CDC ACM serial port
 *  \param[in, out] cdc         Pointer to CDC ACM instance
 *  \param[in]      line_coding Pointer to line coding information
 *  \return Operation result status
 *  \retval ERR_NONE Open without any error
 *  \retval ERR_NOT_READY Start request for opening
 *  \retval ERR_BUSY Busy, request not started
 *  \retval ERR_IO Operation error
 */
int32_t cdchf_acm_open(struct cdchf_acm *cdc, usb_cdc_line_coding_t *line_coding);

/** \brief Close the CDC ACM serial port
 *  \param[in, out] cdc         Pointer to CDC ACM instance
 *  \return Operation result status
 *  \retval ERR_NONE Close without any error
 *  \retval ERR_NOT_READY Start request for closing
 *  \retval ERR_BUSY Busy R/W, request not started
 *  \retval ERR_IO Operation error
 */
int32_t cdchf_acm_close(struct cdchf_acm *cdc);

/** \brief Start reading on the CDC ACM serial port
 *  The data directly goes through bulk IN endpoint, so when total amount of
 *  bytes are not archived, the driver keeps reading if device keeps sending
 *  full size packets, the reading terminates if device sends a short packet.
 *  \param[in, out] cdc  Pointer to CDC ACM instance
 *  \param[out]     buf  Pointer to buffer to fill received data
 *  \param[in]      size Total number of bytes expected
 *  \return Operation result status
 */
int32_t cdchf_acm_read(struct cdchf_acm *cdc, uint8_t *buf, uint32_t size);

/** \brief Start writing on the CDC ACM serial port
 *  The data directly goes through bulk OUT endpoint, so keep sending full size
 *  packets means possible more data, in that case zero length packet (ZLP)
 *  is better to be sent to terminate data waiting on device side. The ZLP is
 *  sent by invoking cdchf_acm_write_flush.
 *  \param[in, out] cdc  Pointer to CDC ACM instance
 *  \param[in]      buf  Pointer to buffer to fill received data
 *  \param[in]      size Total number of bytes expected
 *  \return Operation result status
 */
int32_t cdchf_acm_write(struct cdchf_acm *cdc, uint8_t *buf, uint32_t size);

/** \brief Flush for writing
 *  \param[in, out] cdc  Pointer to CDC ACM instance
 */
void cdchf_acm_write_flush(struct cdchf_acm *cdc);

#ifdef __cplusplus
}
#endif

#endif /* USBDF_CDC_ACM_SER_H_ */
