#ifndef USBCONFIG_H
#define USBCONFIG_H

#include "usb.h"
#include "usb_cdc.h"

#define USB_PLL_DIV USBPLL_SETCLK_4_0

#define USB_RESET_HANDLER
//#define USB_SUSPEND_HANDLER
//#define USB_RESUME_HANDLER

#define USB_SETUP_REQUEST_HANDLER
#define USB_SET_CONFIGURATION_HANDLER
//#define USB_SET_ADDRESS_HANDLER
//#define USB_GET_DESCRIPTOR_HOOK
#define USB_EP0_WRITE_HANDLER

#define USB_ENDPOINT_IN_HANDLER
#define USB_ENDPOINT_OUT_HANDLER

#define USB_DEVICE_CLASS USB_CLASS_CDC
#define USB_DEVICE_SUBCLASS USB_SUBCLASS_ACM
#define USB_DEVICE_PROTOCOL 1

#define USB_DEVICE_VENDOR 0x16C0
#define USB_DEVICE_PRODUCT 0x05DC
#define USB_DEVICE_VERSION 0

#define USB_CONFIG_COUNT 1

const USB_DEVICE_DESCR usb_device_descr = {
	sizeof(USB_DEVICE_DESCR),
	USB_DESCR_DEVICE,
	0x0101,
	USB_DEVICE_CLASS,
	USB_DEVICE_SUBCLASS,
	USB_DEVICE_PROTOCOL,
	USB_EP0_MAX_PACKET_SIZE,
	USB_DEVICE_VENDOR,
	USB_DEVICE_PRODUCT,
	USB_DEVICE_VERSION,
	1,
	2,
	0,
	USB_CONFIG_COUNT
};

const struct {
	USB_CONFIG_DESCR c0;
	USB_INTERFACE_DESCR c1;
	uint8_t c2[19];
	USB_ENDPOINT_DESCR c3;
	USB_INTERFACE_DESCR c4;
	USB_ENDPOINT_DESCR c5;
	USB_ENDPOINT_DESCR c6;
} __attribute__((packed)) usb_config1_descr = {
	{
		sizeof(USB_CONFIG_DESCR),
		USB_DESCR_CONFIG,
		sizeof(usb_config1_descr),
		2,
		1,
		0,
		USB_CONFIG_ATTR_RESERVED | USB_CONFIG_ATTR_REMOTE_WAKEUP,
		100 / 2
	},
	{
		sizeof(USB_INTERFACE_DESCR),
		USB_DESCR_INTERFACE,
		0,
		0,
		1,
		USB_DEVICE_CLASS,
		USB_DEVICE_SUBCLASS,
		USB_DEVICE_PROTOCOL,
		0
	},
	{
		5,           /* sizeof(usbDescrCDC_HeaderFn): length of descriptor in bytes */
		0x24,        /* descriptor type */
		0,           /* header functional descriptor */
		0x10, 0x01,

		4,           /* sizeof(usbDescrCDC_AcmFn): length of descriptor in bytes */
		0x24,        /* descriptor type */
		2,           /* abstract control management functional descriptor */
		0x02,        /* SET_LINE_CODING,    GET_LINE_CODING, SET_CONTROL_LINE_STATE    */

		5,           /* sizeof(usbDescrCDC_UnionFn): length of descriptor in bytes */
		0x24,        /* descriptor type */
		6,           /* union functional descriptor */
		0,           /* CDC_COMM_INTF_ID */
		1,           /* CDC_DATA_INTF_ID */

		5,           /* sizeof(usbDescrCDC_CallMgtFn): length of descriptor in bytes */
		0x24,        /* descriptor type */
		1,           /* call management functional descriptor */
		3,           /* allow management on data interface, handles call management by itself */
		1,           /* CDC_DATA_INTF_ID */
	},
	{
		sizeof(USB_ENDPOINT_DESCR),
		USB_DESCR_ENDPOINT,
		USB_ENDPOINT_IN | USB_CDC_INTR_ENDPOINT,
		USB_ENDPOINT_TYPE_INTERRUPT,
		8,
		100
	},
	{
		sizeof(USB_INTERFACE_DESCR),
		USB_DESCR_INTERFACE,
		1,
		0,
		2,
		0x0A,
		0,
		0,
		0
	},
	{
		sizeof(USB_ENDPOINT_DESCR),
		USB_DESCR_ENDPOINT,
		USB_ENDPOINT_OUT | USB_CDC_BULK_ENDPOINT,
		USB_ENDPOINT_TYPE_BULK,
		USB_CDC_BULK_PACKET_SIZE,
		0
	},
	{
		sizeof(USB_ENDPOINT_DESCR),
		USB_DESCR_ENDPOINT,
		USB_ENDPOINT_IN | USB_CDC_BULK_ENDPOINT,
		USB_ENDPOINT_TYPE_BULK,
		USB_CDC_BULK_PACKET_SIZE,
		0
	}
};

const void *const usb_config_descr[USB_CONFIG_COUNT] = {&usb_config1_descr};

const uint8_t usb_string0_descr[] = {4, USB_DESCR_STRING, 9, 4};
const uint8_t usb_string1_descr[] = {2 + 2 * 4 + 2, USB_DESCR_STRING, 'T', 0, 'E', 0, 'S', 0, 'T', 0};
const uint8_t usb_string2_descr[] = {2 + 2 * 4 + 2, USB_DESCR_STRING, 'T', 0, 'E', 0, 'S', 0, 'T', 0};

#define USB_STRING_COUNT 3
const void *const usb_string_descr[USB_STRING_COUNT] = {&usb_string0_descr, &usb_string1_descr, &usb_string2_descr};

#endif
