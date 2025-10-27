#include <linux/module.h>
#include <linux/kernel.h>       // For printk()
#include <linux/gpio.h>         // For gpio_request, gpio_direction_output/input, gpio_set_value
#include <linux/interrupt.h>    // For request_irq, free_irq, gpio_to_irq
#include <linux/timer.h>        // For timer_setup(), mod_timer(), del_timer_sync()
#include <linux/jiffies.h>      // For jiffies, msecs_to_jiffies
#include <linux/fs.h>           // For register_chrdev, file_operations
#include <linux/uaccess.h>      // For copy_to_user()
#include <linux/gpio/consumer.h>
MODULE_LICENSE("GPL");

// GPIO pin numbers from the lab doc
#define GPIO_RED    67
#define GPIO_YELLOW 68
#define GPIO_GREEN  44
#define GPIO_BTN0   26

#define MAJOR_NUM 61

static struct gpio_desc *led_red, *led_ye, *led_gr, *button_1, *button_2;

// need to track which mode we're in
// 0=normal, 1=flash-red, 2=flash-yellow
static int current_mode = 0;

// timer fires every cycle to update the lights
static struct timer_list traffic_timer;

// for normal mode - need to remember which light is on and how long
// state tracks which light: 0=green, 1=yellow, 2=red
// counter tracks how many cycles the current light has been on
// green needs 3 cycles, yellow needs 1, red needs 2
static int state = 0;
static int counter = 0;

// need to store IRQ number so we can free it later
static int irq_num;

//track whether LEDs are on or off
//used to decide whether to turn off or on an Led when blinking
static int red_state = 0;
static int ye_state = 0;

static int major;

// handles the normal mode sequence
// green stays on 3 cycles, then yellow 1 cycle, then red 2 cycles, repeat
static void handle_normal_mode(void)
{
    // TODO
}

// handles flashing red mode
// red LED toggles on/off each cycle
static void handle_flash_red(void)
{
    // TODO
    if (red_state == 0)
    {
        gpiod_set_value(led_red, 0);
        red_state = 1;
    }
    else
    {
        gpiod_set_value(led_red, 1);
        red_state = 0;
    }
}

// handles flashing yellow mode
// yellow LED toggles on/off each cycle
static void handle_flash_yellow(void)
{
    // TODO
    if (ye_state == 0)
    {
        gpiod_set_value(led_ye, 0);
        ye_state = 1;
    }
    else
    {
        gpiod_set_value(led_ye, 1);
        ye_state = 0;
    }
}

// timer callback - kernel calls this every cycle
// figures out which mode we're in and calls the right handler
static void timer_callback(struct timer_list *t)
{
    if (current_mode == 0) 
        handle_normal_mode();
    else if (current_mode == 1) 
        handle_flash_red();
    else 
        handle_flash_yellow();
    
    // reschedule timer to fire again in 1 second
    mod_timer(&traffic_timer, jiffies + msecs_to_jiffies(1000));
}

// button interrupt handler - kernel calls this when BTN0 is pressed
// need to cycle through modes and reset state when switching
static irqreturn_t btn0_isr(int irq, void *dev_id)
{
    // TODO
    current_mode++;
    state = 0;
    counter = 0;
    //Now I need to interrput

    return IRQ_HANDLED;
}

// read function for /dev/mytraffic
// userspace calls this when they cat /dev/mytraffic
// need to build a string showing mode, cycle rate, and which LEDs are on
static ssize_t device_read(struct file *f, char __user *buf, size_t len, loff_t *off)
{
    // to prevent looping forever
    if (*off > 0) return 0;
    
    // TODO
    // build status string showing mode name, cycle rate, LED status
    // use copy_to_user to send it back
    // update offset
    
    return 0;
}

// called when userspace opens /dev/mytraffic
static int device_open(struct inode *i, struct file *f) 
{ 
    return 0; 
}

// called when userspace closes /dev/mytraffic
static int device_release(struct inode *i, struct file *f) 
{ 
    return 0; 
}

