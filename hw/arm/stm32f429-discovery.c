#include "qemu/osdep.h"
#include "qapi/error.h"
#include "hw/core/boards.h"
#include "hw/core/qdev-properties.h"
#include "hw/core/qdev-clock.h"
#include "hw/core/loader.h"
#include "qemu/error-report.h"
#include "hw/arm/stm32f405_soc.h"
#include "hw/arm/boot.h"
#include "hw/arm/machines-qom.h"
#include "hw/misc/unimp.h"
#include "system/address-spaces.h"
#include "system/system.h"

#define SYSCLK_FREQ 180000000
#define EXTRA_FLASH_BASE 0x08100000
#define EXTRA_FLASH_SIZE (1024 * 1024)
#define FMC_SDRAM_BASE 0x90000000
#define FMC_SDRAM_SIZE (8 * 1024 * 1024)

static void stm32f429discovery_init(MachineState *machine)
{
    DeviceState *dev;
    Clock *sysclk;

    sysclk = clock_new(OBJECT(machine), "SYSCLK");
    clock_set_hz(sysclk, SYSCLK_FREQ);

    dev = qdev_new(TYPE_STM32F405_SOC);
    object_property_add_child(OBJECT(machine), "soc", OBJECT(dev));
    qdev_connect_clock_in(dev, "sysclk", sysclk);
    sysbus_realize_and_unref(SYS_BUS_DEVICE(dev), &error_fatal);

    MemoryRegion *mem = g_new(MemoryRegion, 1);
    memory_region_init_rom(mem, NULL, "stm32f429.flash_bank2",
                           EXTRA_FLASH_SIZE, &error_fatal);
    memory_region_add_subregion(get_system_memory(), EXTRA_FLASH_BASE, mem);
    
    mem = g_new(MemoryRegion, 1);
    memory_region_init_ram(mem, NULL, "stm32f429.fmc_ram",
                           FMC_SDRAM_SIZE, &error_fatal);
    memory_region_add_subregion(get_system_memory(), FMC_SDRAM_BASE, mem);

    create_unimplemented_device("GPIOJ", 0x40022400, 0x400);
    create_unimplemented_device("GPIOK", 0x40022800, 0x400);

    armv7m_load_kernel(STM32F405_SOC(dev)->armv7m.cpu,
                       machine->kernel_filename,
                       0, FLASH_SIZE);
}

static void stm32f429discovery_machine_init(MachineClass *mc)
{
    static const char * const valid_cpu_types[] = {
        ARM_CPU_TYPE_NAME("cortex-m4"),
        NULL
    };

    mc->desc = "ST STM32F429DISCOVERY (Cortex-M4)";
    mc->init = stm32f429discovery_init;
    mc->valid_cpu_types = valid_cpu_types;
}

DEFINE_MACHINE_ARM("stm32f429discovery", stm32f429discovery_machine_init)
