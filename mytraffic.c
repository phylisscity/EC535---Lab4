#include <linux/module.h>
#include <linux/kernel.h>       // For printk()
#include <linux/gpio.h>         // For gpio_request, gpio_direction_output/input, gpio_set_value
#include <linux/interrupt.h>    // For request_irq, free_irq, gpio_to_irq
#include <linux/timer.h>        // For timer_setup(), mod_timer(), del_timer_sync()
#include <linux/jiffies.h>      // For jiffies, msecs_to_jiffies
#include <linux/fs.h>           // For register_chrdev, file_operations
#include <linux/uaccess.h>      // For copy_to_user()

MODULE_LICENSE("GPL");

// GPIO pin numbers from the lab doc
#define GPIO_RED    67
#define GPIO_YELLOW 68
#define GPIO_GREEN  44
#define GPIO_BTN0   26
#define GPIO_BTN1   46

#define MAJOR_NUM 61

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

//Tracks whether pedestrain call has been activated
static int pedestrain_call = 0;

// need to store IRQ number so we can free it later
static int irq_num_0;
static int irq_num_1;

static unsigned int cycle_speed = 1000; //the cycle speed in miliseconds

//The write function needs to be able to copy from user to kernel
static char *trafficBuf; //Buffer for the write function

static unsigned capacity = 10; //size of buffer
static int trafficBuf_len; //lenght of current message

// handles the normal mode sequence
// green stays on 3 cycles, then yellow 1 cycle, then red 2 cycles, repeat
//ultimately a state machine. each time the timer fires (1s) this funct is called, and based on state,
//we turn on the right LED and turn off others. We increment counter to track how many cycles
//the current light has been on. when target cycles have been hit, we move to next state
static void handle_normal_mode(void)
{
    // TODO
    //0 - green (on for 3cycles, then move to state 1/yellow)
    if (state == 0) {
        gpio_set_value(GPIO_GREEN, 1);
        gpio_set_value(GPIO_YELLOW, 0);
        gpio_set_value(GPIO_RED, 0);

        counter++;
        if (counter >= 3) {
            state = 1; //move to yellow
            counter = 0;
        }
    }

    //1 - yellow (on for 1 cycle, then move to state 2/red)
    else if (state == 1) {
        gpio_set_value(GPIO_GREEN, 0);
        gpio_set_value(GPIO_YELLOW, 1);
        gpio_set_value(GPIO_RED, 0);

        counter++;
        if ((pedestrain_call = 0) && (counter >= 1)) {
            state = 2; //moves to red
            counter = 0;
        }
        else if((pedestrain_call == 1) && (counter >= 1))
        {
	  state = 3; //moves to pedestrain call
	  pedestrain_call = 0; //Turn off the pedestrain call
          counter = 0;
        }
    }

    //2 - red (on for 2 cycles, then wrap back to state 0/green)
    else if (state == 2) {
        gpio_set_value(GPIO_GREEN, 0);
        gpio_set_value(GPIO_YELLOW, 0);
        gpio_set_value(GPIO_RED, 1);

        counter++;
        if (counter >= 2) {
            state = 0; //now wraps to green again
            counter = 0;
        }
    }

    //3 - pedestrain call (on for 5 cycles, both red and yellow)
    // when done wrap back to state 0/green
    else if (state == 3)
    {
        gpio_set_value(GPIO_GREEN, 0);
        gpio_set_value(GPIO_YELLOW, 1);
        gpio_set_value(GPIO_RED, 1);

        counter ++;
        if (counter >= 5)
        {
            state = 0;
            counter = 0;
        }
    }
    

}

// handles flashing red mode
// red LED toggles on/off each cycle
static void handle_flash_red(void)
{
    static int toggle = 0;  // keeps track of on/off state
    
    gpio_set_value(GPIO_RED, toggle);
    gpio_set_value(GPIO_YELLOW, 0);
    gpio_set_value(GPIO_GREEN, 0);
    
    toggle = !toggle;  // flip between 0 and 1
}

// handles flashing yellow mode
// yellow LED toggles on/off each cycle
static void handle_flash_yellow(void)
{
     static int toggle = 0;
    
    gpio_set_value(GPIO_RED, 0);
    gpio_set_value(GPIO_YELLOW, toggle);
    gpio_set_value(GPIO_GREEN, 0);
    
    toggle = !toggle;
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
    mod_timer(&traffic_timer, jiffies + msecs_to_jiffies(cycle_speed));
}


