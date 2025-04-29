#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/input.h>
#include <linux/slab.h>


MODULE_AUTHOR("Group 16 OS");
MODULE_DESCRIPTION("Joystick to Drawing Tool Controller");
MODULE_LICENSE("GPL");

static struct input_dev *virtual_mouse;
static struct input_dev *virtual_keyboard;
static struct input_handle *joystick_handle;
static int joystick_id = -1;
module_param(joystick_id, int, 0644);
MODULE_PARM_DESC(joystick_id, "Input device ID for the joystick");

static int sensitivity = 5; /* Movement sensitivity multiplier */
module_param(sensitivity, int, 0644);
MODULE_PARM_DESC(sensitivity, "Sensitivity for joystick movements (1-10)");

static int joystick_connect(struct input_handler *handler, struct input_dev *dev,
                             const struct input_device_id *id);
static void joystick_disconnect(struct input_handle *handle);
static void joystick_event(struct input_handle *handle, unsigned int type,
                           unsigned int code, int value);

/* Device matching for joysticks */
static const struct input_device_id joystick_ids[] = {
    {
        .flags = INPUT_DEVICE_ID_MATCH_EVBIT,
        .evbit = { BIT_MASK(EV_ABS) },
    },
    { } /* Terminating entry */
};


static struct input_handler joystick_handler = {
    .event = joystick_event,
    .connect = joystick_connect,
    .disconnect = joystick_disconnect,
    .name = "joystick_drawer",
    .id_table = joystick_ids,
};

static void joystick_event(struct input_handle *handle, unsigned int type,
                           unsigned int code, int value)
{
    int scaled_value;

    const int center = 128; /* this was adjusted for our joystick to remove drift*/

    if (type == EV_ABS) {
        switch (code) {
        case ABS_X:
            scaled_value = (value - center) * sensitivity / 32;
            input_report_rel(virtual_mouse, REL_X, scaled_value);
            input_sync(virtual_mouse);
            break;
        case ABS_Y:
            scaled_value = (value - center) * sensitivity / 32;
            input_report_rel(virtual_mouse, REL_Y, scaled_value);
            input_sync(virtual_mouse);
            break;
        default:
            break;
        }
    } else if (type == EV_KEY) { //Registering Keyboard events
        switch (code) {
        case BTN_TL: 
            input_report_key(virtual_mouse, BTN_LEFT, value);
            input_sync(virtual_mouse);
            break;
        case BTN_B: 
            input_report_key(virtual_keyboard, KEY_B, value);
            input_sync(virtual_keyboard);
            break;
        case BTN_X: 
            input_report_key(virtual_keyboard, KEY_E, value);
            input_sync(virtual_keyboard);
            break;
        case BTN_Y: 
            input_report_key(virtual_keyboard, KEY_RIGHTBRACE, value);
            input_sync(virtual_keyboard);
            break;
        case BTN_A: 
            input_report_key(virtual_keyboard, KEY_LEFTBRACE, value);
            input_sync(virtual_keyboard);
            break;
        default:
            break;
        }
    }
   
}

static int joystick_connect(struct input_handler *handler, struct input_dev *dev,
                             const struct input_device_id *id)
{
    struct input_handle *handle;
    int error;

    // Skip devices that don't match our device id parameter
    if (joystick_id != -1 && dev->id.product != joystick_id)
        return -ENODEV;

    // Check if this is actually a joystick
    if (!(test_bit(ABS_X, dev->absbit) && test_bit(ABS_Y, dev->absbit)))
        return -ENODEV;

    handle = kzalloc(sizeof(struct input_handle), GFP_KERNEL);
    if (!handle)
        return -ENOMEM;

    handle->dev = dev;
    handle->handler = handler;
    handle->name = "joystick_drawer";

    error = input_register_handle(handle);
    if (error)
        goto err_free_handle;

    error = input_open_device(handle);
    if (error)
        goto err_unregister_handle;

    joystick_handle = handle;
    printk(KERN_INFO "joystick_drawer: Connected to %s\n", dev->name);
    return 0;

err_unregister_handle:
    input_unregister_handle(handle);
err_free_handle:
    kfree(handle);
    return error;
}

static void joystick_disconnect(struct input_handle *handle)
{
    printk(KERN_INFO "joystick_drawer: Disconnected from %s\n", handle->dev->name);
    input_close_device(handle);
    input_unregister_handle(handle);
    kfree(handle);
}

