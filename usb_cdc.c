#include <msp430.h>
#include <string.h>
#include "usb.h"
#include "usb_cdc.h"

enum {
	SEND_ENCAPSULATED_COMMAND = 0,
	GET_ENCAPSULATED_RESPONSE,
	SET_COMM_FEATURE,
	GET_COMM_FEATURE,
	CLEAR_COMM_FEATURE,
	SET_LINE_CODING = 0x20,
	GET_LINE_CODING,
	SET_CONTROL_LINE_STATE,
	SEND_BREAK
};

volatile uint8_t usb_cdc_line_coding[7];
volatile uint8_t usb_cdc_set_line_coding;
const uint8_t usb_cdc_state[10] = {0xa1, 0x20, 0, 0, 0, 0, 2, 0, 3, 0};
volatile uint8_t usb_cdc_state_sending;
volatile uint16_t usb_cdc_send_buffer_len;
volatile uint8_t *usb_cdc_send_buffer;

void usbCdcEnable() {
	memset((void*)usb_cdc_line_coding, 0, sizeof(usb_cdc_line_coding));
	usb_cdc_set_line_coding = 0;
	usb_cdc_state_sending = 0;
	usb_cdc_send_buffer_len = 0;
	usbEpSetup(USB_CDC_INTR_ENDPOINT | USB_ENDPOINT_IN, 8, 1);
	usbEpSetup(USB_CDC_BULK_ENDPOINT | USB_ENDPOINT_IN, USB_CDC_BULK_PACKET_SIZE, 1);
	usbEpSetup(USB_CDC_BULK_ENDPOINT | USB_ENDPOINT_OUT, USB_CDC_BULK_PACKET_SIZE, 1);
	usbEpEnableRecv(USB_CDC_BULK_ENDPOINT | USB_ENDPOINT_OUT);
}

int usbCdcSetupRequestHandler(USB_DEVICE_REQUEST *rq) {
	if ((rq->bmRequestType & USB_REQ_TYPE_MASK) == USB_REQ_TYPE_CLASS) {
		if (rq->bRequest == SET_LINE_CODING) {
			usb_cdc_set_line_coding = 1;
			usbEp0EnableRecv();
			return 1;
		} else if (rq->bRequest == GET_LINE_CODING) {
			usbEp0SendResponse(&usb_cdc_line_coding, sizeof(usb_cdc_line_coding));
			return 1;
		} else if (rq->bRequest == SET_CONTROL_LINE_STATE) {
			usbEp0SendNull();
			usbEpSendData(USB_CDC_INTR_ENDPOINT | USB_ENDPOINT_IN, usb_cdc_state, 8);
			usb_cdc_state_sending = 1;
			return 1;
		}
		usbEp0SendNull();
		return 1;
	}
	return 0;
}

int usbCdcEp0WriteHandler(void *data, uint8_t len) {
	if (usb_cdc_set_line_coding) {
		if (len >= sizeof(usb_cdc_line_coding)) {
			memmove((void*)usb_cdc_line_coding, data, sizeof(usb_cdc_line_coding));
		}
		return 0;
	} else {
		return 0;
	}
}

void usbCdcIntrIn() {
	if (usb_cdc_state_sending) {
		usbEpSendData(USB_CDC_INTR_ENDPOINT | USB_ENDPOINT_IN, usb_cdc_state + 8, 2);
	} else {
		usbEpSendNull(USB_CDC_INTR_ENDPOINT | USB_ENDPOINT_IN);
	}
}

void usbCdcBulkIn() {
	if (usbEpBusy(USB_CDC_BULK_ENDPOINT | USB_ENDPOINT_IN)) return;
	if (usb_cdc_send_buffer_len == 0xFFFF) {
		usb_cdc_send_buffer_len = 0;
		usbEpSendNull(USB_CDC_BULK_ENDPOINT | USB_ENDPOINT_IN);
#ifdef USB_CDC_DATA_SENT_HANDLER
		void usbCdcDataSentHandler();
		usbCdcDataSentHandler();
#endif
		P1OUT &= ~BIT0;
	} else if (usb_cdc_send_buffer_len) {
		P1OUT |= BIT0;
		uint8_t len = 0;
		if (usb_cdc_send_buffer_len > USB_CDC_BULK_PACKET_SIZE) {
			len = USB_CDC_BULK_PACKET_SIZE;
			usb_cdc_send_buffer_len -= USB_CDC_BULK_PACKET_SIZE;
		} else {
			len = usb_cdc_send_buffer_len;
			usb_cdc_send_buffer_len = 0xFFFF;
		}
		uint8_t *data = (void*)usb_cdc_send_buffer;
		usb_cdc_send_buffer += len;
		usbEpSendData(USB_CDC_BULK_ENDPOINT | USB_ENDPOINT_IN, data, len);
	}
}

int usbCdcBulkOut(uint8_t *data, uint8_t len) {
	if (len) {
#ifdef USB_CDC_DATA_RECEIVED_HANDLER
		void usbCdcDataReceivedHandler(uint8_t *data, uint8_t len);
		usbCdcDataReceivedHandler(data, len);
#endif
	}
	return 1;
}

void usbCdcSendData(void *data, uint16_t len, int wait) {
	while (usb_cdc_send_buffer_len);
	usb_cdc_send_buffer = data;
	usb_cdc_send_buffer_len = len;
	usbCdcBulkIn();
	if (wait) {
		while (usb_cdc_send_buffer_len);
	}
}

void usbCdcSendString(char *str, int wait) {
	usbCdcSendData(str, strlen(str), wait);
}

int usbCdcBusy() {
	return (usb_cdc_send_buffer_len);
}