// file operations - tells kernel which functions handle /dev/mytraffic operations
static struct file_operations fops = {
    .owner = THIS_MODULE,
    .open = device_open,
    .release = device_release,
    .read = device_read,
};

// module init - runs when we insmod mytraffic.ko
// need to set up GPIOs, button interrupt, char device, and timer
static int __init traffic_init(void)
{
    int ret;
    int status;
    
    // TODO
    // request GPIO pins using gpio_request for each pin
    // set directions: gpio_direction_output for LEDs, gpio_direction_input for button
    // get IRQ number with gpio_to_irq and store in irq_num
    // request interrupt with request_irq
    // register char device with register_chrdev using MAJOR_NUM
    // setup timer with timer_setup and start it with mod_timer

    major = register_chrdev(MAJOR_NUM, "mytraffic", 0644, NULL, &fops)
    if(major < 0)
    {
        printk(KERN_ALERT "Registering device failed\n");
        return major;
    }

    led_red = gpio_to_desc(67);
    if(!led_red)
    {
        printk(KERN_ALERT "Error acessing pin 67\n")
        return -ENODEV;
    }
    led_ye = gpio_to_desc(68);
    if(!led_ye)
    {
        printk(KERN_ALERT "Error acessing pin 68\n");
        return - ENODEV;
    }
    led_gr = gpio_to_desc(44);
    if(!led_gr)
    {
        printk(KERN_ALERT "Error acessing pin 44\n");
        return -ENODEV;
    }
    button_1 = gpio_to_desc(26);
    if(!button_1)
    {
        printk(KERN_ALERT "Error acessing pin 26\n");
        return -ENODEV;
    }
    button_2 = gpio_to_desc(46);
    if(!button_2)
    {
        printk(KERN_ALERT "Error acessing pin 46\n");
        return -ENODEV;
    }

    status = gpiod_direction_output(led_red);
    if (status)
    {
        printk(KERN_ALERT "Error setting pin 67 to output\n");
        return status;
    }

    status = gpiod_direction_output(led_ye);
    if (status)
    {
        printk(KERN_ALERT "Error setting pin 68 to output\n");
        return status;
    }

    status = gpiod_direction_output(led_gr);
    if (status)
    {
        printk(KERN_ALERT "Error setting pin 44 to output");
        return status;
    }

    status = gpiod_direction_input  (button_1)
    if (status)
    {
        printk(KERN_ALERT "Error setting pin 26 to input\n");
        return status;
    }

    status = gpiod_direction_input(button_2);
    {
        printk(KERN_ALERT "Error setting pin 46 to input\n");
    }
        
    gpiod_set_value(led_red, 0);
    gpiod_set_value(led_ye, 0);
    gpiod_set_value(led_gr, 0);

    red_state = 1;
    ye_state = 1;
    
    printk(KERN_INFO "mytraffic: module loaded\n");
    return 0;
}

// module exit - runs when we rmmod mytraffic
// need to clean up everything to prevent kernel crashes
static void __exit traffic_exit(void)
{
    // TODO
    // stop timer with del_timer_sync
    // turn off all LEDs
    // free interrupt with free_irq using irq_num
    // free GPIOs with gpio_free for each pin
    // unregister char device with unregister_chrdev

    delt_timer(traffic_timer);

    gpiod_set_value(led_red, 0);
    gpiod_set_value(led_ye, 0);
    gpiod_set_value(led_gr, 0);

    gpio_free(led_red);
    gpio_free(led_ye);
    gpio_free(led_gr);
    gpio_free(button_1);
    gpio_free(button_2);

    
    printk(KERN_INFO "mytraffic: module unloaded\n");
}

module_init(traffic_init);
module_exit(traffic_exit);


// what we need for basic features:
//
// need three modes that switch when BTN0 is pressed
// normal mode cycles through green 3 sec, yellow 1 sec, red 2 sec, then repeats
// flashing-red blinks red on/off every second
// flashing-yellow blinks yellow on/off every second
//
// need a character device at /dev/mytraffic (major 61, minor 0)
// when someone reads it, show current mode name, cycle rate (1 Hz), and LED status
//
// timer fires every 1 second to update the lights
