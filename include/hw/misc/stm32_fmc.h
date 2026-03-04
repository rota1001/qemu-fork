#ifndef HW_STM32_FMC_H
#define HW_STM32_FMC_H

#include "hw/core/sysbus.h"
#include "qom/object.h"

#define TYPE_STM32_FMC "stm32.fmc"
OBJECT_DECLARE_SIMPLE_TYPE(STM32FMCState, STM32_FMC)

#define STM32_FMC_PERIPHERAL_SIZE 0x400
#define STM32_FMC_NREGS (STM32_FMC_PERIPHERAL_SIZE / 4)

struct STM32FMCState {
    SysBusDevice parent_obj;

    MemoryRegion mmio;
    uint32_t regs[STM32_FMC_NREGS];
};

#endif /* HW_STM32_FMC_H */
