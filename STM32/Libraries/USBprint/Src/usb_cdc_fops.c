/*
 * TODO:
 */

#include "usb_cdc_fops.h"
#include "circular_buffer.h"
#include <stdbool.h>

#define CDC_INIT_TIME 10

static int8_t CDC_Init_FS(void);
static int8_t CDC_DeInit_FS(void);
static int8_t CDC_Control_FS(uint8_t cmd, uint8_t* pbuf, uint16_t length);
static int8_t CDC_Receive_FS(uint8_t* pbuf, uint32_t *Len);
static int8_t CDC_TransmitCplt_FS(uint8_t *pbuf, uint32_t *Len, uint8_t epnum);

extern USBD_HandleTypeDef hUsbDeviceFS;
USBD_CDC_ItfTypeDef usb_cdc_fops =
{
        CDC_Init_FS,
        CDC_DeInit_FS,
        CDC_Control_FS,
        CDC_Receive_FS,
        CDC_TransmitCplt_FS
};

static USBD_CDC_LineCodingTypeDef LineCoding = {
        115200, /* baud rate     */
        0x00,   /* stop bits-1   */
        0x00,   /* parity - none */
        0x08    /* nb. of bits 8 */
};

// Internal data for rx/tx
static struct
{
    struct {
        cbuf_handle_t ctx;
        uint8_t irqBuf[CIRCULAR_BUFFER_SIZE];   // lower layer buffer for IRQ USB_CDC driver callback
    } tx, rx;
    enum {
    	closed,
		preOpen,
		open
    } isComPortOpen;
    unsigned long portOpenTime;
} usb_cdc_if = { {0}, {0}, closed, 0};

bool isComPortOpen() {
	// The USB CDC needs to be initialised and open for some time before the COM port is opened properly
	if (usb_cdc_if.isComPortOpen == open ||
	   (HAL_GetTick() - usb_cdc_if.portOpenTime > CDC_INIT_TIME && usb_cdc_if.isComPortOpen == preOpen))
	{
		usb_cdc_if.isComPortOpen = open;
		return true;
	}
	return false;
}

/**
  * @brief  Initializes the CDC media low layer over the FS USB IP
  * @retval USBD_OK if all operations are OK else USBD_FAIL
  */
static int8_t CDC_Init_FS(void)
{
    // Setup TX Buffer
    USBD_CDC_SetTxBuffer(&hUsbDeviceFS, usb_cdc_if.tx.irqBuf, 0);
    usb_cdc_if.tx.ctx = circular_buf_init(CIRCULAR_BUFFER_SIZE);

    // Setup RX Buffer
    USBD_CDC_SetRxBuffer(&hUsbDeviceFS, usb_cdc_if.rx.irqBuf);
    usb_cdc_if.rx.ctx = circular_buf_init(CIRCULAR_BUFFER_SIZE);

    // Default is no host attached.
    usb_cdc_if.isComPortOpen = false;

    return (USBD_OK);
}

/**
  * @brief  DeInitializes the CDC media low layer
  * @retval USBD_OK if all operations are OK else USBD_FAIL
  */
static int8_t CDC_DeInit_FS(void)
{
    // Nothing hear, never called.
    return (USBD_OK);
}

/**
  * @brief  Manage the CDC class requests
  * @param  cmd: Command code
  * @param  pbuf: Buffer containing command data (request parameters)
  * @param  length: Number of data to be sent (in bytes)
  * @retval Result of the operation: USBD_OK if all operations are OK else USBD_FAIL
  */
static int8_t CDC_Control_FS(uint8_t cmd, uint8_t* pbuf, uint16_t length)
{
    switch(cmd)
    {
        case CDC_SEND_ENCAPSULATED_COMMAND:    break;
        case CDC_GET_ENCAPSULATED_RESPONSE:    break;
        case CDC_SET_COMM_FEATURE:             break;
        case CDC_GET_COMM_FEATURE:             break;
        case CDC_CLEAR_COMM_FEATURE:           break;

    /*******************************************************************************/
    /* Line Coding Structure                                                       */
    /*-----------------------------------------------------------------------------*/
    /* Offset | Field       | Size | Value  | Description                          */
    /* 0      | dwDTERate   |   4  | Number |Data terminal rate, in bits per second*/
    /* 4      | bCharFormat |   1  | Number | Stop bits                            */
    /*                                        0 - 1 Stop bit                       */
    /*                                        1 - 1.5 Stop bits                    */
    /*                                        2 - 2 Stop bits                      */
    /* 5      | bParityType |  1   | Number | Parity                               */
    /*                                        0 - None                             */
    /*                                        1 - Odd                              */
    /*                                        2 - Even                             */
    /*                                        3 - Mark                             */
    /*                                        4 - Space                            */
    /* 6      | bDataBits  |   1   | Number Data bits (5, 6, 7, 8 or 16).          */
    /*******************************************************************************/
    case CDC_SET_LINE_CODING:
        LineCoding.bitrate = (uint32_t)(pbuf[0] | (pbuf[1] << 8) |
                (pbuf[2] << 16) | (pbuf[3] << 24));
        LineCoding.format = pbuf[4];
        LineCoding.paritytype = pbuf[5];
        LineCoding.datatype = pbuf[6];
    break;

    case CDC_GET_LINE_CODING:
        pbuf[0] = (uint8_t)(LineCoding.bitrate);
        pbuf[1] = (uint8_t)(LineCoding.bitrate >> 8);
        pbuf[2] = (uint8_t)(LineCoding.bitrate >> 16);
        pbuf[3] = (uint8_t)(LineCoding.bitrate >> 24);
        pbuf[4] = LineCoding.format;
        pbuf[5] = LineCoding.paritytype;
        pbuf[6] = LineCoding.datatype;
    break;

    case CDC_SET_CONTROL_LINE_STATE:
    	if ((((USBD_SetupReqTypedef *) pbuf)->wValue & 0x0001) == 0)
    	{
    		usb_cdc_if.isComPortOpen = closed;
    	}
    	else
    	{
    		usb_cdc_if.isComPortOpen = preOpen;
    		usb_cdc_if.portOpenTime = HAL_GetTick();
    	}
    break;

    case CDC_SEND_BREAK:    break;
    default:                break;
  }

  return (USBD_OK);
}

