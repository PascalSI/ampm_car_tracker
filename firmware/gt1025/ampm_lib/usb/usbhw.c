/*----------------------------------------------------------------------------
 *      U S B  -  K e r n e l
 *----------------------------------------------------------------------------
 *      Name:    USBHW.C
 *      Purpose: USB Hardware Layer Module for ST STM32F10x
 *      Version: V1.10
 *----------------------------------------------------------------------------
 *      This file is part of the uVision/ARM development tools.
 *      This software may only be used under the terms of a valid, current,
 *      end user licence from KEIL for a compatible version of KEIL software
 *      development tools. Nothing else gives you the right to use it.
 *
 *      Copyright (c) 2005-2007 Keil Software.
 *---------------------------------------------------------------------------*/

/* Double Buffering is not yet supported              */

#include <stm32f10x.h>                           // stm32f10x definitions

#include "type.h"

#include "usb.h"
#include "usbcfg.h"
#include "usbreg.h"
#include "usbhw.h"
#include "usbuser.h"
#include "usbcore.h"
#include "lib/sys_tick.h"
#include "hw_config.h"

#define EP_BUF_ADDR     (sizeof(EP_BUF_DSCR)*USB_EP_NUM) /* Endpoint Buffer Start Address */

/* Pointer to Endpoint Buffer Descriptors */
EP_BUF_DSCR *pBUF_DSCR = (EP_BUF_DSCR *)USB_PMA_ADDR;

/* Endpoint Free Buffer Address */
uint16_t FreeBufAddr;


/*
 *  Reset Endpoint
 *    Parameters:      EPNum: Endpoint Number
 *                       EPNum.0..3: Address
 *                       EPNum.7:    Dir
 *    Return Value:    None
 */

void EP_Reset (uint32_t EPNum) {
  uint32_t num, val;

  num = EPNum & 0x0F;
  val = EPxREG(num);
  if (EPNum & 0x80) {                       /* IN Endpoint */
    EPxREG(num) = val & (EP_MASK | EP_DTOG_TX);
  } else {                                  /* OUT Endpoint */
    EPxREG(num) = val & (EP_MASK | EP_DTOG_RX);
  }
}


/*
 *  Set Endpoint Status
 *    Parameters:      EPNum: Endpoint Number
 *                       EPNum.0..3: Address
 *                       EPNum.7:    Dir
 *                     stat: New Status
 *    Return Value:    None
 */

void EP_Status (uint32_t EPNum, uint32_t stat) {
  uint32_t num, val;

  num = EPNum & 0x0F;
  val = EPxREG(num);
  if (EPNum & 0x80) {                       /* IN Endpoint */
    EPxREG(num) = (val ^ (stat & EP_STAT_TX)) & (EP_MASK | EP_STAT_TX);
  } else {                                  /* OUT Endpoint */
    EPxREG(num) = (val ^ (stat & EP_STAT_RX)) & (EP_MASK | EP_STAT_RX);
  }
}


/*
 *  USB Initialize Function
 *   Called by the User to initialize USB
 *    Return Value:    None
 */

void USB_Init (void) {
	RCC->APB1ENR |= RCC_APB1ENR_USBEN;/*enable clock for USB*/
  /* Enable USB Interrupts */
	NVIC_SetPriority(USB_LP_CAN1_RX0_IRQn, ((0x01<<3)| USB_IRQ_PRIORITY));
	NVIC_EnableIRQ(USB_LP_CAN1_RX0_IRQn);
}


/*
 *  USB Connect Function
 *   Called by the User to Connect/Disconnect USB
 *    Parameters:      con:   Connect/Disconnect
 *    Return Value:    None
 */

void USB_Connect (uint32_t con) {

  CNTR = CNTR_FRES;                         /* Force USB Reset */
  ISTR = 0;                                 /* Clear Interrupt Status */
  if (con) {
    CNTR = CNTR_RESETM;                     /* USB Reset Interrupt Mask */
  } else {
		CNTR = CNTR_FRES | CNTR_LPMODE | CNTR_FSUSP;
		USB_Connected = 0;
    //CNTR = CNTR_FRES | CNTR_PDWN;           /* Switch Off USB Device */
  }
}