// button interrupt handler - kernel calls this when BTN0 is pressed
// need to cycle through modes and reset state when switching
static irqreturn_t btn0_isr(int irq, void *dev_id)
{
    // cycle through modes: 0 -> 1 -> 2 -> 0
    current_mode = (current_mode + 1) % 3;
    
    // reset state and counter when switching modes
    state = 0;
    counter = 0;
    
    printk(KERN_INFO "mytraffic: switched to mode %d\n", current_mode);

    return IRQ_HANDLED;
}

//Interrupt handler for BTN1
//Controls the pedestrain call
static irqreturn_t btn1_isr(int irq, void *dev_id)
{
    // turns on pedestain call
    // does not affect state and counter, they can continue normally
    pedestrain_call = 1;
    
    printk(KERN_INFO "mytraffic: switched to pedestrain call\n");

    return IRQ_HANDLED;
}

// read function for /dev/mytraffic
// userspace calls this when they cat /dev/mytraffic
// need to build a string showing mode, cycle rate, and which LEDs are on
static ssize_t device_read(struct file *f, char __user *buf, size_t len, loff_t *off)
{
    char status[256];
    int status_len;
    const char *mode_name;

    // to prevent looping forever
    if (*off > 0) return 0;
    
    // figure out mode name based on current_mode
    if (current_mode == 0)
        mode_name = "normal";
    else if (current_mode == 1)
        mode_name = "flashing-red";
    else
        mode_name = "flashing-yellow";
    
    // read current LED states
    int red_val = gpio_get_value(GPIO_RED);
    int yellow_val = gpio_get_value(GPIO_YELLOW);
    int green_val = gpio_get_value(GPIO_GREEN);
    
    // build the status string
    status_len = snprintf(status, sizeof(status),
        "mode: %s\nrate: %d Hz\nlights: red %s, yellow %s, green %s\n",
	1/(cycle_speed/1000),
        mode_name,
        red_val ? "on" : "off",
        yellow_val ? "on" : "off",
        green_val ? "on" : "off"
    );
    
    // make sure we don't overflow user's buffer
    if (len < status_len)
        status_len = len;
    
    // send to userspace
    if (copy_to_user(buf, status, status_len))
        return -EFAULT;
    
    // update offset so we don't loop
    *off += status_len;
    
    return status_len;
}

static ssize_t device_write(struct file *f, char __user *buf, size_t len, loff_t *off)
{
    char new_cycle[10], *nwptr = new_cycle;
    char *endptr;
    int new_cycle_len;
    long temp;

    if (*off >= capacity) return 0;

    if (len > capacity - *off) //prevent reading more than one number
    {
        len = capacity - *off;
    }

    if (copy_from_user(trafficBuf, buf, len))
    {
        return -EFAULT;
    }

    memcpy(&new_cycle, &trafficBuf, len);
    new_cycle_len = sizeof(new_cycle);
    endptr += new_cycle_len;
    temp = simple_strtol(nwptr, &endptr, 10);

    if (temp < 1)
      {
	printk(KERN_ALERT "Cannot set hz to 0");
      }
    else
      {
	cycle_speed = 1000/temp;
      }
      
    *off += new_cycle_len;
    trafficBuf_len  = *off;
    return new_cycle_len;
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
    .write = device_write,
};


