// SPDX-License-Identifier: GPL-2.0
/*
 * ASUS Zenbook S14 (UX5406AA) Battery Charge Threshold Driver
 *
 * On the 2026 ASUS Zenbook S14 UX5406AA, the BIOS WMI method DSTS(0x00120057)
 * returns an 8-byte ACPI_TYPE_BUFFER (BCMV) instead of an ACPI_TYPE_INTEGER.
 * Because mainline asus-wmi only checks for ACPI_TYPE_INTEGER in
 * asus_wmi_evaluate_method3(), it fails to detect ASUS_WMI_DEVID_RSOC
 * (0x00120057) and does not register charge_control_end_threshold.
 *
 * Meanwhile, DEVS(0x00120057, val) calls \_SB.PC00.LPCB.EC0.SRSC(val) and
 * returns an integer (1), while \_SB.PC00.LPCB.EC0.REBC(0x05, 0x03) reads back
 * the active threshold from the Embedded Controller.
 */

#include <linux/acpi.h>
#include <linux/device.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/notifier.h>
#include <linux/platform_data/x86/asus-wmi.h>
#include <linux/power_supply.h>
#include <linux/suspend.h>
#include <acpi/battery.h>

#define ASUS_EC_REBC_PATH "\\_SB.PC00.LPCB.EC0.REBC"

static int default_threshold = 80;
module_param(default_threshold, int, 0644);
MODULE_PARM_DESC(default_threshold, "Initial battery charge threshold on load: 60, 80 or 100 (default: 80)");

static bool use_myasus_flags = false;
module_param(use_myasus_flags, bool, 0644);
MODULE_PARM_DESC(use_myasus_flags, "Use Windows MyASUS SRSC mode flags (0x80 for 80%, 0x100 for 100%)");

static int cached_end_threshold = 80;
static DEFINE_MUTEX(ux5406_lock);

static bool ux5406_threshold_valid(int value)
{
	return value == 60 || value == 80 || value == 100;
}

static int ux5406_ec_read_rsoc(u8 out_buf[3])
{
	struct acpi_buffer output = { ACPI_ALLOCATE_BUFFER, NULL };
	union acpi_object params[2];
	struct acpi_object_list input;
	union acpi_object *obj;
	acpi_status status;
	int ret = 0;

	params[0].type = ACPI_TYPE_INTEGER;
	params[0].integer.value = 0x05;
	params[1].type = ACPI_TYPE_INTEGER;
	params[1].integer.value = 0x03;

	input.count = 2;
	input.pointer = params;

	status = acpi_evaluate_object(NULL, ASUS_EC_REBC_PATH, &input, &output);
	if (ACPI_FAILURE(status))
		return -EIO;

	obj = output.pointer;
	if (!obj || obj->type != ACPI_TYPE_BUFFER || obj->buffer.length < 3) {
		ret = -EPROTO;
		goto out_free;
	}

	out_buf[0] = obj->buffer.pointer[0];
	out_buf[1] = obj->buffer.pointer[1];
	out_buf[2] = obj->buffer.pointer[2];

out_free:
	kfree(output.pointer);
	return ret;
}

static int __ux5406_set_threshold(int value)
{
	u32 ctrl_param = value;
	u32 retval = 0;
	int err;

	lockdep_assert_held(&ux5406_lock);

	if (!ux5406_threshold_valid(value))
		return -EINVAL;

	if (use_myasus_flags) {
		if (value == 80)
			ctrl_param = 0x80;
		else if (value == 100)
			ctrl_param = 0x0100;
	}

	err = asus_wmi_set_devstate(ASUS_WMI_DEVID_RSOC, ctrl_param, &retval);
	if (err)
		return err;

	if (retval != 1) {
		pr_warn("asus_ux5406_battery: DEVS(0x00120057, 0x%x) returned 0x%x\n",
			ctrl_param, retval);
		return -EIO;
	}

	cached_end_threshold = value;
	return 0;
}

static int ux5406_set_threshold(int value)
{
	guard(mutex)(&ux5406_lock);
	return __ux5406_set_threshold(value);
}

static ssize_t charge_control_end_threshold_show(struct device *dev,
						 struct device_attribute *attr,
						 char *buf)
{
	u8 ec_buf[3] = { 0 };
	int ret;

	ret = ux5406_ec_read_rsoc(ec_buf);
	if (ret)
		return ret;

	if (ec_buf[0] < 1 || ec_buf[0] > 100)
		return -EIO;

	return sysfs_emit(buf, "%d\n", ec_buf[0]);
}

