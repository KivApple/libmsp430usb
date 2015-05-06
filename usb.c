#include <msp430.h>
#include <string.h>
#include "config.h"
#include "usb.h"
#include "usbconfig.h"

typedef struct {
	volatile uint8_t CNF;
	volatile uint8_t BAX;
	volatile uint8_t CTX;
	uint8_t reserved[2];
	volatile uint8_t BAY;
	volatile uint8_t CTY;
	volatile uint8_t SIZXY;
} __attribute__((packed)) USB_EPBD;

const void *volatile usb_ep0_response;
volatile unsigned int usb_ep0_response_len;
volatile USB_EPBD *const usb_in_epbd = ((USB_EPBD*)USBIEPCNF_1_) - 1;
volatile USB_EPBD *const usb_out_epbd = ((USB_EPBD*)USBOEPCNF_1_) - 1;
uint8_t usb_free_memory_ptr;

void usbEpDisable(uint8_t ep);

void usbInit(int connect) {
	USBKEYPID = USBKEY;
	USBCNF = 0;
	USBPHYCTL = PUSEL;
	USBPWRCTL = SLDOEN | VUSBEN;
	while ((USBPWRCTL & USBBGVBV) == 0);
	USBKEYPID = 0;
	if (connect) {
		usbConnect();
	}
}

void usbEnablePll() {
	USBKEYPID = USBKEY;
	USBPLLDIVB = USB_PLL_DIV;
	USBPLLCTL = UPFDEN | UPLLEN;
	do {
		USBPLLIR = 0;
		__delay_cycles(F_CPU / 1000); // 1ms delay
	} while (USBPLLIR != 0);
	USBKEYPID = 0;
}

void usbDisableEndpoints() {
	int i;
	for (i = 1; i < 8; i++) {
		usbEpDisable(i | USB_ENDPOINT_IN);
		usbEpDisable(i | USB_ENDPOINT_OUT);
	}
	usb_free_memory_ptr = 0;
}

void usbReset() {
	USBKEYPID = USBKEY;

	USBCTL = 0;
	USBFUNADR = 0;
	usb_ep0_response_len = 0;
	usb_configuration = 0;
	usb_test_mode = 0;
	usb_device_status = 0;

	USBOEPIE = 0;
	USBIEPIE = 0;
	USBOEPCNT_0 = NAK;
	USBOEPCNF_0 = UBME | USBIIE | STALL;
	USBIEPCNT_0 = NAK;
	USBIEPCNF_0 = UBME | USBIIE | STALL;
	usbDisableEndpoints();
	USBOEPIE = BIT0;
	USBIEPIE = BIT0;

#ifdef USB_RESET_HANDLER
	void usbResetHandler();
	usbResetHandler();
#endif

	USBCTL = FEN;
	USBIFG = 0;
	USBIE = SETUPIE | RSTRIE | SUSRIE;

	USBKEYPID = 0;
}

void usbConnect() {
	usbEnablePll();
	USBKEYPID = USBKEY;
	USBCNF |= USB_EN;
	USBKEYPID = 0;
	usbReset();
	USBKEYPID = USBKEY;
	USBCNF |= PUR_EN;
	USBKEYPID = 0;
}

void usbSuspend() {
	USBKEYPID = USBKEY;
	USBCTL |= FRSTE;
	USBIFG &= ~SUSRIFG;
	USBPLLCTL &= ~UPLLEN;
	USBIE = RESRIE;
	USBKEYPID = 0;
#ifdef USB_SUSPEND_HANDLER
	void usbSuspendHandler();
	usbSuspendHandler();
#endif
}

void usbResume() {
	USBKEYPID = USBKEY;
	usbEnablePll();
	USBIFG &= ~(RESRIFG | SUSRIFG);
	USBIE = SETUPIE | RSTRIE | SUSRIE;
	USBKEYPID = 0;
#ifdef USB_RESUME_HANDLER
	void usbResumeHandler();
	usbResumeHandler();
#endif
}

void usbEp0SendChunk() {
	int len;
	if (usb_ep0_response_len > USB_EP0_MAX_PACKET_SIZE) {
		len = USB_EP0_MAX_PACKET_SIZE;
	} else {
		len = usb_ep0_response_len;
	}
	memmove((void*)&USBIEP0BUF, usb_ep0_response, len);
	usb_ep0_response += len;
	usb_ep0_response_len -= len;
	USBIEPCNT_0 = len;
}