// module init - runs when we insmod mytraffic.ko
// need to set up GPIOs, button interrupt, char device, and timer
static int __init traffic_init(void)
{
    int ret;
    
    // request GPIO pins
    ret = gpio_request(GPIO_RED, "red_led");
    if (ret) {
        printk(KERN_ALERT "mytraffic: failed to request GPIO_RED\n");
        return ret;
    }
    
    ret = gpio_request(GPIO_YELLOW, "yellow_led");
    if (ret) {
        printk(KERN_ALERT "mytraffic: failed to request GPIO_YELLOW\n");
        gpio_free(GPIO_RED);
        return ret;
    }
    
    ret = gpio_request(GPIO_GREEN, "green_led");
    if (ret) {
        printk(KERN_ALERT "mytraffic: failed to request GPIO_GREEN\n");
        gpio_free(GPIO_RED);
        gpio_free(GPIO_YELLOW);
        return ret;
    }
    
    ret = gpio_request(GPIO_BTN0, "btn0");
    if (ret) {
        printk(KERN_ALERT "mytraffic: failed to request GPIO_BTN0\n");
        gpio_free(GPIO_RED);
        gpio_free(GPIO_YELLOW);
        gpio_free(GPIO_GREEN);
        return ret;
    }

    ret = gpio_request(GPIO_BTN1, "btn1");
    if (ret) {
        printk(KERN_ALERT "mytraffic: failed to request GPIO_BTN1\n");
        gpio_free(GPIO_RED);
        gpio_free(GPIO_YELLOW);
        gpio_free(GPIO_GREEN);
        gpio_free(GPIO_BTN0);
        return ret;
    }
    
    // set GPIO directions - LEDs as output (init to 0), button as input
    gpio_direction_output(GPIO_RED, 0);
    gpio_direction_output(GPIO_YELLOW, 0);
    gpio_direction_output(GPIO_GREEN, 0);
    gpio_direction_input(GPIO_BTN0);
    gpio_direction_input(GPIO_BTN1);
    
    // get IRQ number for BTN0 and request interrupt
    irq_num_0 = gpio_to_irq(GPIO_BTN0);
    ret = request_irq(irq_num_0, btn0_isr, IRQF_TRIGGER_FALLING, "mytraffic_btn0", NULL);
    if (ret) {
        printk(KERN_ALERT "mytraffic: failed to request IRQ for BTN0\n");
        gpio_free(GPIO_RED);
        gpio_free(GPIO_YELLOW);
        gpio_free(GPIO_GREEN);
        gpio_free(GPIO_BTN0);
        gpio_free(GPIO_BTN1);
        return ret;
    }

       // get IRQ number for BTN1 and request interrupt
    irq_num_1 = gpio_to_irq(GPIO_BTN1);
    ret = request_irq(irq_num_1, btn1_isr, IRQF_TRIGGER_FALLING, "mytraffic_btn0", NULL);
    if (ret) {
        printk(KERN_ALERT "mytraffic: failed to request IRQ for BTN1\n");
        gpio_free(GPIO_RED);
        gpio_free(GPIO_YELLOW);
        gpio_free(GPIO_GREEN);
        gpio_free(GPIO_BTN0);
        gpio_free(GPIO_BTN1);
        return ret;
    }
    
    // register character device
    ret = register_chrdev(MAJOR_NUM, "mytraffic", &fops);
    if (ret < 0) {
        printk(KERN_ALERT "mytraffic: failed to register char device\n");
        free_irq(irq_num_0, NULL);
	free_irq(irq_num_1, NULL);
        gpio_free(GPIO_RED);
        gpio_free(GPIO_YELLOW);
        gpio_free(GPIO_GREEN);
        gpio_free(GPIO_BTN0);
	gpio_free(GPIO_BTN1);
        return ret;
    }

    //allocating buffer
    trafficBuf = kmalloc(capacity, GFP_KERNEL);
    if (!trafficBuf)
      {
	printk(KERN_ALERT "Insufficient kernel memory\n");
        free_irq(irq_num_0, NULL);
	free_irq(irq_num_1, NULL);
        gpio_free(GPIO_RED);
        gpio_free(GPIO_YELLOW);
        gpio_free(GPIO_GREEN);
        gpio_free(GPIO_BTN0);
	gpio_free(GPIO_BTN1);
	return -ENOMEM;
      }
    memset(trafficBuf, 0, capacity);
    trafficBuf_len = 0;
    
    // setup and start timer
    timer_setup(&traffic_timer, timer_callback, 0);
    mod_timer(&traffic_timer, jiffies + msecs_to_jiffies(cycle_speed));
    
    printk(KERN_INFO "mytraffic: module loaded\n");
    return 0;
}

// module exit - runs when we rmmod mytraffic
// need to clean up everything to prevent kernel crashes
static void __exit traffic_exit(void)
{
    // stop timer
    del_timer_sync(&traffic_timer);
    
    // turn off all LEDs
    gpio_set_value(GPIO_RED, 0);
    gpio_set_value(GPIO_YELLOW, 0);
    gpio_set_value(GPIO_GREEN, 0);
    
    // free interrupt
    free_irq(irq_num_0, NULL);
    free_irq(irq_num_1, NULL);
    
    // free GPIOs
    gpio_free(GPIO_RED);
    gpio_free(GPIO_YELLOW);
    gpio_free(GPIO_GREEN);
    gpio_free(GPIO_BTN0);
    gpio_free(GPIO_BTN1);
    
    // unregister char device
    unregister_chrdev(MAJOR_NUM, "mytraffic");
    
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

//Additionall features:
// Pedestrain button
// When BTN1 is pressed the next "stop" will activate both red and green for 5 cycles
// Afterwards the program return to normal operation
// Only needs to work when operating in normal mode
//
//Writable character device
//Allows to alter the cycle speed by writing to /dev/mytraffic
//Can set HZ between 1 to 9 (inclusive)