static ssize_t charge_control_end_threshold_store(struct device *dev,
						  struct device_attribute *attr,
						  const char *buf, size_t count)
{
	int value;
	int ret;

	ret = kstrtouint(buf, 10, &value);
	if (ret)
		return ret;

	ret = ux5406_set_threshold(value);
	if (ret)
		return ret;

	return count;
}

static DEVICE_ATTR_RW(charge_control_end_threshold);

static ssize_t ec_rsoc_raw_show(struct device *dev,
				struct device_attribute *attr,
				char *buf)
{
	u8 ec_buf[3] = { 0 };
	int ret;

	ret = ux5406_ec_read_rsoc(ec_buf);
	if (ret)
		return ret;

	return sysfs_emit(buf, "%02x %02x %02x\n", ec_buf[0], ec_buf[1], ec_buf[2]);
}

static DEVICE_ATTR_RO(ec_rsoc_raw);

static int ux5406_battery_add(struct power_supply *battery,
			      struct acpi_battery_hook *hook)
{
	int err;

	if (strcmp(battery->desc->name, "BAT0") != 0 &&
	    strcmp(battery->desc->name, "BAT1") != 0 &&
	    strcmp(battery->desc->name, "BATC") != 0 &&
	    strcmp(battery->desc->name, "BATT") != 0)
		return -ENODEV;

	err = device_create_file(&battery->dev,
				 &dev_attr_charge_control_end_threshold);
	if (err)
		return err;

	err = device_create_file(&battery->dev, &dev_attr_ec_rsoc_raw);
	if (err) {
		device_remove_file(&battery->dev,
				   &dev_attr_charge_control_end_threshold);
		return err;
	}

	err = ux5406_set_threshold(default_threshold);
	if (err)
		pr_warn("asus_ux5406_battery: initial threshold %d not applied: %d\n",
			default_threshold, err);

	pr_info("asus_ux5406_battery: attached to %s (threshold=%d%%)\n",
		battery->desc->name, cached_end_threshold);
	return 0;
}

static int ux5406_battery_remove(struct power_supply *battery,
				 struct acpi_battery_hook *hook)
{
	device_remove_file(&battery->dev, &dev_attr_ec_rsoc_raw);
	device_remove_file(&battery->dev,
			   &dev_attr_charge_control_end_threshold);
	return 0;
}

static struct acpi_battery_hook ux5406_battery_hook = {
	.name = "ASUS UX5406 Battery Extension",
	.add_battery = ux5406_battery_add,
	.remove_battery = ux5406_battery_remove,
};

static int ux5406_pm_notify(struct notifier_block *nb,
			    unsigned long mode, void *_unused)
{
	switch (mode) {
	case PM_POST_HIBERNATION:
	case PM_POST_SUSPEND:
	case PM_POST_RESTORE: {
		u8 ec_buf[3] = { 0 };
		int err;

		guard(mutex)(&ux5406_lock);
		pr_info("asus_ux5406_battery: pm event %lu, restoring %d%%\n",
			mode, cached_end_threshold);
		err = __ux5406_set_threshold(cached_end_threshold);
		if (err)
			pr_warn("asus_ux5406_battery: failed to restore threshold %d%% after resume: %d\n",
				cached_end_threshold, err);
		if (ux5406_ec_read_rsoc(ec_buf) == 0)
			pr_info("asus_ux5406_battery: EC after restore: %02x %02x %02x\n",
				ec_buf[0], ec_buf[1], ec_buf[2]);
		break;
	}
	default:
		break;
	}
	return 0;
}

static struct notifier_block ux5406_pm_nb = {
	.notifier_call = ux5406_pm_notify,
};

static int __init ux5406_battery_init(void)
{
	u32 rsoc;

	if (!asus_wmi_get_devstate_dsts(ASUS_WMI_DEVID_RSOC, &rsoc)) {
		pr_info("asus_ux5406_battery: asus-wmi already supports RSOC, not loading\n");
		return -ENODEV;
	}

	battery_hook_register(&ux5406_battery_hook);
	register_pm_notifier(&ux5406_pm_nb);
	return 0;
}

static void __exit ux5406_battery_exit(void)
{
	unregister_pm_notifier(&ux5406_pm_nb);
	battery_hook_unregister(&ux5406_battery_hook);
}

module_init(ux5406_battery_init);
module_exit(ux5406_battery_exit);

MODULE_AUTHOR("OpenCode");
MODULE_DESCRIPTION("ASUS Zenbook S14 UX5406AA Battery Charge Threshold Driver");
MODULE_LICENSE("GPL");
MODULE_VERSION("1.1.1");
MODULE_IMPORT_NS("ASUS_WMI");