void usbEp0SendResponse(const void *buffer, unsigned int len) {
	volatile USB_DEVICE_REQUEST *rq = (void*)&USBSUBLK;
	if (rq->wLength < len) {
		len = rq->wLength;
	}
	usb_ep0_response = buffer;
	usb_ep0_response_len = len;
	usbEp0SendChunk();
}

void usbEp0SendNull() {
	usbEp0SendResponse(0, 0);
}

void usbEp0SendStall() {
	USBIEPCNF_0 |= STALL;
}

void usbEp0EnableRecv() {
	USBOEPCNT_0 = 0;
}

void usbGetDescriptor(USB_DEVICE_REQUEST *rq) {
#ifdef USB_GET_DESCRIPTOR_HOOK
	int usbGetDescriptorHook(USB_DEVICE_REQUEST *rq);
	if (usbGetDescriptorHook(rq)) return;
#endif
	uint8_t index = rq->wValue & 0xFF;
	switch (rq->wValue >> 8) {
		case USB_DESCR_DEVICE:
			usbEp0SendResponse(&usb_device_descr, usb_device_descr.bLength);
			break;
		case USB_DESCR_CONFIG:
			usbEp0SendResponse(&usb_config1_descr, sizeof(usb_config1_descr));
			break;
		case USB_DESCR_STRING:
			if (index < USB_STRING_COUNT) {
				usbEp0SendResponse(usb_string_descr[index],
					((USB_STRING_DESCR*)(usb_string_descr[index]))->bLength);
			} else {
				usbEp0SendStall();
			}
			break;
		default:
			usbEp0SendStall();
	}
}

void usbSetupRequest(USB_DEVICE_REQUEST *rq) {
	if ((rq->bmRequestType & USB_REQ_TYPE_MASK) == USB_REQ_TYPE_STANDARD) {
		uint8_t recipient = rq->bmRequestType & USB_REQ_RECIPIENT_MASK;
		switch (rq->bRequest) {
			case USB_REQ_STD_GET_STATUS:
				if (recipient == USB_REQ_RECIPIENT_DEVICE) {
					usbEp0SendResponse(&usb_device_status, sizeof(usb_device_status));
				} else {
					uint16_t tmp = 0;
					usbEp0SendResponse(&tmp, sizeof(tmp));
				}
				break;
			case USB_REQ_STD_CLEAR_FEATURE:
				if (recipient == USB_REQ_RECIPIENT_DEVICE) {
					if (rq->wValue == USB_DEVICE_FEATURE_REMOTE_WAKEUP) {
						usb_device_status &= ~USB_DEVICE_STATUS_REMOTE_WAKEUP;
					} else if (rq->wValue == USB_DEVICE_FEATURE_TEST_MODE) {
						usb_test_mode = 0;
					}
					usbEp0SendNull();
				} else {
					usbEp0SendStall();
				}
				break;
			case USB_REQ_STD_SET_FEATURE:
				if (recipient == USB_REQ_RECIPIENT_DEVICE) {
					if (rq->wValue == USB_DEVICE_FEATURE_REMOTE_WAKEUP) {
						usb_device_status |=USB_DEVICE_STATUS_REMOTE_WAKEUP;
					} else if (rq->wValue == USB_DEVICE_FEATURE_TEST_MODE) {
						usb_test_mode = 0;
					}
					usbEp0SendNull();
				} else {
					usbEp0SendStall();
				}
				break;
			case USB_REQ_STD_GET_CONFIGURATION:
				if (recipient == USB_REQ_RECIPIENT_DEVICE) {
					usbEp0SendResponse(&usb_configuration, sizeof(usb_configuration));
				} else {
					usbEp0SendStall();
				}
				break;
			case USB_REQ_STD_SET_CONFIGURATION:
				usbDisableEndpoints();
				if (recipient == USB_REQ_RECIPIENT_DEVICE) {
					usb_configuration = rq->wValue;
					usbEp0SendNull();
#ifdef USB_SET_CONFIGURATION_HANDLER
					void usbSetConfigurationHandler();
					usbSetConfiguratonHandler();
#endif
				} else {
					usbEp0SendStall();
				}
				break;
			case USB_REQ_STD_SET_ADDRESS:
				USBFUNADR = rq->wValue;
				usbEp0SendNull();
#ifdef USB_SET_ADDRESS_HANDLER
				void usbSetAddressHandler(uint8_t address);
				usbSetAddressHandler(rq->wValue);
#endif
				break;
			case USB_REQ_STD_GET_DESCRIPTOR:
				usbGetDescriptor(rq);
				break;
			case USB_REQ_STD_SET_DESCRIPTOR:
				usbEp0SendNull();
				break;
			default:
				usbEp0SendStall();
		}
	} else {
#ifdef USB_SETUP_REQUEST_HANDLER
		int usbSetupRequestHandler(USB_DEVICE_REQUEST *rq);
		if (!usbSetupRequestHandler(rq)) {
			usbEp0SendStall();
		}
#else
		usbEp0SendStall();
#endif
	}
}