/*
 *  USB Reset Function
 *   Called automatically on USB Reset
 *    Return Value:    None
 */

void USB_Reset (void) {
/* Double Buffering is not yet supported              */

  ISTR = 0;                                 /* Clear Interrupt Status */

  CNTR = CNTR_CTRM | CNTR_RESETM |
         (USB_SUSPEND_EVENT ? CNTR_SUSPM   : 0) |
         (USB_WAKEUP_EVENT  ? CNTR_WKUPM   : 0) |
         (USB_ERROR_EVENT   ? CNTR_ERRM    : 0) |
         (USB_ERROR_EVENT   ? CNTR_PMAOVRM : 0) |
         (USB_SOF_EVENT     ? CNTR_SOFM    : 0) |
         (USB_SOF_EVENT     ? CNTR_ESOFM   : 0);

  FreeBufAddr = EP_BUF_ADDR;

  BTABLE = 0x00;                            /* set BTABLE Address */

  /* Setup Control Endpoint 0 */
  
  pBUF_DSCR->ADDR_TX = FreeBufAddr;
  FreeBufAddr += USB_MAX_PACKET0;
  pBUF_DSCR->ADDR_RX = FreeBufAddr;
  FreeBufAddr += USB_MAX_PACKET0;
  if (USB_MAX_PACKET0 > 62) {
    pBUF_DSCR->COUNT_RX = ((USB_MAX_PACKET0 << 5) - 1) | 0x8000;
  } else {
    pBUF_DSCR->COUNT_RX =   USB_MAX_PACKET0 << 9;
  }
  EPxREG(0) = EP_CONTROL | EP_RX_VALID;

  DADDR = DADDR_EF | 0;                     /* Enable USB Default Address */
}


/*
 *  USB Suspend Function
 *   Called automatically on USB Suspend
 *    Return Value:    None
 */

void USB_Suspend (void) {
  CNTR |= CNTR_FSUSP;                       /* Force Suspend */
  CNTR |= CNTR_LPMODE;                      /* Low Power Mode */
}


/*
 *  USB Resume Function
 *   Called automatically on USB Resume
 *    Return Value:    None
 */

void USB_Resume (void) {
  /* Performed by Hardware */
}


/*
 *  USB Remote Wakeup Function
 *   Called automatically on USB Remote Wakeup
 *    Return Value:    None
 */

void USB_WakeUp (void) {
  CNTR &= ~CNTR_FSUSP;                      /* Clear Suspend */
}


/*
 *  USB Remote Wakeup Configuration Function
 *    Parameters:      cfg:   Enable/Disable
 *    Return Value:    None
 */

void USB_WakeUpCfg (uint32_t cfg) {
  /* Not needed */
}


/*
 *  USB Set Address Function
 *    Parameters:      adr:   USB Address
 *    Return Value:    None
 */

void USB_SetAddress (uint32_t adr) {
  DADDR = DADDR_EF | adr;
}


/*
 *  USB Configure Function
 *    Parameters:      cfg:   Configure/Deconfigure
 *    Return Value:    None
 */

void USB_Configure (uint32_t cfg) {
  cfg = cfg;
}


/*
 *  Configure USB Endpoint according to Descriptor
 *    Parameters:      pEPD:  Pointer to Endpoint Descriptor
 *    Return Value:    None
 */

