#include <linux/module.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/i2c.h>
#include <linux/delay.h>

//i2cdetect all
//echo > /proc/i2cdetectall
//cat  /proc/i2cdetectall

//i2cdect i2_n address
//echo n > /proc/i2cdetect
//cat /proc/i2cdetect

#define I2C_ADAPTER_NUM  (128)
unsigned char tbuf = 0;
unsigned int i2c_bus;
unsigned int i2c_bus_count = 0;
unsigned int i2c_bus_list[I2C_ADAPTER_NUM];
unsigned char i2c_addr[256];

struct i2c_msg msg = {
	.flags = 0,
	.buf = &tbuf,
	.len = 1,
};

static int i2c_detect_show(struct seq_file *m, void *v)
{
	unsigned char addr;
	seq_printf(m, "scan i2c_%d bus:\n", i2c_bus);
	for (addr = 0x1; addr < 0x80; addr++) {
		if (i2c_addr[addr] != 0xff)
			seq_printf(m, "0x%X ", i2c_addr[addr]);
	}
	seq_printf(m, "\n");
	return 0;
}

ssize_t i2c_detect_write(struct file * file, const char __user * buffer,
			 size_t count, loff_t * ppos)
{
	int addr;
	int ret;
	struct i2c_adapter *adapter;
	memset(i2c_addr, 0xff, sizeof(i2c_addr));

	i2c_bus = simple_strtoul(buffer, NULL, 10);
	printk("i2c_bus=%d\n", i2c_bus);

	adapter = i2c_get_adapter(i2c_bus);
	if (!adapter) {
		printk("get i2c_%d adapter err\n", i2c_bus);
		return -ENODEV;
	} else {
		printk("get i2c_%d adapter ok\n", i2c_bus);
	}

	for (addr = 0x1; addr < 0x80; addr++) {
		msg.addr = addr;
		ret = i2c_transfer(adapter, &msg, 1);
		if (ret > 0) {
			printk("found i2c　addr 0x%X\n", addr);
			i2c_addr[addr] = addr;
		}
	};

	i2c_put_adapter(adapter);
	return count;
}

//0xff addr1 addr2 0xff addr3 addr4 0xff
static int i2c_detect_all_show(struct seq_file *m, void *v)
{
	int i = 0;
	int j = 0;

	for (i = 0; i < i2c_bus_count; i++) {
		seq_printf(m, "Scan iic bus%02d:", i2c_bus_list[i]);
		j++;
		while (i2c_addr[j] != 0xff) {
			seq_printf(m, "  0x%02X", i2c_addr[j]);
			j++;
		}
		seq_printf(m, "\n");
	}
	return 0;
}

ssize_t i2c_detect_all_write(struct file * file, const char __user * buffer,
			     size_t count, loff_t * ppos)
{
	int i;
	int ret;
	int addr;
	int addr_count = 0;
	struct i2c_adapter *adapter;
	memset(i2c_addr, 0xff, sizeof(i2c_addr));

	for (i = 0; i < i2c_bus_count; i++) {
		adapter = i2c_get_adapter(i2c_bus_list[i]);
		if (!adapter) {
			printk("get i2c-%d adapter err\n", i);
			return -ENODEV;
		}

		addr_count++;

		printk(KERN_CONT "\nDetect i2c-%02d:", i2c_bus_list[i]);
		for (addr = 0x1; addr < 0x80; addr++) {
			msg.addr = addr;
			ret = i2c_transfer(adapter, &msg, 1);
			if (ret > 0) {
				printk(KERN_CONT "\t0x%02X",addr);
				i2c_addr[addr_count++] = addr;
			}
		};
		i2c_put_adapter(adapter);
	}
	return count;
}

static int i2c_detect_open(struct inode *inode, struct file *file)
{
	return single_open(file, i2c_detect_show, NULL);
}

static int i2c_detect_all_open(struct inode *inode, struct file *file)
{
	return single_open(file, i2c_detect_all_show, NULL);
}

static const struct file_operations i2c_detect_fops = {
	.owner = THIS_MODULE,
	.open = i2c_detect_open,
	.read = seq_read,
	.write = i2c_detect_write,
};

static const struct file_operations i2c_detect_all_fops = {
	.owner = THIS_MODULE,
	.open = i2c_detect_all_open,
	.read = seq_read,
	.write = i2c_detect_all_write,
};

void i2c_bus_detect(void)
{
	int i;
	struct i2c_adapter *adapter;
	memset(i2c_bus_list, 0, sizeof(i2c_bus_list));

	for (i = 0; i < I2C_ADAPTER_NUM; i++) {
		adapter = i2c_get_adapter(i);
		if (adapter) {
		    printk("found i2c_bus: %02d\n", i);
			i2c_bus_list[i2c_bus_count++] = i;
			i2c_put_adapter(adapter);
		}
	}
	printk("Total i2c_adapter: %d\n", i2c_bus_count);
}

static int __init i2c_detect_init(void)
{
	i2c_bus_detect();
	memset(i2c_addr, 0xff, sizeof(i2c_addr));
	proc_create("i2cdetect", S_IRWXUGO, NULL, &i2c_detect_fops);
	proc_create("i2cdetectall", S_IRWXUGO, NULL, &i2c_detect_all_fops);
	return 0;
}

static void __exit i2c_detect_exit(void)
{
	remove_proc_entry("i2cdetect", NULL);
	remove_proc_entry("i2cdetectall", NULL);
}

module_init(i2c_detect_init);
module_exit(i2c_detect_exit);
MODULE_AUTHOR("www");
MODULE_LICENSE("GPL");
