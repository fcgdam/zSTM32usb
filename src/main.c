/*
 * STM32 Blue Pill Zephyr sample code
 *
 * PrimalCortex -> 2020
*/
#include <zephyr.h>
#include <device.h>
#include <devicetree.h>
#include <drivers/gpio.h>

#include <sys/printk.h>
#include <sys/util.h>
#include <string.h>
#include <usb/usb_device.h>
#include <drivers/uart.h>

/* 1000 msec = 1 sec */
#define SLEEP_TIME_MS   500

/* size of stack area used by each thread */
#define STACKSIZE 1024

/* scheduling priority used by each thread */
#define PRIORITY 7

#define LED0_NODE DT_ALIAS(led0)

struct led {
    const    char *gpio_dev_name;
    const    char *gpio_pin_name;
    unsigned int  gpio_pin;
    unsigned int  gpio_flags;
};

/*
 * Blinks a specific LED. This code is reuseable if we want to blink multiple LEDs
*/

void blink(const struct led *led, uint32_t id) {
    const struct device *gpio_dev;
    int ret;
    bool led_is_on = true;

    gpio_dev = device_get_binding(led->gpio_dev_name);
    if (gpio_dev == NULL) {
        printk("Error: didn't find %s device\n",
        led->gpio_dev_name);
        return;
    }

    ret = gpio_pin_configure(gpio_dev, led->gpio_pin, led->gpio_flags);
    if (ret != 0) {
        printk("Error %d: failed to configure pin %d '%s'\n",
        ret, led->gpio_pin, led->gpio_pin_name);
    return;
    }

    // Blink for ever.
	while (1) {
		gpio_pin_set(gpio_dev, led->gpio_pin, (int)led_is_on);
		led_is_on = !led_is_on;
		k_msleep(SLEEP_TIME_MS);    // We should sleep, otherwise the task won't release the cpu for other tasks!
	}
}

/*
 * Task for blinking led0, which is the blue pill onboard only led.
*/

void blink0(void) {
    const struct led led0 = {
    #if DT_NODE_HAS_STATUS(LED0_NODE, okay)
        .gpio_dev_name = DT_GPIO_LABEL(LED0_NODE, gpios),
        .gpio_pin_name = DT_LABEL(LED0_NODE),
        .gpio_pin = DT_GPIO_PIN(LED0_NODE, gpios),
        .gpio_flags = GPIO_OUTPUT | DT_GPIO_FLAGS(LED0_NODE, gpios),
    #else
        #error "Unsupported board: led0 devicetree alias is not defined"
    #endif
    };

    blink(&led0, 0);
}

/*
 * USB console task. 
 * Initializes the USB port, waits for a connection and outputs periodically a string to the USB virtual com port.
 *
*/

void usb_console_init(void) {
    const struct device *dev = device_get_binding(CONFIG_UART_CONSOLE_ON_DEV_NAME);
    uint32_t dtr = 0;

    if (usb_enable(NULL)) {
        return;
    }

    /* Poll if the DTR flag was set, optional */
    while (!dtr) {
        uart_line_ctrl_get(dev, UART_LINE_CTRL_DTR, &dtr);
        k_msleep(250);          // Let other tasks to run if no terminal is connected to USB
    }

    if (strlen(CONFIG_UART_CONSOLE_ON_DEV_NAME) != strlen("CDC_ACM_0") ||
        strncmp(CONFIG_UART_CONSOLE_ON_DEV_NAME, "CDC_ACM_0",   strlen(CONFIG_UART_CONSOLE_ON_DEV_NAME))) {
            printk("Error: Console device name is not USB ACM\n");

        return;
    }
    
    while ( 1 ) {
        printk("Hello from STM32 CDC Virtual COM port!\n\n");
        k_msleep( 2000 );
    }
}


// Task for handling blinking led.
K_THREAD_DEFINE(blink0_id, STACKSIZE, blink0, NULL, NULL, NULL, PRIORITY, 0, 0);    

// Task to initialize the USB CDC ACM virtual COM port used for outputing data.
// It's a separeted task since if nothing is connected to the USB port the task will hang...
K_THREAD_DEFINE(console_id, STACKSIZE, usb_console_init, NULL, NULL, NULL, PRIORITY, 0, 0);