void USB_ConfigEP (USB_ENDPOINT_DESCRIPTOR *pEPD) {
/* Double Buffering is not yet supported              */
  uint32_t num, val;

  num = pEPD->bEndpointAddress & 0x0F;

  val = pEPD->wMaxPacketSize;
  if (pEPD->bEndpointAddress & USB_ENDPOINT_DIRECTION_MASK) {
    (pBUF_DSCR + num)->ADDR_TX = FreeBufAddr;
    val = (val + 1) & ~1;
  } else {
    (pBUF_DSCR + num)->ADDR_RX = FreeBufAddr;
    if (val > 62) {
      val = (val + 31) & ~31;
      (pBUF_DSCR + num)->COUNT_RX = ((val << 5) - 1) | 0x8000;
    } else {
      val = (val + 1)  & ~1;
      (pBUF_DSCR + num)->COUNT_RX =   val << 9;
    }
  }
  FreeBufAddr += val;

  switch (pEPD->bmAttributes & USB_ENDPOINT_TYPE_MASK) {
    case USB_ENDPOINT_TYPE_CONTROL:
      val = EP_CONTROL;
      break;
    case USB_ENDPOINT_TYPE_ISOCHRONOUS:
      val = EP_ISOCHRONOUS;
      break;
    case USB_ENDPOINT_TYPE_BULK:
      val = EP_BULK;
      if (USB_DBL_BUF_EP & (1 << num)) {
        val |= EP_KIND;
      }
      break;
    case USB_ENDPOINT_TYPE_INTERRUPT:
      val = EP_INTERRUPT;
      break;
  }
  val |= num;
  EPxREG(num) = val;
}


/*
 *  Set Direction for USB Control Endpoint
 *    Parameters:      dir:   Out (dir == 0), In (dir <> 0)
 *    Return Value:    None
 */

void USB_DirCtrlEP (uint32_t dir) {
  /* Not needed */
}


/*
 *  Enable USB Endpoint
 *    Parameters:      EPNum: Endpoint Number
 *                       EPNum.0..3: Address
 *                       EPNum.7:    Dir
 *    Return Value:    None
 */

void USB_EnableEP (uint32_t EPNum) {
  EP_Status(EPNum, EP_TX_VALID | EP_RX_VALID);
}


/*
 *  Disable USB Endpoint
 *    Parameters:      EPNum: Endpoint Number
 *                       EPNum.0..3: Address
 *                       EPNum.7:    Dir
 *    Return Value:    None
 */

void USB_DisableEP (uint32_t EPNum) {
  EP_Status(EPNum, EP_TX_DIS | EP_RX_DIS);
}


/*
 *  Reset USB Endpoint
 *    Parameters:      EPNum: Endpoint Number
 *                       EPNum.0..3: Address
 *                       EPNum.7:    Dir
 *    Return Value:    None
 */

void USB_ResetEP (uint32_t EPNum) {
  EP_Reset(EPNum);
}


/*
 *  Set Stall for USB Endpoint
 *    Parameters:      EPNum: Endpoint Number
 *                       EPNum.0..3: Address
 *                       EPNum.7:    Dir
 *    Return Value:    None
 */

void USB_SetStallEP (uint32_t EPNum) {
  EP_Status(EPNum, EP_TX_STALL | EP_RX_STALL);
}


/*
 *  Clear Stall for USB Endpoint
 *    Parameters:      EPNum: Endpoint Number
 *                       EPNum.0..3: Address
 *                       EPNum.7:    Dir
 *    Return Value:    None
 */

void USB_ClrStallEP (uint32_t EPNum) {
  EP_Status(EPNum, EP_TX_VALID | EP_RX_VALID);
}


/*
 *  Read USB Endpoint Data
 *    Parameters:      EPNum: Endpoint Number
 *                       EPNum.0..3: Address
 *                       EPNum.7:    Dir
 *                     pData: Pointer to Data Buffer
 *    Return Value:    Number of bytes read
 */