/* Initialize virtual devices */
static int __init create_virtual_devices(void)
{
    int error;

    /* Create virtual mouse */
    virtual_mouse = input_allocate_device();
    if (!virtual_mouse) {
        printk(KERN_ERR "joystick_drawer: Not enough memory for virtual mouse\n");
        return -ENOMEM;
    }

    virtual_mouse->name = "Joystick Virtual Mouse";
    virtual_mouse->phys = "joydraw/mouse";
    virtual_mouse->id.bustype = BUS_VIRTUAL;
    virtual_mouse->id.vendor = 0x0000;
    virtual_mouse->id.product = 0x0000;
    virtual_mouse->id.version = 0x0000;

    set_bit(EV_REL, virtual_mouse->evbit);
    set_bit(REL_X, virtual_mouse->relbit);
    set_bit(REL_Y, virtual_mouse->relbit);
    set_bit(EV_KEY, virtual_mouse->evbit);
    set_bit(BTN_LEFT, virtual_mouse->keybit);
    set_bit(BTN_RIGHT, virtual_mouse->keybit);
    set_bit(BTN_MIDDLE, virtual_mouse->keybit);

    error = input_register_device(virtual_mouse);
    if (error) {
        printk(KERN_ERR "joystick_drawer: Failed to register virtual mouse\n");
        goto err_free_mouse;
    }

    /* Create virtual keyboard */
    virtual_keyboard = input_allocate_device();
    if (!virtual_keyboard) {
        printk(KERN_ERR "joystick_drawer: Not enough memory for virtual keyboard\n");
        error = -ENOMEM;
        goto err_unregister_mouse;
    }

    virtual_keyboard->name = "Joystick Virtual Keyboard";
    virtual_keyboard->phys = "joydraw/keyboard";
    virtual_keyboard->id.bustype = BUS_VIRTUAL;
    virtual_keyboard->id.vendor = 0x0000;
    virtual_keyboard->id.product = 0x0000;
    virtual_keyboard->id.version = 0x0000;

    set_bit(EV_KEY, virtual_keyboard->evbit);
    set_bit(KEY_B, virtual_keyboard->keybit); 
    set_bit(KEY_E, virtual_keyboard->keybit);
    set_bit(KEY_C, virtual_keyboard->keybit); /* Color picker */
    set_bit(KEY_RIGHTBRACE, virtual_keyboard->keybit); /* Increase brush size */
    set_bit(KEY_LEFTBRACE, virtual_keyboard->keybit); /* Decrease brush size */
    set_bit(KEY_R, virtual_keyboard->keybit); /* Red color */
    set_bit(KEY_G, virtual_keyboard->keybit); /* Green color */

    error = input_register_device(virtual_keyboard);
    if (error) {
        printk(KERN_ERR "joystick_drawer: Failed to register virtual keyboard\n");
        goto err_free_keyboard;
    }

    return 0;

err_free_keyboard:
    input_free_device(virtual_keyboard);
err_unregister_mouse:
    input_unregister_device(virtual_mouse);
    virtual_mouse = NULL;
    return error;

err_free_mouse:
    input_free_device(virtual_mouse);
    return error;
}

static int __init joystick_drawer_init(void)
{
    int error;

    error = create_virtual_devices();
    if (error)
        return error;

    error = input_register_handler(&joystick_handler);
    if (error) {
        printk(KERN_ERR "joystick_drawer: Failed to register input handler\n");
        goto err_unregister_devices;
    }

    printk(KERN_INFO "joystick_drawer: Module loaded\n");
    return 0;

err_unregister_devices:
    if (virtual_keyboard) {
        input_unregister_device(virtual_keyboard);
        virtual_keyboard = NULL;
    }
    if (virtual_mouse) {
        input_unregister_device(virtual_mouse);
        virtual_mouse = NULL;
    }
    return error;
}

static void __exit joystick_drawer_exit(void)
{
    input_unregister_handler(&joystick_handler);

    if (virtual_keyboard) {
        input_unregister_device(virtual_keyboard);
        virtual_keyboard = NULL;
    }
    if (virtual_mouse) {
        input_unregister_device(virtual_mouse);
        virtual_mouse = NULL;
    }

    printk(KERN_INFO "joystick_drawer: Module unloaded\n");
}

module_init(joystick_drawer_init);
module_exit(joystick_drawer_exit);
