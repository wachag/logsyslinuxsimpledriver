# logsyslinuxsimpledriver
Platform driver - character (misc) device interoperability code for demonstration

This driver creates a device file under /dev for each hardware instance it has found in the device tree description.

## Notes:
- Having to go through write() for accessing IO is not the fastest option if you require performance (e.g. if you want to bitbang).
- Many kinds (GPIO, UART, DMA, Video, I2C controller, Sensors, etc.) of hardware have its own framework in the kernel. In most cases writing a character device is not necessary.
- If a character device is really necessary, it could be better to use the full-blown character device API, see Linux Device Drivers 3rd edition for details: https://lwn.net/Kernel/LDD3/
