#ifndef PLATFORM_Q400_H
#define PLATFORM_Q400_H

#include "messagebox.h"

class Q400 : public Platform {
public:
	Q400(GMenu2X *gmenu2x) : Platform(gmenu2x) {
		INFO("Q400");

		rtc = false;
		tvout = false;
		udc = false;
		ext_sd = false;
		hw_scaler = false;
		poweroff = false;

		opk = "armv7";
		mount_point = "/mnt/app";

		system("mount -o remount,async /mnt/app");

		w = 800;
		h = 480;
		bpp = 32;
	};

	uint8_t getMMC() {
		if (FILE *f = fopen("/dev/mmcblk0p1", "r")) {
			fclose(f);
			return MMC_INSERT;
		}
		return MMC_REMOVE;
	}

	void enableTerminal() {
		/* Enable the framebuffer console */
		char c = '1';
		int fd = open("/sys/devices/virtual/vtconsole/vtcon1/bind", O_WRONLY);
		if (fd) {
			write(fd, &c, 1);
			close(fd);
		}

		fd = open("/dev/tty1", O_RDWR);
		if (fd) {
			ioctl(fd, VT_ACTIVATE, 1);
			close(fd);
		}
	}

	int16_t getBattery(bool raw) {
		/*
			/sys/devices/platform/adc-battery/power_supply/ac/online charge status
			/sys/devices/platform/adc-battery/power_supply/battery/capacity" battery capacity
			/sys/devices/platform/adc-battery/power_supply/battery/voltage_now" battery voltage
			/sys/devices/platform/adc-battery/state status ex(gBatVol=4119,gBatCap=79,charge_ok=0,on)
		*/
		int gBatVol = 0;
		int gBatCap = 0;
		int charge_ok = -1;

		if (FILE *f = fopen("/sys/devices/platform/adc-battery/state", "r")) {
			fscanf(f, "gBatVol=%i,gBatCap=%i,charge_ok=%i,%*s", &gBatVol, &gBatCap, &charge_ok);
			fclose(f);
		}
		return ((charge_ok == 0) ? 6 : (gBatCap / (100 / 5)));
	}

	void setBacklight(int val) {
		if (FILE *f = fopen("/sys/class/backlight/backlight/brightness", "a")) {
			fprintf(f, "%0.0f", val * (255.0f / 100.0f)); // fputs(val, f);
			fclose(f);
		}

		if (FILE *f = fopen("/sys/class/graphics/fb0/blank", "a")) {
			fprintf(f, "%d", val <= 0);
			fclose(f);
		}
	}

	int16_t getBacklight() {
		int val = -1;
		if (FILE *f = fopen("/sys/class/backlight/backlight/brightness", "r")) {
			fscanf(f, "%i", &val);
			fclose(f);
		}
		return val * (100.0f / 255.0f);
	}

	void setVolume(int val) {
		char cmd[96];
		val = val * (63.0f / 100.0f);
		sprintf(cmd, "amixer set Master %d; amixer set HDMI %d", val, val);	
		system(cmd);
	}

	string hwPreLinkLaunch() {
		system("mount -o remount,sync /mnt/app");
		return "";
	}
};

#endif /* PLATFORM_Q400_H */
