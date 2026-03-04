#include "qemu/osdep.h"
#include "hw/dma/stm32f2xx_dma.h"
#include "hw/core/irq.h"
#include "qemu/log.h"
#include "qemu/module.h"
#include "system/dma.h"


/* Bit-shift of stream's interrupt flags */
#define dma_flags_shift(stream) (((stream) & 2) << 3) | (((stream) & 1) * 6)

static uint32_t *dma_isr_reg(STM32F2XXDMAState *s, int stream)
{
    return (stream < 4) ? &s->lisr : &s->hisr;
}

static void stm32f2xx_dma_update_irq(STM32F2XXDMAState *s, int stream)
{
    uint32_t *isr = dma_isr_reg(s, stream);
    int shift = dma_flags_shift(stream);
    uint32_t flags = (*isr >> shift) & 0x3F;
    uint32_t cr = s->stream[stream].cr;

    bool raise = 0;
    if ((cr & DMA_SCR_TCIE) && (flags & DMA_ISR_TCIF)) {
        raise = 1;
    }
    if ((cr & DMA_SCR_HTIE) && (flags & DMA_ISR_HTIF)) {
        raise = 1;
    }
    if ((cr & DMA_SCR_TEIE) && (flags & DMA_ISR_TEIF)) {
        raise = 1;
    }
    if ((cr & DMA_SCR_DMEIE) && (flags & DMA_ISR_DMEIF)) {
        raise = 1;
    }

    qemu_set_irq(s->irq[stream], raise ? 1 : 0);
}

/*
 * Perform an immediate DMA transfer for the given stream.
 * P2M: peripheral (PAR) to memory (M0AR)
 * M2M: memory (M0AR) to peripheral (PAR)
 * M2M: memory (PAR) to memory (M0AR)
 */
static void stm32f2xx_dma_do_transfer(STM32F2XXDMAState *s, int stream)
{
    STM32F2XXDMAStream *st = &s->stream[stream];
    uint32_t dir = st->cr & DMA_SCR_DIR_MASK;
    uint32_t ndtr = st->ndtr;
    uint32_t par = st->par;
    uint32_t m0ar = st->m0ar;
    bool minc = (st->cr & DMA_SCR_MINC);
    bool pinc = (st->cr & DMA_SCR_PINC);

    if (!ndtr) {
        return;
    }

    uint8_t buf;
    for (uint32_t i = 0; i < ndtr; i++) {
        if (dir == DMA_SCR_DIR_M2P) {
            dma_memory_read(&address_space_memory, m0ar, &buf, 1,
                           MEMTXATTRS_UNSPECIFIED);
            dma_memory_write(&address_space_memory, par, &buf, 1,
                            MEMTXATTRS_UNSPECIFIED);
        } else if (dir == DMA_SCR_DIR_P2M) {
            dma_memory_read(&address_space_memory, par, &buf, 1,
                           MEMTXATTRS_UNSPECIFIED);
            dma_memory_write(&address_space_memory, m0ar, &buf, 1,
                            MEMTXATTRS_UNSPECIFIED);
        } else {
            dma_memory_read(&address_space_memory, par, &buf, 1,
                           MEMTXATTRS_UNSPECIFIED);
            dma_memory_write(&address_space_memory, m0ar, &buf, 1,
                            MEMTXATTRS_UNSPECIFIED);
        }

        if (minc) {
            m0ar++;
        }
        if (pinc) {
            par++;
        }
    }

    uint32_t *isr = dma_isr_reg(s, stream);
    int shift = dma_flags_shift(stream);
    *isr |= (DMA_ISR_TCIF << shift);

    st->cr &= ~DMA_SCR_EN;
    st->ndtr = 0;

    stm32f2xx_dma_update_irq(s, stream);
}

bool stm32f2xx_dma_receive_byte(STM32F2XXDMAState *s, hwaddr periph_addr,
                                uint8_t byte)
{
    for (int i = 0; i < STM32_DMA_NUM_STREAMS; i++) {
        STM32F2XXDMAStream *st = &s->stream[i];

        if (!(st->cr & DMA_SCR_EN) 
            || (st->cr & DMA_SCR_DIR_MASK) != DMA_SCR_DIR_P2M
            || st->par != (uint32_t)periph_addr
            || !st->ndtr
        ) {
            continue;
        }

        uint32_t buf_base;
        if (st->cr & DMA_SCR_DBM) {
            buf_base = (st->cr & DMA_SCR_CT) ? st->m1ar : st->m0ar;
        } else {
            buf_base = st->m0ar;
        }
        uint32_t offset = st->initial_ndtr - st->ndtr;
        uint32_t write_addr = buf_base + offset;

        dma_memory_write(&address_space_memory, write_addr, &byte, 1,
                         MEMTXATTRS_UNSPECIFIED);

        st->ndtr--;

        if (st->ndtr == 0) {
            uint32_t *isr = dma_isr_reg(s, i);
            int shift = dma_flags_shift(i);
            *isr |= (DMA_ISR_TCIF << shift);

            if (st->cr & DMA_SCR_DBM) {
                st->cr ^= DMA_SCR_CT;
                st->ndtr = st->initial_ndtr;
            } else if (st->cr & DMA_SCR_CIRC) {
                st->ndtr = st->initial_ndtr;
            } else {
                st->cr &= ~DMA_SCR_EN;
            }

            stm32f2xx_dma_update_irq(s, i);
        }

        return true;
    }
    return false;
}

