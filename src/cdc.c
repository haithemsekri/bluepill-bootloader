//  CDC code from https://github.com/Apress/Beg-STM32-Devel-FreeRTOS-libopencm3-GCC/blob/master/rtos/usbcdcdemo/usbcdc.c
#include <libopencm3/usb/usbd.h>
#include <libopencm3/usb/cdc.h>
#include <logger.h>
#include "usb_conf.h"
#include "cdc.h"

#define CONTROL_CALLBACK_TYPE (USB_REQ_TYPE_CLASS | USB_REQ_TYPE_INTERFACE)
#define CONTROL_CALLBACK_MASK (USB_REQ_TYPE_TYPE | USB_REQ_TYPE_RECIPIENT)

/*
 * USB Control Requests:
 */
static enum usbd_request_return_codes
cdcacm_control_request(
  usbd_device *usbd_dev __attribute__((unused)),
  struct usb_setup_data *req,
  uint8_t **buf __attribute__((unused)),
  uint16_t *len,
  void (**complete)(
    usbd_device *usbd_dev,
    struct usb_setup_data *req
  ) __attribute__((unused))
) {
	dump_usb_request("*** cdcacm_control", req); ////
    if (req->wIndex != INTF_COMM && req->wIndex != INTF_DATA) {
		//  Not for my interface.  Hand off to next interface.
        ////return USBD_REQ_NEXT_CALLBACK;
    }
    debug_print("*** cdcacm_control "); debug_print_unsigned(req->bRequest); debug_println(""); // debug_flush(); ////
	switch (req->bRequest) {
	case USB_CDC_REQ_SET_CONTROL_LINE_STATE: {
		/* From https://github.com/libopencm3/libopencm3-examples/blob/master/examples/stm32/f3/stm32f3-discovery/usb_cdcacm/cdcacm.c
		 * This Linux cdc_acm driver requires this to be implemented
		 * even though it's optional in the CDC spec, and we don't
		 * advertise it in the ACM functional descriptor.
		 */
		char local_buf[10];
		struct usb_cdc_notification *notif = (void *)local_buf;

		/* We echo signals back to host as notification. */
		notif->bmRequestType = 0xA1;
		notif->bNotification = USB_CDC_NOTIFY_SERIAL_STATE;
		notif->wValue = 0;
		notif->wIndex = 0;
		notif->wLength = 2;
		local_buf[8] = req->wValue & 3;
		local_buf[9] = 0;
		// usbd_ep_write_packet(0x83, buf, 10);
		return USBD_REQ_HANDLED;
	}
	case USB_CDC_REQ_SET_LINE_CODING: {
		if ( *len < sizeof(struct usb_cdc_line_coding) ) {
			debug_print("*** cdcacm_control notsupp line_coding "); debug_print_unsigned(sizeof(struct usb_cdc_line_coding)); 
			debug_print(", len "); debug_print_unsigned(*len);
			debug_println(""); debug_flush(); ////
			return USBD_REQ_NOTSUPP;
		}
		return USBD_REQ_HANDLED;
	}}
    debug_print("*** cdcacm_control next "); debug_print_unsigned(req->bRequest); debug_println(""); debug_flush(); ////
	return USBD_REQ_NEXT_CALLBACK;  //  Previously USBD_REQ_NOTSUPP
}

//  TODO: TX Up to MAX_USB_PACKET_SIZE
//  usbd_ep_write_packet(usbd_dev, DATA_IN, txbuf, txlen)

static char cdcbuf[MAX_USB_PACKET_SIZE + 1];   // rx buffer

/*
 * USB Receive Callback:
 */
static void
cdcacm_data_rx_cb(
  usbd_device *usbd_dev,
  uint8_t ep __attribute__((unused))
) {
	uint16_t len = usbd_ep_read_packet(usbd_dev, DATA_OUT, cdcbuf, MAX_USB_PACKET_SIZE);
    if (len == 0) { return; }
    uint16_t pos = (len < MAX_USB_PACKET_SIZE) ? len : MAX_USB_PACKET_SIZE;
    cdcbuf[pos] = 0;
    debug_print("<< "); debug_println(cdcbuf); ////
/*
	// How much queue capacity left?
	unsigned rx_avail = uxQueueSpacesAvailable(usb_rxq);
	char buf[64];	// rx buffer
	int len, x;

	if ( rx_avail <= 0 )
		return;	// No space to rx

	// Bytes to read
	len = sizeof buf < rx_avail ? sizeof buf : rx_avail;

	// Read what we can, leave the rest:
	len = usbd_ep_read_packet(usbd_dev, 0x01, buf, len);

	for ( x=0; x<len; ++x ) {
		// Send data to the rx queue
		xQueueSend(usb_rxq,&buf[x],0);
	}
*/
}

static enum usbd_request_return_codes
dump_control_request(
  usbd_device *usbd_dev __attribute__((unused)),
  struct usb_setup_data *req,
  uint8_t **buf __attribute__((unused)),
  uint16_t *len,
  void (**complete)(
    usbd_device *usbd_dev,
    struct usb_setup_data *req
  ) __attribute__((unused))
) {
	dump_usb_request(">> ", req); ////
	return USBD_REQ_NEXT_CALLBACK;
}

/*
 * USB Configuration:
 */
static void
cdcacm_set_config(
  usbd_device *usbd_dev,
  uint16_t wValue __attribute__((unused))
) {
	//  From https://github.com/libopencm3/libopencm3-examples/blob/master/examples/stm32/f3/stm32f3-discovery/usb_cdcacm/cdcacm.c
    debug_println("*** cdcacm_set_config"); ////
	usbd_ep_setup(usbd_dev, DATA_OUT, USB_ENDPOINT_ATTR_BULK, MAX_USB_PACKET_SIZE, cdcacm_data_rx_cb);
	usbd_ep_setup(usbd_dev, DATA_IN, USB_ENDPOINT_ATTR_BULK, MAX_USB_PACKET_SIZE, NULL);
	usbd_ep_setup(usbd_dev, COMM_IN, USB_ENDPOINT_ATTR_INTERRUPT, COMM_PACKET_SIZE, NULL);

#ifdef NOTUSED
	usbd_ep_setup(usbd_dev,
		DATA_OUT,
		USB_ENDPOINT_ATTR_BULK,
		MAX_USB_PACKET_SIZE,
		cdcacm_data_rx_cb);
	usbd_ep_setup(usbd_dev,
		DATA_IN,
		USB_ENDPOINT_ATTR_BULK,
		MAX_USB_PACKET_SIZE,
		NULL);
#endif  //  NOTUSED

	int status = usbd_register_control_callback(
		usbd_dev,
		CONTROL_CALLBACK_TYPE,
		CONTROL_CALLBACK_MASK,
		cdcacm_control_request);
	if (status < 0) {
    	debug_println("*** cdcacm_set_config failed"); debug_flush(); ////
	}
#ifdef NOTUSED
	//  Debug all requests
	status = usbd_register_control_callback(
		usbd_dev,
		0,
		0,
		dump_control_request);
	if (status < 0) {
    	debug_println("*** dump_control_request failed"); debug_flush(); ////
	}
#endif  //  NOTUSED	
}

void cdc_setup(usbd_device* usbd_dev) {
    debug_println("*** cdc_setup"); ////
	usbd_register_set_config_callback(usbd_dev, cdcacm_set_config);
}
