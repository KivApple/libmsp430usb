#ifndef USB_CDC_H
#define USB_CDC_H

#include "usb.h"

#define USB_CDC_BULK_ENDPOINT 1
#define USB_CDC_INTR_ENDPOINT 2

#define USB_CDC_BULK_PACKET_SIZE 40

//#define USB_CDC_DATA_SENT_HANDLER
#define USB_CDC_DATA_RECEIVED_HANDLER

void usbCdcInit();

void usbCdcEnable();
int usbCdcSetupRequestHandler(USB_DEVICE_REQUEST *rq);
int usbCdcEp0WriteHandler(void *data, uint8_t len);
void usbCdcIntrIn();
void usbCdcBulkIn();
int usbCdcBulkOut(uint8_t *data, uint8_t len);
int usbCdcBusy();

void usbCdcSendData(void *data, uint16_t len, int wait);
void usbCdcSendString(char *str, int wait);

#endif
