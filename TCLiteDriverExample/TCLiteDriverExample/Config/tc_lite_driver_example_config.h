/**
 * \file
 *
 * \brief Configuration header
 *
 * Copyright (c) 2019 Microchip Technology Inc. and its subsidiaries.
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
 * THIS SOFTWARE IS SUPPLIED BY MICROCHIP "AS IS".  NO WARRANTIES,
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
/*
 * Support and FAQ: visit <a href="https://www.microchip.com/support/">Microchip Support</a>
 */

#ifndef _TC_LITE_DRIVER_EXAMPLE_CONFIG_H_INCLUDED
#define _TC_LITE_DRIVER_EXAMPLE_CONFIG_H_INCLUDED

// <h> Basic Configuration

// <y> Select TC instance
// <TC0"> TC0
// <TC1"> TC1
// <TC2"> TC2
// <TC3"> TC3
// <i> Select TC instance for this example. Note: TC2 and TC3 instance is not valid for SAMG devices
// <id> tc_lite_driver_example_instance
#ifndef TC_LITE_DRIVER_EXAMPLE_INSTANCE
#define TC_LITE_DRIVER_EXAMPLE_INSTANCE TC1 //TC2
#endif

// </h>

#endif /* _TC_LITE_DRIVER_EXAMPLE_CONFIG_H_INCLUDED */
