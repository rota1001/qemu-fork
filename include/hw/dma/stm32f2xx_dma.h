/*
 * STM32F2XX DMA controller
 *
 * Minimal model for STM32F4 DMA1/DMA2 controllers.
 * Performs immediate synchronous transfers when a stream is enabled.
 *
 * Copyright (c) 2026
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef HW_STM32F2XX_DMA_H
#define HW_STM32F2XX_DMA_H

#include "hw/core/sysbus.h"
#include "qom/object.h"

#define TYPE_STM32F2XX_DMA "stm32f2xx-dma"
OBJECT_DECLARE_SIMPLE_TYPE(STM32F2XXDMAState, STM32F2XX_DMA)

#define STM32_DMA_NUM_STREAMS 8

#define DMA_LISR    0x00
#define DMA_HISR    0x04
#define DMA_LIFCR   0x08
#define DMA_HIFCR   0x0C

#define DMA_STREAM_CR   0x00
#define DMA_STREAM_NDTR 0x04
#define DMA_STREAM_PAR  0x08
#define DMA_STREAM_M0AR 0x0C
#define DMA_STREAM_M1AR 0x10
#define DMA_STREAM_FCR  0x14


#define DMA_SxCR(x)    (0x10 + 0x18 * (x))
#define DMA_SxNDTR(x)  (0x14 + 0x18 * (x))
#define DMA_SxPAR(x)   (0x18 + 0x18 * (x))
#define DMA_SxM0AR(x)  (0x1C + 0x18 * (x))
#define DMA_SxM1AR(x)  (0x20 + 0x18 * (x))
#define DMA_SxFCR(x)   (0x24 + 0x18 * (x))


#define DMA_SCR_EN     BIT(0)
#define DMA_SCR_DMEIE  BIT(1)
#define DMA_SCR_TEIE   BIT(2)
#define DMA_SCR_HTIE   BIT(3)
#define DMA_SCR_TCIE   BIT(4)
#define DMA_SCR_PFCTRL BIT(5)
#define DMA_SCR_DIR_MASK  (3 << 6)
#define DMA_SCR_DIR_P2M   (0 << 6)
#define DMA_SCR_DIR_M2P   (1 << 6)
#define DMA_SCR_DIR_M2M   (2 << 6)
#define DMA_SCR_CIRC   BIT(8)
#define DMA_SCR_PINC   BIT(9)
#define DMA_SCR_MINC   BIT(10)
#define DMA_SCR_CT     BIT(19)
#define DMA_SCR_DBM    BIT(18)

#define DMA_ISR_FEIF   BIT(0)
#define DMA_ISR_DMEIF  BIT(2)
#define DMA_ISR_TEIF   BIT(3)
#define DMA_ISR_HTIF   BIT(4)
#define DMA_ISR_TCIF   BIT(5)

typedef struct STM32F2XXDMAStream {
    uint32_t cr;
    uint32_t ndtr;
    uint32_t par;
    uint32_t m0ar;
    uint32_t m1ar;
    uint32_t fcr;
    uint32_t initial_ndtr;
} STM32F2XXDMAStream;

struct STM32F2XXDMAState {
    SysBusDevice parent_obj;

    MemoryRegion mmio;

    uint32_t lisr;
    uint32_t hisr;

    STM32F2XXDMAStream stream[STM32_DMA_NUM_STREAMS];

    qemu_irq irq[STM32_DMA_NUM_STREAMS];
};


bool stm32f2xx_dma_receive_byte(STM32F2XXDMAState *s, hwaddr periph_addr,
                                uint8_t byte);

#endif
