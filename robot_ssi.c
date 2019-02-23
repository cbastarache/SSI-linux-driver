#include <linux/init.h>
#include <linux/module.h>
#include <linux/gpio.h>
#include <linux/delay.h>
#include <linux/slab.h>

//https://en.wikipedia.org/wiki/Synchronous_Serial_Interface
//defined for raspberry pi B+
#define CLK  25 // GPIO 25 pin 22
#define DATA  8 // GPIO 8 pin 24

#define CLK_DELAY 20
#define READ_ATTEMPTS 3

MODULE_LICENSE("GPL");

struct task_struct *task;
static struct kobject *ssi_kobject;
static u8 size = 0;
static u32 *a;
static u32 *b;
static u8 data_length = 0;

static void read_device(u32 *val){
	u8 i;
	u8 block = 0;

	for ( i = 0; i < data_length; i++){
		val[i] = 0;
	}

	for (i = 0; i < size; i = i + 1){
		//trigger clock pulse
		gpio_set_value(CLK, 1);
		usleep_range(CLK_DELAY, CLK_DELAY);
		gpio_set_value(CLK, 0);

		//finds index of array with integer division
		block = i / (32);
		val[block] = val[block] | gpio_get_value(DATA) << (i%32);
		
		//printk("SSI read: blk %d val %d", block, val[block]);		
		usleep_range(CLK_DELAY, CLK_DELAY);
	}
}

static ssize_t set_ssi(struct kobject *kobj, struct kobj_attribute *attr, const char *buff, size_t count) {
	
	//allocate 32 byte blocks for transmission
	u8 tmp = 0;
	data_length = 0;
	
	sscanf(buff, "%hhd", &size);
	tmp = size;
	
	if (a || b) {
		kfree(a);
		kfree(b);
	}

	while (tmp <= size && tmp > 0) { 
		data_length = data_length + 1;
		tmp = tmp - 32;
	}

	printk(KERN_INFO "SSI: allocating %d blocks for %d bits", data_length, size); 
	a = kmalloc(sizeof(*a) * data_length, GFP_KERNEL);
	b = kmalloc(sizeof(*b) * data_length, GFP_KERNEL);

	if (!a || !b) {
		printk(KERN_ERR "SSI: Memory allocation failed.");
	}
		
	return count;
}

static u8 compare_lists(void) {
	u8 i = 0;

	for ( i = 0; i < data_length; i++) {
		if (a[i] != b[i])
			return 1;
	}
	return 0;
}

static ssize_t get_ssi(struct kobject *kobj, struct kobj_attribute *attr, char *buf){
	u8 attempts = 0;
	u8 i = 0;
	u8 ret_val = 0;

	do {
		read_device(a);
		read_device(b);
		attempts = attempts + 1;
	} while (compare_lists() != 0 && attempts <= READ_ATTEMPTS);		
	
	if(attempts >= READ_ATTEMPTS) { 
		printk(KERN_ERR "SSI: ERROR read failed after %d attempts!", attempts);	
		return sprintf(buf, "%d", -1); 
	}
	
	if(attempts > 1){
		printk(KERN_WARNING "SSI: WARNING took multiple attempts to read from device");
	}

	for ( i = 0; i < data_length; i = i + 1 )
		ret_val = ret_val + sprintf(buf + strlen(buf), "%d\n", a[i]); 
	return ret_val;
}

static struct kobj_attribute ssi_attribute =__ATTR(stream, (S_IRUGO | S_IWUSR), get_ssi, set_ssi);

static int __init robot_ssi_init(void){
  	printk(KERN_INFO "SSI: staring...");
  	
	gpio_request(A1, "A1");
  	gpio_request(A2, "A2");

  	gpio_direction_output(A1, 0);
  	gpio_direction_input(A2);

	ssi_kobject = kobject_create_and_add("ssi", NULL);
	
	if (sysfs_create_file(ssi_kobject, &ssi_attribute.attr)) {
    	pr_debug("failed to create ssi sysfs!\n");
  	}
  
  	printk(KERN_INFO "SSI: staring done.");
  	return 0;
}

static void __exit robot_ssi_exit(void){
  	printk(KERN_INFO "SSI: stopping.");
  	
	gpio_free(A1);
  	gpio_free(A2);

  	kfree(a);
  	kfree(b);
 
  	kobject_put(ssi_kobject);
}

module_init(robot_ssi_init);
module_exit(robot_ssi_exit);
