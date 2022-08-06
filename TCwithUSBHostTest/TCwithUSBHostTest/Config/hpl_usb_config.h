/* Auto-generated config file hpl_usb_config.h */
#ifndef HPL_USB_CONFIG_H
#define HPL_USB_CONFIG_H

// <<< Use Configuration Wizard in Context Menu >>>

#define CONF_USB_N_0 0
#define CONF_USB_N_1 1
#define CONF_USB_N_2 2
#define CONF_USB_N_3 3
#define CONF_USB_N_4 4
#define CONF_USB_N_5 5
#define CONF_USB_N_6 6
#define CONF_USB_N_7 7
#define CONF_USB_N_8 8
#define CONF_USB_N_9 9
#define CONF_USB_N_10 10

// <h> USB Host HAL Configurations

// <q> USB Host Instance Owner Support
// <i> Add owner field in driver instances, for upper layer to use
// <i> Add owner pointer in driver descriptor and pipe descriptor
// <i> Turn off it to optimize memory while it's not used
// <id> usbh_inst_owner_sp
#ifndef CONF_USB_H_INST_OWNER_SP
#define CONF_USB_H_INST_OWNER_SP 1
#endif

// </h>

// <h> USB Host Implement Configurations

// <y> Number of root hub ports
// <CONF_USB_N_1"> 1
// <i> There is only one port in USB host root hub for this chip.
// <i> According to the status code of root hub change callback, the max supported port number is 31.
// <id> usbh_arch_n_rh_port
#ifndef CONF_USB_H_NUM_RH_PORT
#define CONF_USB_H_NUM_RH_PORT CONF_USB_N_1
#endif

// <y> Max Endpoint Number supported
// <CONF_USB_N_10"> 10 (EP0A/EP8A)
// <i> The USB host peripheral only accepts endpoint number <= 10
// <id> usbh_arch_max_ep_n
#ifndef CONF_USB_H_MAX_EP_N
#define CONF_USB_H_MAX_EP_N CONF_USB_N_10
#endif

// <y> Max number of pipes supported
// <i> Limits the number of pipes can be used in upper layer.
// <CONF_USB_N_1"> 1 (Pipe0 only)
// <CONF_USB_N_2"> 2 (Pipe0 + 1 Pipe)
// <CONF_USB_N_3"> 3 (Pipe0 + 2 pipes)
// <CONF_USB_N_4"> 4 (Pipe0 + 3 pipes)
// <CONF_USB_N_5"> 5 (Pipe0 + 4 pipes)
// <CONF_USB_N_6"> 6 (Pipe0 + 5 pipes)
// <CONF_USB_N_7"> 7 (Pipe0 + 6 pipes)
// <CONF_USB_N_8"> 8 (Pipe0 + 7 pipes)
// <CONF_USB_N_9"> 9 (Pipe0 + 8 pipes)
// <CONF_USB_N_10"> 10 (Pipe0 + 9 pipes)
// <id> usbh_num_pipe_sp
#ifndef CONF_USB_H_NUM_PIPE_SP
#define CONF_USB_H_NUM_PIPE_SP CONF_USB_N_7
#endif

// <e> USB Host DMA transfer support
// <i> This defines whether DMA operation is supported.
// <i> Note that endpoint 1 ~ 7 can use DMA operation.
// <id> usbh_dma_sp
#ifndef CONF_USB_H_DMA_SP
#define CONF_USB_H_DMA_SP 1
#endif

// <y> Max number of high bandwidth pipes supported
// <i> DMA descriptor list will be used to handle high bandwidth transactions in this case
// <i> For each transaction of high bandwidth pipe, a DMA descriptor is used so it consume more resource
// <CONF_USB_N_0"> Not supported
// <CONF_USB_N_1"> 1
// <CONF_USB_N_2"> 2
// <CONF_USB_N_3"> 3
// <CONF_USB_N_4"> 4
// <CONF_USB_N_5"> 5
// <CONF_USB_N_6"> 6
// <CONF_USB_N_7"> 7
// <id> usbh_arch_num_high_bw_pipe_sp
#ifndef CONF_USB_H_NUM_HIGH_BW_PIPE_SP
#define CONF_USB_H_NUM_HIGH_BW_PIPE_SP CONF_USB_N_0
#endif

// </e>

// <e> USB Host VBus supply control
// <id> usbh_arch_vbus_ctrl
#ifndef CONF_USB_H_VBUS_CTRL
#define CONF_USB_H_VBUS_CTRL 0
#endif

// <s> External function name for VBus on/off control
// <i> The function prototype: void function_name(void *drv, uint8_t port, bool onoff).
// <id> usbh_arch_vbus_ctrl_func
#ifndef CONF_USB_H_VBUS_CTRL_FUNC_NAME
#define CONF_USB_H_VBUS_CTRL_FUNC_NAME "my_vbus_ctrl_func"
#endif

extern void my_vbus_ctrl_func(void *drv, uint8_t port, bool onoff);
#ifndef CONF_USB_H_VBUS_CTRL_FUNC
#define CONF_USB_H_VBUS_CTRL_FUNC my_vbus_ctrl_func
#endif
// </e>

// </h>

// <<< end of configuration section >>>

#endif // HPL_USB_CONFIG_H
