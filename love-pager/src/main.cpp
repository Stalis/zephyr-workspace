#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/display.h>
#include <zephyr/display/cfb.h>
#include <zephyr/kernel.h>

#define USER_NODE DT_PATH(zephyr_user)
#define LED0_NODE DT_ALIAS(led0)

static const gpio_dt_spec led = GPIO_DT_SPEC_GET(LED0_NODE, gpios);

static const device* display =
        DEVICE_DT_GET(DT_CHOSEN(zephyr_display));

static const gpio_dt_spec vibro = GPIO_DT_SPEC_GET(USER_NODE, vibro_gpios);


static const gpio_dt_spec button = GPIO_DT_SPEC_GET(USER_NODE, button_gpios);
static gpio_callback button_callback;
static k_work_delayable button_work;
void button_irq_handler(
    const device*,
    gpio_callback*,
    uint32_t
);
void button_work_handler(k_work*);

int init_led() {
    int ret = 0;
    if (!gpio_is_ready_dt(&led)) {
		return 0;
	}

	ret = gpio_pin_configure_dt(&led, GPIO_OUTPUT_ACTIVE);
	if (ret < 0) {
		return 0;
	}

    return ret;
}

int init_display() {
    // const device* display =
    //     DEVICE_DT_GET(DT_CHOSEN(zephyr_display));

        
    if (!device_is_ready(display)) {
        printk("Display is not ready\n");
        return -1;
    }


    int ret = cfb_framebuffer_init(display);
    if (ret != 0) {
        printk("CFB init failed: %d\n", ret);
        return ret;
    }

    display_blanking_off(display);

    cfb_framebuffer_clear(display, false);

    cfb_framebuffer_set_font(display, 0);

    cfb_print(display, "ESP32-C3", 0, 0);
    cfb_print(display, "Zephyr", 0, 12);

    ret = cfb_framebuffer_finalize(display);
    if (ret != 0) {
        printk("Framebuffer flush failed: %d\n", ret);
        return ret;
    }

    return 0;
}

void set_vibration(bool active) {
    gpio_pin_set_dt(&vibro, active ? 1 : 0);
}

int init_vibro() {
    if (!device_is_ready(vibro.port)) {
        printk("Vibro is not ready");
        return -1;
    }

    auto ret = gpio_pin_configure_dt(&vibro, GPIO_OUTPUT_INACTIVE);
    if (ret < 0) {
        printk("Failed configuring vibromotor pin %d\n", ret);
        return ret;
    }

    return 0;
}

int init_button()
{
    if (!gpio_is_ready_dt(&button)) {
        printk("Button GPIO is not ready\n");
        return -1;
    }

    int ret = gpio_pin_configure_dt(
        &button,
        GPIO_INPUT
    );

    if (ret < 0) {
        printk("Failed configuring button: %d\n", ret);
        return ret;
    }

    k_work_init_delayable(
        &button_work,
        button_work_handler
    );

    gpio_init_callback(
        &button_callback,
        button_irq_handler,
        BIT(button.pin)
    );

    ret = gpio_add_callback(
        button.port,
        &button_callback
    );

    if (ret < 0) {
        printk("Failed adding button callback: %d\n", ret);
        return ret;
    }

    ret = gpio_pin_interrupt_configure_dt(
        &button,
        GPIO_INT_EDGE_TO_ACTIVE
    );

    if (ret < 0) {
        printk("Failed configuring button IRQ: %d\n", ret);
        return ret;
    }

    return 0;
}

constexpr int LED_CYCLE_TIME_MS = 500;

void led_thread(void*, void*, void*) {
    while(true) {
        gpio_pin_toggle_dt(&led);
        k_msleep(LED_CYCLE_TIME_MS);
    }
}

K_THREAD_DEFINE(
    led_thread_id, 
    1024,
    led_thread,
    nullptr, 
    nullptr,
    nullptr,
    K_PRIO_PREEMPT(7),
    0,
    100
);


enum class AppEventType : uint8_t {
    ButtonPressed,
};

struct AppEvent {
    AppEventType type;
};

K_MSGQ_DEFINE(
    app_event_queue,
    sizeof(AppEvent),
    8,
    alignof(AppEvent)
);

void app_thread(void*, void*, void*) {
    AppEvent event{};

    bool vibration = false;

    while(true) {
        k_msgq_get(&app_event_queue, &event, K_FOREVER);

        switch (event.type)
        {
        case AppEventType::ButtonPressed:
            set_vibration(!vibration);
            vibration = !vibration;
            break;
        
        default:
            break;
        }
    }
}

K_THREAD_DEFINE(
    app_thread_id,
    1024,
    app_thread,
    nullptr,
    nullptr,
    nullptr,
    K_PRIO_PREEMPT(5),
    0,
    100
);


// void button_thread(void*, void*, void*) {
//     bool button_state = false;

//     int button_pin = 0;
// 	while (true) {
//         button_pin = gpio_pin_get_dt(&button);
//         if (button_pin < 0) {
//             printk("Button error: %d", button_pin);
//         } else if (button_pin == 0) {
//             button_state = false;
//         } else if (button_state == false) {
//             button_state = true;
//             printk("Button pressed");

//             AppEvent event{
//                 .type = AppEventType::ButtonPressed
//             };
//             k_msgq_put(&app_event_queue, &event, K_NO_WAIT);
//         }

//         k_msleep(100);
// 	}
// }

// K_THREAD_DEFINE(
//     button_thread_id,
//     1024,
//     button_thread,
//     nullptr,
//     nullptr,
//     nullptr,
//     K_PRIO_PREEMPT(6),
//     0,
//     100
// );



void button_irq_handler(
    const device*,
    gpio_callback*,
    uint32_t
) {
    k_work_schedule(
        &button_work,
        K_MSEC(20)
    );
}

void button_work_handler(k_work*) {
    const int pressed = gpio_pin_get_dt(&button);

    if (pressed < 0) {
        printk("Button read error: %d\n", pressed);
        return;
    }

    if (pressed == 0) {
        return;
    }

    AppEvent event{
        .type = AppEventType::ButtonPressed
    };

    const int ret = k_msgq_put(
        &app_event_queue,
        &event,
        K_NO_WAIT
    );

    if (ret != 0) {
        printk("App event queue full: %d\n", ret);
    }
}

int main()
{
    auto ret = init_display();
    if (ret != 0) {
        printk("Error when init display: %d\n", ret);
        return ret;
    }

    ret = init_led();

    ret = init_vibro();
    if (ret != 0) {
        printk("Error when init vibromotor: %d\n", ret);
        return ret;
    }

    ret = init_button();
    if (ret != 0) {
        printk("Error when init button: %d\n", ret);
        return ret;
    }

    printk("#Initialization completed!");

    return 0;
}
