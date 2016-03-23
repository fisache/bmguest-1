#include "gic-v2.h"
#include <arch/gic_regs.h>  // Define OFFSETS
#include <arch/armv7.h>     // get_periphbase()
#include <io.h>
#include <core/vm/vcpu.h>
#include <core/scheduler.h>
#include <stdio.h>
#include "../include/asm/macro.h"        // SECTION(x)

#include "irq-chip.h"

#include <arch/irq.h>

#define HW_IRQ      1
#define SW_IRQ      0

#define EOI_ENABLE      1
#define EOI_DISABLE     0

#define HIGHEST_PRIORITY    0

#define VIRQ_MAX_ENTRIES                128
#define VGIC_MAINTENANCE_INTERRUPT_IRQ  25
#define VGIC_MAX_LISTREGISTERS          VGIC_NUM_MAX_SLOTS
#define VGIC_SLOT_NOTFOUND              (0xFFFFFFFF)

void gic_init(void)
{
    int i;
    uint32_t periphbase;

    // This code will be moved other parts, not here. */
    /* Get GICv2 base address */
    periphbase = (uint32_t)(get_periphbase() & 0x000000FFFFFFFFFFULL);
    if (periphbase == 0x0) {
        printf("Warning: Curretn architecture has no value in CBAR\n    \
                The architecture do not follow design philosophy from ARM recommendation\n");
        // TODO(casionwoo): vaule of base address will be read from DTB or configuration file.
        // Currently, we set the base address of gic to 0x2C000000, it is for RTSM.
        periphbase = 0x2C000000;
    }
    GICv2.gicd = periphbase + GICD_OFFSET;
    GICv2.gicc = periphbase + GICC_OFFSET;
    GICv2.gich = periphbase + GICH_OFFSET;

    /*
     * We usually use the name of variables in lower case, but
     * here, using upper case is special case for readability.
     */
    uint32_t gicd_typer = GICD_READ(GICD_TYPER_OFFSET);
    /* maximum number of irq lines: 32(N+1). */
    GICv2.ITLinesNumber = 32 * ((gicd_typer & GICD_NR_IT_LINES_MASK) + 1);
    GICv2.CPUNumber = 1 + (gicd_typer & GICD_NR_CPUS_MASK);
    printf("Number of IRQs: %d\n", GICv2.ITLinesNumber);
    printf("Number of CPU interfaces: %d\n", GICv2.CPUNumber);

    GICC_WRITE(GICC_CTLR_OFFSET, GICC_CTL_ENABLE | GICC_CTL_EOI);
    GICD_WRITE(GICD_CTLR_OFFSET, 0x1);

    /* No Priority Masking: the lowest value as the threshold : 255 */
    // We set 0xff but, real value is 0xf8
    GICC_WRITE(GICC_PMR_OFFSET, 0xff);

    // Set interrupt configuration do not work.
    for (i = 32; i < GICv2.ITLinesNumber; i += 16) {
        GICD_WRITE(GICD_ICFGR(i >> 4), 0x0);
    }

    /* Disable all global interrupts. */
    for (i = 0; i < GICv2.ITLinesNumber; i += 32) {
        GICD_WRITE(GICD_ISENABLER(i >> 5), 0xffffffff);
        uint32_t valid = GICD_READ(GICD_ISENABLER(i >> 5));
        GICD_WRITE(GICD_ICENABLER(i >> 5), valid);
    }

    // We set priority 0xa0 for each but real value is a 0xd0, Why?
    /* Set priority as default for all interrupts */
    for (i = 0; i < GICv2.ITLinesNumber; i += 4) {
        GICD_WRITE(GICD_IPRIORITYR(i >> 2), 0xa0a0a0a0);
    }

#ifndef __CONFIG_SMP__
    // NOTE: GIC_ITRAGETSR is read-only on multiprocessor environment.
    for (i = 32; i < GICv2.ITLinesNumber; i += 4) {
        GICD_WRITE(GICD_ITARGETSR(i >> 2), 1 << 0 | 1 << 8 | 1 << 16 | 1 << 24);
    }
#endif


}

void gic_configure_irq(uint32_t irq, uint8_t polarity)
{
    if (irq < GICv2.ITLinesNumber) {
        uint32_t reg;
        uint32_t mask;

        reg = GICD_READ(GICD_ICFGR(irq >> 4));

        mask = (reg >> 2 * (irq % 16)) & 0x3;
        if (polarity == IRQ_LEVEL_TRIGGERED) {
            mask &= 0;
            mask |= IRQ_LEVEL_TRIGGERED << 0;
        } else { /* IRQ_EDGE_TRIGGERED */
            mask &= 0;
            mask |= IRQ_EDGE_TRIGGERED << 0;
        }
        reg = reg & ~(0x3 << 2 * (irq % 16));
        reg = reg | (mask << 2 * (irq % 16));

        GICD_WRITE(GICD_ICFGR(irq >> 4), reg);
    } else {
        printf("invalid or spurious irq: %d\n", irq);
    }
}

void gic_set_sgi(const uint32_t target, uint32_t sgi)
{
    if (!(sgi < 16)) {
        return ;
    }

    GICD_WRITE(GICD_SGIR, GICD_SGIR_TARGET_LIST |
               (target << GICD_SGIR_CPU_TARGET_LIST_OFFSET) |
               (sgi & GICD_SGIR_SGI_INT_ID_MASK));
}

uint32_t gic_get_irq_number(void)
{
    return (GICC_READ(GICC_IAR_OFFSET) & GICC_IAR_MASK);
}

void gic_enable_irq(uint32_t irq)
{
    GICD_WRITE(GICD_ISENABLER(irq >> 5), 1UL << (irq & 0x1F));
}

void gic_disable_irq(uint32_t irq)
{
    GICD_WRITE(GICD_ICENABLER(irq >> 5), 1UL << (irq & 0x1F));
}

void gic_completion_irq(uint32_t irq)
{
    GICC_WRITE(GICC_EOIR_OFFSET, irq);
}

void gic_deactivate_irq(uint32_t irq)
{
    GICC_WRITE(GICC_DIR_OFFSET, irq);
}