uint32_t USB_ReadEP (uint32_t EPNum, uint8_t *pData) {
/* Double Buffering is not yet supported              */
  uint32_t num, cnt, *pv, n;

  num = EPNum & 0x0F;

  pv  = (uint32_t *)(USB_PMA_ADDR + 2*((pBUF_DSCR + num)->ADDR_RX));
  cnt = (pBUF_DSCR + num)->COUNT_RX & EP_COUNT_MASK;
  for (n = 0; n < (cnt + 1) / 2; n++) {
    *((__packed uint16_t *)pData) = *pv++;
    pData += 2;
  }
  EP_Status(EPNum, EP_RX_VALID);


  return (cnt);
}


/*
 *  Write USB Endpoint Data
 *    Parameters:      EPNum: Endpoint Number
 *                       EPNum.0..3: Address
 *                       EPNum.7:    Dir
 *                     pData: Pointer to Data Buffer
 *                     cnt:   Number of bytes to write
 *    Return Value:    Number of bytes written
 */

uint32_t USB_WriteEP (uint32_t EPNum, uint8_t *pData, uint32_t cnt) {
/* Double Buffering is not yet supported              */
  uint32_t num, *pv, n;

  num = EPNum & 0x0F;

  pv  = (uint32_t *)(USB_PMA_ADDR + 2*((pBUF_DSCR + num)->ADDR_TX));
  for (n = 0; n < (cnt + 1) / 2; n++) {
    *pv++ = *((__packed uint16_t *)pData);
    pData += 2;
  }
  (pBUF_DSCR + num)->COUNT_TX = cnt;
  EP_Status(EPNum, EP_TX_VALID);

  return (cnt);
}


/*
 *  Get USB Last Frame Number
 *    Parameters:      None
 *    Return Value:    Frame Number
 */

uint32_t USB_GetFrame (void) {
  return (FNR & FNR_FN);
}


/*
 *  USB Interrupt Service Routine
 */

void USB_LP_CAN1_RX0_IRQHandler (void) {
  uint32_t istr, num, val;

  istr = ISTR;

  /* USB Reset Request */
  if (istr & ISTR_RESET) {
    USB_Reset();
#if USB_RESET_EVENT
    USB_Reset_Event();
#endif
    ISTR = ~ISTR_RESET;
  }

  /* USB Suspend Request */
  if (istr & ISTR_SUSP) {
    USB_Suspend();
#if USB_SUSPEND_EVENT
    USB_Suspend_Event();
#endif
    ISTR = ~ISTR_SUSP;
  }

  /* USB Wakeup */
  if (istr & ISTR_WKUP) {
    USB_WakeUp();
#if USB_WAKEUP_EVENT
    USB_WakeUp_Event();
#endif
    ISTR = ~ISTR_WKUP;
  }

  /* Start of Frame */
  if (istr & ISTR_SOF) {
#if USB_SOF_EVENT
    USB_SOF_Event();
#endif
    ISTR = ~ISTR_SOF;
  }

#if USB_ERROR_EVENT

  /* PMA Over/underrun */
  if (istr & ISTR_PMAOVR) {
    USB_Error_Event(1);
    ISTR = ~ISTR_PMAOVR;
  }

  /* Error: No Answer, CRC Error, Bit Stuff Error, Frame Format Error */
  if (istr & ISTR_ERR) {
    USB_Error_Event(0);
    ISTR = ~ISTR_ERR;
  }

#endif

  /* Endpoint Interrupts */
  while ((istr = ISTR) & ISTR_CTR) {
    ISTR = ~ISTR_CTR;

    num = istr & ISTR_EP_ID;

    val = EPxREG(num);
    if (val & EP_CTR_RX) {
      EPxREG(num) = val & ~EP_CTR_RX & EP_MASK;
      if (USB_P_EP[num]) {
        if (val & EP_SETUP) {
          USB_P_EP[num](USB_EVT_SETUP);
        } else {
          USB_P_EP[num](USB_EVT_OUT);
        }
      }
    }
    if (val & EP_CTR_TX) {
      EPxREG(num) = val & ~EP_CTR_TX & EP_MASK;
      if (USB_P_EP[num]) {
        USB_P_EP[num](USB_EVT_IN);
      }
    }
  }

}
