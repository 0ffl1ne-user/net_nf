#include "bus_sh.h"
#include <systemd/sd-bus.h>

int notify(const char* dsc)
{
	sd_bus* bus;
	if(sd_bus_open_user(&bus) < 0)
	{
		return -1;
	}	
	
	sd_bus_message* msg;
	if(sd_bus_message_new_method_call(bus, &msg, "org.freedesktop.Notifications", "/org/freedesktop/Notifications", "org.freedesktop.Notifications", "Notify") < 0)
	{
		sd_bus_message_unref(msg);
		sd_bus_unref(bus);
		return -1;
	}
	
	int ret = sd_bus_message_append(msg, "s", "") < 0;
	ret |= sd_bus_message_append(msg, "u", 0) < 0;
	ret |= sd_bus_message_append(msg, "s", "") < 0;
	ret |= sd_bus_message_append(msg, "s", dsc) < 0;
	ret |= sd_bus_message_append(msg, "s", "") < 0;
	ret |= sd_bus_message_open_container(msg, 'a', "s") < 0;
	ret |= sd_bus_message_close_container(msg) < 0;
	ret |= sd_bus_message_open_container(msg, 'a', "{sv}") < 0;
	ret |= sd_bus_message_close_container(msg) < 0;
	ret |= sd_bus_message_append(msg, "i", -1) < 0;

	sd_bus_error err = SD_BUS_ERROR_NULL;
	if(sd_bus_call(bus, msg, 1000, &err, NULL) < 0)
	{
		ret |= 1;
		sd_bus_error_free(&err);
	}

	sd_bus_message_unref(msg);
	sd_bus_unref(bus);
	return ret;
}

