# EC535 – Lab 4: Traffic Light Kernel Module System

## Team Members
- **Phyliss Darko** — phylissd@bu.edu  
- **Tymoteusz Pardej** — tpardej@bu.edu

---

## Project Overview
This project implements a fully kernel-space traffic light controller for the BeagleBone Black.  
The module handles GPIO output, button interrupts, and a recurring kernel timer to operate a traffic signal with three LEDs and two pushbuttons.  
A character device driver (/dev/mytraffic) exposes system state to userspace and allows cycle-rate adjustments.

All traffic-light behavior is implemented in kernelspace, ensuring predictable timing and controlled access to hardware. Userspace interaction is strictly limited to reading/writing the character device.

---

## System Architecture Diagram

```
                           ┌───────────────────────────────┐
                           │         Userspace             │
                           │───────────────────────────────│
                           │   cat /dev/mytraffic          │
                           │   echo 5 > /dev/mytraffic     │
                           └───────────────┬───────────────┘
                                           │
                                           │ read/write
                                           ▼
                          ┌────────────────────────────────────┐
                          │          Kernel Module             │
                          │             mytraffic              │
                          │────────────────────────────────────│
                          │  • Mode State Machine              │
                          │      - Normal (G→Y→R cycles)       │
                          │      - Flashing Red                │
                          │      - Flashing Yellow             │
                          │      - Pedestrian Stop Phase       │
                          │                                    │
                          │  • Timer (mod_timer, jiffies)      │
                          │  • Interrupt Handlers (BTN0/BTN1)  │
                          │  • Character Device (major 61)     │
                          └───────────────┬────────────────────┘
                                          │ GPIO operations
                                          ▼
         ┌──────────────────────────────────────────────────────────────────┐
         │                           Hardware                               │
         │──────────────────────────────────────────────────────────────────│
         │  BeagleBone Black (GPIO)                                         │
         │     - GPIO44 → Green LED                                         │
         │     - GPIO68 → Yellow LED                                        │
         │     - GPIO67 → Red LED                                           │
         │     - GPIO26 → BTN0 (mode switch)                                │
         │     - GPIO46 → BTN1 (pedestrian call)                            │
         │──────────────────────────────────────────────────────────────────│
         │     LED Traffic Module + Breadboard + 330Ω resistors             │
         └──────────────────────────────────────────────────────────────────┘
```

---

## Features

### Operational Modes (BTN0)
- **Normal Mode:**  
  green 3 cycles → yellow 1 → red 2 → repeat
- **Flashing-Red Mode:**  
  red toggles on/off every cycle
- **Flashing-Yellow Mode:**  
  yellow toggles on/off every cycle

### Pedestrian Call (BTN1)
- Only active during normal mode  
- On next red phase, activates **red + yellow** for 5 cycles  
- Automatically returns to normal sequence afterward

### Character Device: /dev/mytraffic
**Read:** prints  
- current mode  
- current cycle frequency (Hz)  
- LED states  
- pedestrian status  

**Write:**  
- integer 1–9 adjusts cycle rate to N Hz  
- invalid writes are ignored without error  

### Kernel Implementation Details
- GPIO requested with `gpio_request`
- LEDs configured as outputs, buttons as inputs
- External interrupts configured using `gpio_to_irq` + `request_irq`
- Timer configured using `timer_setup` + `mod_timer`
- State machine maintains cycle transitions
- Memory allocated with `kmalloc` and freed on exit

---

## Resources Used
1. https://kernel.org/doc/html/v4.19/driver-api/gpio/consumer.html  
2. https://vadl.github.io/beagleboneblack/2016/07/29/setting-up-bbb-gpio  
3. https://lwn.net/Articles/533632/  
4. BeagleBone Black System Reference Manual  
5. Linux Device Drivers, 3rd Edition (Interrupts chapter)  
6. https://docs.kernel.org/next/core-api/kernel-api.html  

---

## Demo Video
https://drive.google.com/file/d/117TO9bLrzItnMFyNnjVif7072EyQmrLS/view?usp=sharing