/**
  * @brief  Data received over USB OUT endpoint are sent over CDC interface
  *         through this function.
  *
  *         @note
  *         This function will issue a NAK packet on any OUT packet received on
  *         USB endpoint until exiting this function. If you exit this function
  *         before transfer is complete on CDC interface (ie. using DMA controller)
  *         it will result in receiving more data while previous ones are still
  *         not sent.
  *
  * @param  Buf: Buffer of data to be received
  * @param  Len: Number of data received (in bytes)
  * @retval Result of the operation: USBD_OK if all operations are OK else USBD_FAIL
  */
static int8_t CDC_Receive_FS(uint8_t* Buf, uint32_t *Len)
{
    USBD_CDC_SetRxBuffer(&hUsbDeviceFS, &Buf[0]);
    USBD_CDC_ReceivePacket(&hUsbDeviceFS);

    uint16_t len = (uint16_t)*Len;

    // Update circular buffer with incoming values
    for(uint8_t i = 0; i < len; i++)
        circular_buf_put(usb_cdc_if.rx.ctx, Buf[i]);

    memset(Buf, '\0', len); // clear the buffer
    return (USBD_OK);
}


void usb_cdc_rx_flush()
{
    if (!usb_cdc_if.tx.ctx)
        return; // Error, USB CDC is not initialized

    circular_buf_reset(usb_cdc_if.rx.ctx);
}

int usb_cdc_rx(uint8_t* rxByte)
{
    if (!usb_cdc_if.tx.ctx)
        return -1; // Error, USB CDC is not initialized

    return circular_buf_get(usb_cdc_if.rx.ctx, rxByte);
}

/**
  * @brief  CDC_Transmit_FS
  *         Data to send over USB IN endpoint are sent over CDC interface
  *         through this function.
  *
  * @param  Buf: Buffer of data to be sent
  * @param  Len: Number of data to be sent (in bytes)
  * @retval USBD_OK if all operations are OK else USBD_FAIL or USBD_BUSY
  */
ssize_t usb_cdc_transmit(const uint8_t* Buf, uint16_t Len)
{
    if (!usb_cdc_if.tx.ctx)
        return -1; // Error, USB CDC is not initialized

    USBD_CDC_HandleTypeDef *hcdc = (USBD_CDC_HandleTypeDef*)hUsbDeviceFS.pClassData;

    if (hcdc->TxState != 0)
    {
        // USB CDC is transmitting data to the network. Leave transmit handling to CDC_TransmitCplt_FS
        for (int len = 0;len < Len; len++)
        {
            if (circular_buf_put(usb_cdc_if.tx.ctx, *Buf))
                return len; // len < Len since not enough space in buffer. Leave error handling to caller.
            Buf++;
        }
        return Len;
    }

    // Fill in the data from buffer directly, no need copy bytes
    if (Len > sizeof(usb_cdc_if.tx.irqBuf))
    {
        // remaining bytes could be moved to circular buffer but in this case,
        // system is possible in a lack of resources. That problem can not be solved hear.
        Len = sizeof(usb_cdc_if.tx.irqBuf);
    }

    memcpy(usb_cdc_if.tx.irqBuf, Buf, Len);
    USBD_CDC_SetTxBuffer(&hUsbDeviceFS, usb_cdc_if.tx.irqBuf, Len);
    if (USBD_CDC_TransmitPacket(&hUsbDeviceFS) != USBD_OK)
        return -1; // Something went wrong in IO layer.

    // All good.
    return Len;
}

size_t usb_cdc_tx_available()
{
    if (!usb_cdc_if.tx.ctx)
        return 0; // Error, USB CDC is not initialised, for now return 0.

    return circular_buf_capacity(usb_cdc_if.tx.ctx) - circular_buf_size(usb_cdc_if.tx.ctx);
}

/**
  * @brief  CDC_TransmitCplt_FS
  *         Data transmited callback
  *
  *         @note
  *         This function is IN transfer complete callback used to inform user that
  *         the submitted Data is successfully sent over USB.
  *
  * @param  Buf: Buffer of data to be received
  * @param  Len: Number of data received (in bytes)
  * @retval Result of the operation: USBD_OK if all operations are OK else USBD_FAIL
  */
static int8_t CDC_TransmitCplt_FS(uint8_t *Buf, uint32_t *Len, uint8_t epnum)
{
    uint8_t result = USBD_OK;

    // Fill in the next number of bytes.
    uint16_t len = 0;
    for (uint8_t *ptr = usb_cdc_if.tx.irqBuf; ptr < &usb_cdc_if.tx.irqBuf[sizeof(usb_cdc_if.tx.irqBuf)]; ptr++)
    {
        if (circular_buf_get(usb_cdc_if.tx.ctx, ptr) != 0) {
            break; // No more bytes ready to transmit.
        }
        len++; // Yet another byte to send.
    }

    if (len != 0)
    {
        USBD_CDC_SetTxBuffer(&hUsbDeviceFS, usb_cdc_if.tx.irqBuf, len);
        result = USBD_CDC_TransmitPacket(&hUsbDeviceFS);
    }

    return result;
}
