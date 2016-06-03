#include "platform.h"
#include <stdint.h>
#include <serial.h>
#include <size.h>
#include <arch/gic_regs.h>

#include <arch/armv7.h>

void console_init()
{
    // TODO(wonseok): add general initialization for console devices.
    serial_init(115200, 24000000);
}
