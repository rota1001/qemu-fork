#include "qemu/osdep.h"
#include "qemu/log.h"
#include "hw/misc/stm32_fmc.h"
#include "migration/vmstate.h"

#define FMC_SDCMR  (0x150 / 4)
#define FMC_SDSR   (0x158 / 4)

static void stm32_fmc_reset(DeviceState *dev)
{
    STM32FMCState *s = STM32_FMC(dev);

    for (int i = 0; i < STM32_FMC_NREGS; i++) {
        s->regs[i] = 0;
    }
    qemu_log_mask( LOG_UNIMP, "%s\n", __func__);
    s->regs[FMC_SDSR] &= ~(1 << 5);
}

static uint64_t stm32_fmc_read(void *opaque, hwaddr addr, unsigned int size)
{
    STM32FMCState *s = STM32_FMC(opaque);
    uint32_t index = addr >> 2;

    qemu_log_mask( LOG_UNIMP, "%s\n", __func__);
    return s->regs[index];
}

static void stm32_fmc_write(void *opaque, hwaddr addr,
                            uint64_t val64, unsigned int size)
{
    STM32FMCState *s = STM32_FMC(opaque);
    uint32_t index = addr >> 2;
    uint32_t value = val64;

    s->regs[index] = value;
    s->regs[FMC_SDSR] &= ~(1 << 5);
    qemu_log_mask( LOG_UNIMP, "%s\n", __func__);
}

static const MemoryRegionOps stm32_fmc_ops = {
    .read = stm32_fmc_read,
    .write = stm32_fmc_write,
    .endianness = DEVICE_NATIVE_ENDIAN,
    .valid = {
        .min_access_size = 4,
        .max_access_size = 4,
    },
};

static void stm32_fmc_init(Object *obj)
{
    STM32FMCState *s = STM32_FMC(obj);

    memory_region_init_io(&s->mmio, obj, &stm32_fmc_ops, s,
                          TYPE_STM32_FMC, STM32_FMC_PERIPHERAL_SIZE);
    sysbus_init_mmio(SYS_BUS_DEVICE(obj), &s->mmio);
}

static const VMStateDescription vmstate_stm32_fmc = {
    .name = TYPE_STM32_FMC,
    .version_id = 1,
    .minimum_version_id = 1,
    .fields = (const VMStateField[]) {
        VMSTATE_UINT32_ARRAY(regs, STM32FMCState, STM32_FMC_NREGS),
        VMSTATE_END_OF_LIST()
    }
};

static void stm32_fmc_class_init(ObjectClass *klass, const void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);

    dc->vmsd = &vmstate_stm32_fmc;
    device_class_set_legacy_reset(dc, stm32_fmc_reset);
}

static const TypeInfo stm32_fmc_info = {
    .name          = TYPE_STM32_FMC,
    .parent        = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(STM32FMCState),
    .instance_init = stm32_fmc_init,
    .class_init    = stm32_fmc_class_init,
};

static void stm32_fmc_register_types(void)
{
    type_register_static(&stm32_fmc_info);
}

type_init(stm32_fmc_register_types)
