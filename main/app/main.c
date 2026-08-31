#include "diff_drive.h"
#include "battery.h"
#include "board_led.h"
#include "rp3_receiver.h"
#include "usb_bridge.h"
#include "ros2_msgs.h"
#include "shell_uart.h"
#include "diag.h"
#include "flash_storage.h"

void app_main(void)
{
    diag_init();

    motor_init();

    flash_storage_init();

    battery_init();

    led_init();

    rp3_receiver_init();

    ros2_msgs_init();

    shell_uart_init();
}