void usbHandleSetupRequests() {
	volatile USB_DEVICE_REQUEST *rq = (void*)&USBSUBLK;
	while (1) {
		if ((rq->bmRequestType & USB_REQ_DIR_MASK) == USB_REQ_DIR_DEVICE_TO_HOST) {
			USBCTL |= DIR;
		} else {
			USBCTL &= ~DIR;
		}
		usbSetupRequest((void*)rq);
		if (USBIFG & STPOWIFG) {
			USBIFG &= ~(STPOWIFG | SETUPIFG);
		} else {
			break;
		}
	}
}

#define USB_BUFFER(base_addr) ((void*)(((base_addr) << 3) + USBSTABUFF_))

void *usbEpBuffer(uint8_t ep) {
	if (ep & USB_ENDPOINT_IN) {
		if (ep == 0) {
			return USBIEP0BUF;
		} else {
			if ((usb_in_epbd[ep].CNF & DBUF) && (usb_in_epbd[ep].CNF & TOGGLE)) {
				return USB_BUFFER(usb_in_epbd[ep].BAY);
			} else {
				return USB_BUFFER(usb_in_epbd[ep].BAX);
			}
	} else {
		return NULL;
	}
}

void usbEpSendData(uint8_t ep, const void *data, uint8_t len) {
	if (ep & USB_ENDPOINT_IN) {
		ep &= ~USB_ENDPOINT_IN;
		if (ep == 0) {
			usbEp0SendResponse(data, len);
		} else {
			if ((usb_in_epbd[ep].CNF & DBUF) && (usb_in_epbd[ep].CNF & TOGGLE)) {
				if (data) {
					memmove(USB_BUFFER(usb_in_epbd[ep].BAY), data, len);
				}
				usb_in_epbd[ep].CTY = len;
			} else {
				if (data) {
					memmove(USB_BUFFER(usb_in_epbd[ep].BAX), data, len);
				}
				usb_in_epbd[ep].CTX = len;
			}
		}
	}
}

void usbEpSendNull(uint8_t ep) {
	usbEpSendData(ep, 0, 0);
}

void usbEpSendStall(uint8_t ep) {
	if (ep & USB_ENDPOINT_IN) {
		ep &= ~USB_ENDPOINT_IN;
		if (ep == 0) {
			usbEp0SendStall();
		} else {
			usb_in_epbd[ep].CNF |= STALL;
		}
	}
}

void usbEpEnableRecv(uint8_t ep) {
	if (ep & USB_ENDPOINT_IN) return;
	if (ep == 0) {
		usbEp0EnableRecv();
	} else {
		if ((usb_out_epbd[ep].CNF & DBUF) && (usb_out_epbd[ep].CNF & TOGGLE)) {
			usb_out_epbd[ep].CTY = 0;
		} else {
			usb_out_epbd[ep].CTX = 0;
		}
	}
}

void usbEpSetup(uint8_t ep, uint8_t bufferSize, int doubleBuf) {
	volatile USB_EPBD *epbd;
	int in;
	if (ep & USB_ENDPOINT_IN) {
		ep &= ~USB_ENDPOINT_IN;
		epbd = &usb_in_epbd[ep];
		in = 1;
	} else {
		epbd = &usb_out_epbd[ep];
		in = 0;
	}
	if (ep == 0) return;
	epbd->CNF = 0;
	epbd->BAX = usb_free_memory_ptr;
	usb_free_memory_ptr += (bufferSize + 7) >> 3;
	if (doubleBuf) {
		epbd->BAY = usb_free_memory_ptr;
		usb_free_memory_ptr += (bufferSize + 7) >> 3;
		epbd->CNF |= DBUF;
	}
	epbd->SIZXY = bufferSize;
	epbd->CTX = NAK;
	epbd->CTY = NAK;
	epbd->CNF |= UBME | USBIIE;
	if (in) {
		USBIEPIE |= 1 << ep;
	} else {
		USBOEPIE |= 1 << ep;
	}
}

void usbEpDisable(uint8_t ep) {
	volatile USB_EPBD *epbd;
	if (ep & USB_ENDPOINT_IN) {
		ep &= ~USB_ENDPOINT_IN;
		epbd = &usb_in_epbd[ep];
		USBIEPIE &= ~(1 << ep);
	} else {
		epbd = &usb_out_epbd[ep];
		USBOEPIE &= ~(1 << ep);
	}
	if (ep == 0) return;
	epbd->CNF = 0;
	epbd->BAX = 0;
	epbd->BAY = 0;
	epbd->CTX = NAK;
	epbd->CTY = NAK;
	epbd->SIZXY = 0;
}

int usbEpBusy(uint8_t ep) {
	if (ep & USB_ENDPOINT_IN) {
		ep &= ~USB_ENDPOINT_IN;
		if (ep == 0) {
			return 0;
		} else {
			if ((usb_in_epbd[ep].CNF & DBUF) && (usb_in_epbd[ep].CNF & TOGGLE)) {
				return (usb_in_epbd[ep].CTY != NAK);
			} else {
				return (usb_in_epbd[ep].CTX != NAK);
			}
		}
	} else {
		return 0;
	}
}

void __attribute__((interrupt(USB_UBM_VECTOR))) USB_UBM_ISR(void) {
	if (USBIFG & SETUPIFG) {
		USBCTL |= FRSTE;
		usbHandleSetupRequests();
		USBIFG &= ~SETUPIFG;
	}
	int vector = USBVECINT;
	switch (vector) {
		case USBVECINT_RSTR:
			usbReset();
			break;
		case USBVECINT_SUSR:
			usbSuspend();
			break;
		case USBVECINT_RESR:
			usbResume();
			break;
		case USBVECINT_INPUT_ENDPOINT0:
			USBCTL |= FRSTE;
			USBOEPCNT_0 = 0;
			if (usb_ep0_response_len) {
				usbEp0SendChunk();
			} else {
				usbEp0SendStall();
			}
			break;
		case USBVECINT_OUTPUT_ENDPOINT0:
			USBCTL |= FRSTE;
#ifdef USB_EP0_WRITE_HANDLER
			{
				int usbEp0WriteHandler(void *data, uint8_t len);
				int r = usbEp0WriteHandler((void*)&USBOEP0BUF, USBOEPCNT_0 & ~NAK);
				if (r) {
					USBOEPCNT_0 &= ~NAK;
				} else {
					USBOEPCNF_0 |= STALL;
				}
			}
#else
			USBOEPCNF_0 |= STALL;
#endif
			break;
		case USBVECINT_SETUP_PACKET_RECEIVED:
			USBCTL |= FRSTE;
			usbHandleSetupRequests();
			break;
		default:
			if ((vector >= USBVECINT_INPUT_ENDPOINT1) && (vector <= USBVECINT_INPUT_ENDPOINT7)) {
				uint8_t ep = ((vector - USBVECINT_INPUT_ENDPOINT1) >> 1) + 1;
#ifdef USB_ENDPOINT_IN_HANDLER
				void usbEndpointInHandler(uint8_t ep);
				usbEndpointInHandler(ep);
#else
				usbEpSendStall(ep);
#endif
			} else if ((vector >= USBVECINT_OUTPUT_ENDPOINT1) && (vector <= USBVECINT_OUTPUT_ENDPOINT7)) {
				uint8_t ep = ((vector - USBVECINT_OUTPUT_ENDPOINT1) >> 1) + 1;
#ifdef USB_ENDPOINT_OUT_HANDLER
				int curBuf = (usb_out_epbd[ep].CNF & DBUF) && ((usb_out_epbd[ep].CNF & TOGGLE) == 0);
				uint8_t *buf = USB_BUFFER(curBuf ? usb_out_epbd[ep].BAY : usb_out_epbd[ep].BAX);
				int count = (curBuf ? usb_out_epbd[ep].CTY : usb_out_epbd[ep].CTX) & ~NAK;
				int usbEndpointOutHandler(uint8_t ep, void *data, uint8_t len);
				if (!usbEndpointOutHandler(ep, buf, count)) {
					usb_out_epbd[ep].CNF |= STALL;
				} else {
					if ((usb_out_epbd[ep].CNF & DBUF) && (usb_out_epbd[ep].CNF & TOGGLE)) {
						usb_out_epbd[ep].CTY = 0;
					} else {
						usb_out_epbd[ep].CTX = 0;
					}
				}
#else
				usb_out_epbd[ep].CNF |= STALL;
#endif
			}
	}
}
