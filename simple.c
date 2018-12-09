/*
 * Name: simple.c
 * Author: wachag
 * Description: A simple driver which shows platform driver-miscdevice interoperability
 *
 * What does it do:
 * - it registers a character device (miscdevice)
 * - on write the data will be written to the device base address (in the original Logsys Linux system it will be shown on the LEDs)
 * - on read the data will be read from the device base address (in the original Logsys Linux system it will be read from the LEDs)
 *
 * NOTE: this driver behaves a somewhat limited kind of GPIO (no bidirectional support). For a better
 * GPIO driver one should use the GPIO framework.
 *
 * Also, for setting/getting LEDs a sysfs attribute could be more suitable than a character device.
 *
 * This driver does not demonstrate interrupt handling.
 *
 * Where to start understanding the code:
 * Module entry point is the module_platform_driver macro which registers our driver for which the
 * - Entry point is simple_probe() - when an appropriate device is found
 * - Exit point is simple_remove() - when it is removed.
 *
 * */

#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/miscdevice.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/io.h>

#define DRIVER_NAME "simple"

/*
 * A private struct which contains everything our driver needs.
 * For every device we will allocate such a struct
 * */
struct simple_private {
	struct miscdevice miscdev; /* holds an instance of the registered misc device */
	void * addr; /* holds an instance of the mapped virtual memory space */
};


/*
 * Some pointer magic: we have a pointer to the "miscdev" member of the simple_private struct,
 * cast it to simple_private with the container_of() macro
 * */
#define miscdev_to_simple(misc) container_of(misc,struct simple_private,miscdev);

/*
 * Called when the user opens the device file
 * We won't do anything useful here.
 * */
static int simplemisc_open(struct inode * inode, struct file * file){
	return 0;
}

/*
 * Called when the user closes the device file (more or less, see Linux Device Drivers 3rd edition for a better explanation)
 * We won't do anything useful here.
 * */
static int simplemisc_release(struct inode * inode, struct file * file){
	return 0;
}

/*
 * Called when the user reads from the file.
 * Semantics: buffer: userspace memory pointer to read into, count is the size of the buffer, return
 * value is the number of bytes read.
 *
 * We only read one byte at a time. If the size of the buffer is 0 we return 0, else we return 1 on success
 * (one byte was read) or 0 on error.
 *
 * Note: to access the userspace memory area we have to use copy_to_user (or put_user) to ensure that
 * the appropriate memory pages are valid and in, etc.
 *
 * If we want to be consistent with file positioning (we are a seekable device), we could update f_pos
 */
static ssize_t simplemisc_read(struct file * filp, char __user * buf, size_t count, loff_t * f_pos){
	/*
	 * Note the usage of miscdev_to_simple:
	 * The kernel automatically sets the private_data to the struct miscdevice we've registered.
	 * We can 'upcast' it to our simple_private
	 * */
	struct simple_private * priv=miscdev_to_simple(filp->private_data);
	char data;
	if(count==0)return 0;
	data=ioread8(priv->addr);
	if(copy_to_user(buf,&data,1)==1) return 0; /* copy_to_user returns the number of bytes that could not be copied.
												  if it is 1: we return 0. Userspace will likely try again.  */
	return 1;
}

/*
 * Called when the user wants to write from the file.
 * Semantics are similar to read().
 */
static ssize_t simplemisc_write(struct file * filp, const char __user * buf, size_t count, loff_t * f_pos){
	struct simple_private * priv=miscdev_to_simple(filp->private_data);
	char data;
	if(count==0)return 0;
	if(copy_from_user(&data,buf, 1)==1) return 0;
	iowrite8(data,priv->addr);
	return 1; /* Only one byte */
}

/*
 * Callback functions for our character device (similar to a virtual method table in C++)
 */
const struct file_operations simple_fops = {
	.owner = THIS_MODULE, /* This is necessary so that our kernel module cannot be unloaded while someone uses this file_operations */
	.open = simplemisc_open,
	.release = simplemisc_release,
	.read = simplemisc_read,
	.write = simplemisc_write
};

/*
 * Probe function: called when an instance of our device has been found.
 * pdev describes the hardware device: addresses, interrupts, etv.
 * */
static int simple_probe(struct platform_device *pdev){
	struct simple_private * priv;
	/*
	 * Allocate memory for our private struct.
	 * Note the devm_: this means that this memory will be automatically freed by the kernel
	 * after the remove() function is called.
	 * GFP_KERNEL means that we allocate from the kernel virtual memory address space
	 * */
	priv = devm_kzalloc(&pdev->dev, sizeof(struct simple_private), GFP_KERNEL);
	if(!priv){
		dev_err(&pdev->dev,"Could not allocate private struct!");
		return -ENOMEM;
	}
	/* requests and remaps a physical memory space to kernel virtual memory space */
	priv->addr = devm_ioremap_resource(&pdev->dev,
			platform_get_resource(pdev,IORESOURCE_MEM,0) /* platform_get_resource: gets the device IO memory space physical address and length from device tree description */
	);
	if(!priv->addr){
		dev_err(&pdev->dev,"Could not map physical memory!");
		return -ENOMEM;
	}
	/*
	 * Registers a misc device: we set the file operations (which functions should be called on open, close, etc.
	 * */
	priv->miscdev.minor=MISC_DYNAMIC_MINOR; /* minor number is an ID in the file system representation */
	priv->miscdev.name=DRIVER_NAME; /* the resulting device file name will be based on this */
	priv->miscdev.fops=&simple_fops; /* the callback function table */
	platform_set_drvdata(pdev,priv);
	/*
	 * In the end we register our misc device: if it fails, we return with the failure */
	return misc_register(&priv->miscdev);
}

/*
 * Remove function: called when an instance of our device has been removed
 * */
static int simple_remove(struct platform_device * pdev){
	struct simple_private * priv = platform_get_drvdata(pdev);
	misc_deregister(&priv->miscdev);
	/* Note: no other cleanup is necessary: devm_ functions take care for this */
	return 0;
}

/*
 * An array of IDs our device is compatible with.
 * The device tree source will contain a description of the hardware.
 * */
static const struct of_device_id simple_ids[] = {
		{ .compatible = "xlnx,interrupt-demo-1.0",
		  .data = NULL,
		},

		{}
};
/*
 * This struct describes the behavior of the driver.
 * simple_probe will be called when a compatible device has been found
 * simple_remove will be found on removal
 *
 * simple_ids will contain the device ID strings our driver is compatible with
 * */
static struct platform_driver simple_driver = {
		.driver = {
				.name = DRIVER_NAME,
				.of_match_table = simple_ids,
		},
		.probe = simple_probe,
		.remove = simple_remove,
};

/*
 * Note: instead of a module_init and module_exit macro we register a platform driver
 * See struct simple_driver for further documentation
 * */
module_platform_driver(simple_driver);
MODULE_AUTHOR("wachag");
MODULE_DESCRIPTION("A simple driver which shows platform driver-miscdevice interoperability");
MODULE_LICENSE("GPL");