static uint64_t stm32f2xx_dma_read(void *opaque, hwaddr addr, unsigned int size)
{
    STM32F2XXDMAState *s = opaque;

    switch (addr) {
    case DMA_LISR:
        return s->lisr;
    case DMA_HISR:
        return s->hisr;
    case DMA_LIFCR:
    case DMA_HIFCR:
        return 0;
    default:
        break;
    }

    if (addr >= 0x10 && addr < 0x10 + 0x18 * STM32_DMA_NUM_STREAMS) {
        int stream = (addr - 0x10) / 0x18;
        int offset = (addr - 0x10) % 0x18;
        STM32F2XXDMAStream *st = &s->stream[stream];

        switch (offset) {
        case DMA_STREAM_CR:
            return st->cr;
        case DMA_STREAM_NDTR:
            return st->ndtr;
        case DMA_STREAM_PAR:
            return st->par;
        case DMA_STREAM_M0AR:
            return st->m0ar;
        case DMA_STREAM_M1AR:
            return st->m1ar;
        case DMA_STREAM_FCR:
            return st->fcr;
        default:
            break;
        }
    }

    qemu_log_mask(LOG_GUEST_ERROR,
                  "stm32f2xx_dma: bad read offset 0x%" HWADDR_PRIx "\n", addr);
    return 0;
}

static void stm32f2xx_dma_write(void *opaque, hwaddr addr,
                                uint64_t val64, unsigned int size)
{
    STM32F2XXDMAState *s = opaque;
    uint32_t value = val64;

    switch (addr) {
    case DMA_LISR:
    case DMA_HISR:
        return;
    case DMA_LIFCR:
        s->lisr &= ~value;
        for (int i = 0; i < 4; i++) {
            stm32f2xx_dma_update_irq(s, i);
        }
        return;
    case DMA_HIFCR:
        s->hisr &= ~value;
        for (int i = 4; i < 8; i++) {
            stm32f2xx_dma_update_irq(s, i);
        }
        return;
    default:
        break;
    }

    if (addr >= 0x10 && addr < 0x10 + 0x18 * STM32_DMA_NUM_STREAMS) {
        int stream = (addr - 0x10) / 0x18;
        int offset = (addr - 0x10) % 0x18;
        STM32F2XXDMAStream *st = &s->stream[stream];

        switch (offset) {
        case DMA_STREAM_CR:
        {
            bool was_enabled = (st->cr & DMA_SCR_EN);
            st->cr = value;
            if (was_enabled || !(value & DMA_SCR_EN)) {
                return;
            }
            st->initial_ndtr = st->ndtr;
            if ((value & DMA_SCR_CIRC) || (value & DMA_SCR_DBM)) {
                return;
            }
            stm32f2xx_dma_do_transfer(s, stream);
            return;
        }
        case DMA_STREAM_NDTR:
            st->ndtr = value & 0xFFFF;
            return;
        case DMA_STREAM_PAR:
            st->par = value;
            return;
        case DMA_STREAM_M0AR:
            st->m0ar = value;
            return;
        case DMA_STREAM_M1AR:
            st->m1ar = value;
            return;
        case DMA_STREAM_FCR:
            st->fcr = value;
            return;
        default:
            break;
        }
    }

    qemu_log_mask(LOG_GUEST_ERROR,
                  "stm32f2xx_dma: bad write offset 0x%" HWADDR_PRIx "\n", addr);
}

static const MemoryRegionOps stm32f2xx_dma_ops = {
    .read = stm32f2xx_dma_read,
    .write = stm32f2xx_dma_write,
    .endianness = DEVICE_NATIVE_ENDIAN,
};

static void stm32f2xx_dma_reset(DeviceState *dev)
{
    STM32F2XXDMAState *s = STM32F2XX_DMA(dev);

    s->lisr = 0;
    s->hisr = 0;

    for (int i = 0; i < STM32_DMA_NUM_STREAMS; i++) {
        memset(&s->stream[i], 0, sizeof(STM32F2XXDMAStream));
        s->stream[i].fcr = 0x21;
    }
}

static void stm32f2xx_dma_init(Object *obj)
{
    STM32F2XXDMAState *s = STM32F2XX_DMA(obj);

    memory_region_init_io(&s->mmio, obj, &stm32f2xx_dma_ops, s,
                          TYPE_STM32F2XX_DMA, 0x400);
    sysbus_init_mmio(SYS_BUS_DEVICE(obj), &s->mmio);

    for (int i = 0; i < STM32_DMA_NUM_STREAMS; i++) {
        sysbus_init_irq(SYS_BUS_DEVICE(obj), &s->irq[i]);
    }
}

static void stm32f2xx_dma_class_init(ObjectClass *klass, const void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);
    device_class_set_legacy_reset(dc, stm32f2xx_dma_reset);
}

static const TypeInfo stm32f2xx_dma_info = {
    .name          = TYPE_STM32F2XX_DMA,
    .parent        = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(STM32F2XXDMAState),
    .instance_init = stm32f2xx_dma_init,
    .class_init    = stm32f2xx_dma_class_init,
};

static void stm32f2xx_dma_register_types(void)
{
    type_register_static(&stm32f2xx_dma_info);
}

type_init(stm32f2xx_dma_register_types)
