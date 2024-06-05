#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <zephyr/usb/usb_device.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/console/console.h>
#include <string.h>

/* The devicetree node identifier for the "led0" alias. */
#define LED0_NODE DT_ALIAS(led0)

/* Check overlay exists for CDC UART console */ 
BUILD_ASSERT(DT_NODE_HAS_COMPAT(DT_CHOSEN(zephyr_console), zephyr_cdc_acm_uart),
	    "Console device is not ACM CDC UART device");

K_THREAD_STACK_DEFINE(blink_thread_stack, 512);
static struct k_thread blink_thread_data;
k_tid_t thread_blink;

/*
* A build error on this line means your board is unsupported.
* See the sample documentation for information on how to fix this.
*/
static const struct gpio_dt_spec led = GPIO_DT_SPEC_GET(LED0_NODE, gpios);

/* Configure to set Console output to USB Serial */ 
static const struct device *usb_device = DEVICE_DT_GET(DT_CHOSEN(zephyr_console));

void blink_thread(void *p1, void *p2, void *p3){
	while (1) {
		gpio_pin_toggle_dt(&led);
		k_sleep(K_MSEC(500));
	}
}

int main(void)
{	
	console_getline_init();

	gpio_pin_configure_dt(&led, GPIO_OUTPUT);

	uint32_t dtr = 0;

    /* Check if USB can be initialised, bails out if fail is returned */
	if (usb_enable(NULL) != 0) {
		return 0;
	}

	/* Wait for a console connection, if the DTR flag was set to activate USB.
     * If you wish to start generating serial data immediately, you can simply
     * remove the while loop, to not wait until the control line is set.    
     */
	while (!dtr) {
		uart_line_ctrl_get(usb_device, UART_LINE_CTRL_DTR, &dtr);
		k_sleep(K_MSEC(100));
	}

	while (1) {
		printk("-------------------------------------------------\n");
		printk("Hello from the Zephyr Console on the RPi Pico !\n");
		printk("Give me an instruction !\n");

		char *s = console_getline();

		if (strcmp(s, "on") == 0 || strcmp(s, "ON") == 0) {
			gpio_pin_set_dt(&led, 1);
			printk("LED ON !\n");
		} else if (strcmp(s, "off") == 0 || strcmp(s, "OFF") == 0) {
			gpio_pin_set_dt(&led, 0);
			printk("LED OFF !\n");
		} else if (strcmp(s, "blink") == 0) {
			printk("LED toggle ! Write \"stop\" if you want to stop blink\n");

			if(thread_blink == NULL){
				thread_blink = k_thread_create(&blink_thread_data, blink_thread_stack, K_THREAD_STACK_SIZEOF(blink_thread_stack),
						blink_thread, NULL, NULL, NULL, K_PRIO_COOP(1), 0, K_NO_WAIT);
			}

			retour:
			s = console_getline();

			if(strcmp(s, "stop") == 0){
				k_thread_abort(thread_blink);
				gpio_pin_set_dt(&led, 0);
				thread_blink = NULL;
			}else{
				printk("Wrong instruction\n");
				goto retour;
			}
		} else {
			printk("Instruction not supported !\n");
		}
	}

	return 0;
}