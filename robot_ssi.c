#include <linux/init.h>
#include <linux/module.h>
#include <linux/gpio.h>
#include <linux/delay.h>
#include <linux/slab.h>

//https://en.wikipedia.org/wiki/Synchronous_Serial_Interface
//defined for raspberry pi B+
#define CLK  25 // GPIO 25 pin 22
#define DATA  8 // GPIO 8 pin 24

#define CLK_DELAY 100
#define READ_ATTEMPTS 0
// change in data must be less than this value to succeed
#define DATA_PASS 200
//by the current spec of the fpga, index reset delay must be between 150us and 0.001s
#define RESET_DELAY CLK_DELAY<<3

MODULE_LICENSE("GPL");

struct task_struct *task;
static struct kobject *ssi_kobject;
static u8 size = 0;
static u32 *a;
static u32 *b;
static u32 *lastVals;
static u8 data_length = 0;

static void showValues(void){
	u8 i;
	for ( i = 0; i < data_length; i ++ ){
		printk(KERN_INFO "SSI: Data[%d]: %d\t%d\t%d\n",
				i, a[i], b[i], lastVals[i]);
	}
}

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
		
		//printk("SSI read: blk %d  i %d val %d", block, i%32, val[block]);		
		usleep_range(CLK_DELAY, CLK_DELAY);
	}
}

static u8 compare_lists(void) {
	u8 i = 0;

	for ( i = 0; i < data_length; i++) {
		if (a[i] != b[i]){
			printk(KERN_ERR "SSI: Data doesnt match: %d\t%d\n", a[i], b[i]);
			return 1;
		}
	}

	return 0;
}

static ssize_t set_ssi(struct kobject *kobj, struct kobj_attribute *attr, const char *buff, size_t count) {
	
	//allocate 32 byte blocks for transmission
	u8 tmp = 0;
	u8 i;
	u8 attempts = 0;
	data_length = 0;
		
	sscanf(buff, "%hhd", &size);
	tmp = size;
	
	if (a || b || lastVals) {
		kfree(a);
		kfree(b);
		kfree(lastVals);
	}

	while (tmp <= size && tmp > 0) { 
		data_length = data_length + 1;
		tmp = tmp - 32;
	}

	printk(KERN_INFO "SSI: allocating %d blocks for %d bits", data_length, size); 
	a = kmalloc(sizeof(*a) * data_length, GFP_KERNEL);
	b = kmalloc(sizeof(*b) * data_length, GFP_KERNEL);
	lastVals = kmalloc(sizeof(*lastVals) * data_length, GFP_KERNEL);

	if (!a || !b || !lastVals) {
		printk(KERN_ERR "SSI: Memory allocation failed.");
	}
	
	do {
		if ( attempts > 0 ) {
			usleep_range(RESET_DELAY*2, RESET_DELAY*2);		
		}
		read_device(a);
		usleep_range(RESET_DELAY, RESET_DELAY);		
		read_device(b);
		attempts = attempts + 1;
	} while (compare_lists() != 0 && attempts < 100);		
	
	printk(KERN_INFO "initial attempts: %d\n", attempts);
	for( i = 0; i < data_length; i = i + 1 ){	
		lastVals[i] = a[i];
		printk(KERN_INFO "initializing lastVal[%d] to %d\n", i, lastVals[i]); 
	}	

	return count;
}



static ssize_t get_ssi(struct kobject *kobj, struct kobj_attribute *attr, char *buf){
	u8 attempts = 0;
	u8 i = 0;
	u8 ret_val = 0;
	s32 diff;

	do {
		if ( attempts > 0 ) {
			usleep_range(RESET_DELAY, RESET_DELAY);		
		}
		read_device(a);
		usleep_range(RESET_DELAY, RESET_DELAY);		
		read_device(b);
		attempts = attempts + 1;
	} while (compare_lists() != 0 && attempts < READ_ATTEMPTS);		

	/*	
	if(attempts >= READ_ATTEMPTS) { 
		printk(KERN_ERR "SSI: ERROR read failed after %d attempts!", attempts);	
		return sprintf(buf, "%d", -1); 
	}
*/
	//showValues();	
	for ( i = 0; i < data_length; i++ ) {
		diff = a[i] - lastVals[i];	
		if ( !(diff < DATA_PASS && diff > -DATA_PASS)){
			printk(KERN_ERR "SSI: ERROR read shows a random change lastValue[%d] %d, current %d", i, lastVals[i], a[i]);	
			return sprintf(buf, "%d", -2); 
		}
	}

	if(attempts > 1){
		printk(KERN_WARNING "SSI: WARNING took multiple (%d) attempts to read from device", attempts);
	}


	for ( i = 0; i < data_length; i = i + 1 ){
		//printk(KERN_INFO "SSI: writing Data: %d\n", a[i]);

		lastVals[i] = a[i];
		ret_val = ret_val + sprintf(buf + strlen(buf), "%d\n", a[i]); 
		//test line for checking SSI device 	
		//ret_val = ret_val + sprintf(buf + strlen(buf), "%d\t%d\n", a[i], b[i]); 
	}
	return ret_val;
}

static struct kobj_attribute ssi_attribute =__ATTR(stream, (S_IRUGO | S_IWUSR), get_ssi, set_ssi);

static int __init robot_ssi_init(void){
  	printk(KERN_INFO "SSI: staring...");
  	
	gpio_request(CLK, "A1");
  	gpio_request(DATA, "A2");

  	gpio_direction_output(CLK, 0);
  	gpio_direction_input(DATA);

	

	ssi_kobject = kobject_create_and_add("ssi", NULL);
	
	if (sysfs_create_file(ssi_kobject, &ssi_attribute.attr)) {
    	pr_debug("failed to create ssi sysfs!\n");
  	}
  
  	printk(KERN_INFO "SSI: staring done.");
  	return 0;
}

static void __exit robot_ssi_exit(void){
  	printk(KERN_INFO "SSI: stopping.");
  	
	gpio_free(CLK);
  	gpio_free(DATA);

  	kfree(a);
  	kfree(b);
	kfree(lastVals);
 
  	kobject_put(ssi_kobject);
}

module_init(robot_ssi_init);
module_exit(robot_ssi_exit);
