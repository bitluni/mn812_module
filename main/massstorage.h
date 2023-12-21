/*
 * SPDX-FileCopyrightText: 2022-2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */

/* DESCRIPTION:
 * This example contains code to make ESP32-S3 based device recognizable by USB-hosts as a USB Mass Storage Device.
 * It either allows the embedded application i.e. example to access the partition or Host PC accesses the partition over USB MSC.
 * They can't be allowed to access the partition at the same time.
 * For different scenarios and behaviour, Refer to README of this example.
 */

#include <errno.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>
//#include "esp_console.h"
#include "esp_check.h"
#include "driver/gpio.h"
#include "tinyusb.h"
#include "tusb_msc_storage.h"
#define CONFIG_EXAMPLE_STORAGE_MEDIA_SPIFLASH
#include <string.h>
#include "tusb_cdc_acm.h"

static const char *TAG = "example_main";

/* TinyUSB descriptors
   ********************************************************************* */
#define EPNUM_MSC       1
#define TUSB_DESC_TOTAL_LEN (TUD_CONFIG_DESC_LEN + TUD_MSC_DESC_LEN)

enum {
    ITF_NUM_MSC = 0,
    ITF_NUM_TOTAL
};

enum {
    EDPT_CTRL_OUT = 0x00,
    EDPT_CTRL_IN  = 0x80,

    EDPT_MSC_OUT  = 0x01,
    EDPT_MSC_IN   = 0x81,
};

static uint8_t const desc_configuration[] = {
    // Config number, interface count, string index, total length, attribute, power in mA
    TUD_CONFIG_DESCRIPTOR(1, ITF_NUM_TOTAL, 0, TUSB_DESC_TOTAL_LEN, TUSB_DESC_CONFIG_ATT_REMOTE_WAKEUP, 100),

    // Interface number, string index, EP Out & EP In address, EP size
    TUD_MSC_DESCRIPTOR(ITF_NUM_MSC, 0, EDPT_MSC_OUT, EDPT_MSC_IN, TUD_OPT_HIGH_SPEED ? 512 : 64),
};

static tusb_desc_device_t descriptor_config = {
    .bLength = sizeof(descriptor_config),
    .bDescriptorType = TUSB_DESC_DEVICE,
    .bcdUSB = 0x0200,
    .bDeviceClass = TUSB_CLASS_MISC,
    .bDeviceSubClass = MISC_SUBCLASS_COMMON,
    .bDeviceProtocol = MISC_PROTOCOL_IAD,
    .bMaxPacketSize0 = CFG_TUD_ENDPOINT0_SIZE,
    .idVendor = 0x303A, // This is Espressif VID. This needs to be changed according to Users / Customers
    .idProduct = 0x4002,
    .bcdDevice = 0x100,
    .iManufacturer = 0x01,
    .iProduct = 0x02,
    .iSerialNumber = 0x03,
    .bNumConfigurations = 0x01
};

static char const *string_desc_arr[] = {
    (const char[]) { 0x09, 0x04 },  // 0: is supported language is English (0x0409)
    "TinyUSB",                      // 1: Manufacturer
    "TinyUSB Device",               // 2: Product
    "123456",                       // 3: Serials
    "Example MSC",                  // 4. MSC
};
/*********************************************************************** TinyUSB descriptors*/

#define BASE_PATH "/data" // base path to mount the partition

#define PROMPT_STR CONFIG_IDF_TARGET
static int console_unmount(int argc, char **argv);
static int console_read(int argc, char **argv);
static int console_write(int argc, char **argv);
static int console_size(int argc, char **argv);
static int console_status(int argc, char **argv);
static int console_exit(int argc, char **argv);
/*const esp_console_cmd_t cmds[] = {
    {
        .command = "read",
        .help = "read BASE_PATH/README.MD and print its contents",
        .hint = NULL,
        .func = &console_read,
    },
    {
        .command = "write",
        .help = "create file BASE_PATH/README.MD if it does not exist",
        .hint = NULL,
        .func = &console_write,
    },
    {
        .command = "size",
        .help = "show storage size and sector size",
        .hint = NULL,
        .func = &console_size,
    },
    {
        .command = "expose",
        .help = "Expose Storage to Host",
        .hint = NULL,
        .func = &console_unmount,
    },
    {
        .command = "status",
        .help = "Status of storage exposure over USB",
        .hint = NULL,
        .func = &console_status,
    },
    {
        .command = "exit",
        .help = "exit from application",
        .hint = NULL,
        .func = &console_exit,
    }
};*/

// mount the partition and show all the files in BASE_PATH
static void _mount(void)
{
    ESP_LOGI(TAG, "Mount storage...");
    ESP_ERROR_CHECK(tinyusb_msc_storage_mount(BASE_PATH));


    // List all the files in this directory
    ESP_LOGI(TAG, "\nls command output:");
    struct dirent *d;
    DIR *dh = opendir(BASE_PATH);
    if (!dh) {
        if (errno == ENOENT) {
            //If the directory is not found
            ESP_LOGE(TAG, "Directory doesn't exist %s", BASE_PATH);
        } else {
            //If the directory is not readable then throw error and exit
            ESP_LOGE(TAG, "Unable to read directory %s", BASE_PATH);
        }
        return;
    }
    //While the next entry is not readable we will print directory files
    while ((d = readdir(dh)) != NULL) {
        printf("%s\n", d->d_name);
    }
    return;
}

// unmount storage
static int console_unmount(int argc, char **argv)
{
    if (tinyusb_msc_storage_in_use_by_usb_host()) {
        ESP_LOGE(TAG, "storage is already exposed");
        return -1;
    }
    ESP_LOGI(TAG, "Unmount storage...");
    ESP_ERROR_CHECK(tinyusb_msc_storage_unmount());
    return 0;
}

// read BASE_PATH/README.MD and print its contents
static int console_read(int argc, char **argv)
{
    if (tinyusb_msc_storage_in_use_by_usb_host()) {
        ESP_LOGE(TAG, "storage exposed over USB. Application can't read from storage.");
        return -1;
    }
    ESP_LOGD(TAG, "read from storage:");
    const char *filename = BASE_PATH "/README.MD";
    FILE *ptr = fopen(filename, "r");
    if (ptr == NULL) {
        ESP_LOGE(TAG, "Filename not present - %s", filename);
        return -1;
    }
    char buf[1024];
    while (fgets(buf, 1000, ptr) != NULL) {
        printf("%s", buf);
    }
    fclose(ptr);
    return 0;
}

// create file BASE_PATH/README.MD if it does not exist
static int console_write(int argc, char **argv)
{
    if (tinyusb_msc_storage_in_use_by_usb_host()) {
        ESP_LOGE(TAG, "storage exposed over USB. Application can't write to storage.");
        return -1;
    }
    ESP_LOGD(TAG, "write to storage:");
    const char *filename = BASE_PATH "/README.MD";
    FILE *fd = fopen(filename, "r");
    if (!fd) {
        ESP_LOGW(TAG, "README.MD doesn't exist yet, creating");
        fd = fopen(filename, "w");
        fprintf(fd, "Mass Storage Devices are one of the most common USB devices. It use Mass Storage Class (MSC) that allow access to their internal data storage.\n");
        fprintf(fd, "In this example, ESP chip will be recognised by host (PC) as Mass Storage Device.\n");
        fprintf(fd, "Upon connection to USB host (PC), the example application will initialize the storage module and then the storage will be seen as removable device on PC.\n");
        fclose(fd);
    }
    return 0;
}

int listFiles(char *filenames, const char *suffix)
{
    tinyusb_msc_storage_mount(BASE_PATH);
    struct dirent *d;
	int count = 0;
    DIR *dh = opendir(BASE_PATH);
    if (!dh) {
        if (errno == ENOENT) 
		{
			strcpy(filenames, "DIR?");
        } else {
			strcpy(filenames, "DIR!");
        }
        return 1;
    }
    while ((d = readdir(dh)) != NULL) 
	{
		int len = strlen(d->d_name);
		if(len < 3) continue;
		char s[4];
		strcpy(s, &d->d_name[len - 3]);
		for(int i = 0; i < 3; i++)
			s[i] &= 0xdf;
		if(!strcmp(s, suffix))
		{
			sprintf(&filenames[13 * count], "%.4s", d->d_name);
			count++;
		}
    }

    tinyusb_msc_storage_unmount();
	return count;
}

void* loadFile(int index, const char *suffix, int *psize)
{
    tinyusb_msc_storage_mount(BASE_PATH);
    struct dirent *d;
	int count = 0;
    DIR *dh = opendir(BASE_PATH);
    if (!dh) return 0;
    while ((d = readdir(dh)) != NULL) 
	{
		int len = strlen(d->d_name);
		if(len < 3) continue;
		char s[4];
		strcpy(s, &d->d_name[len - 3]);
		for(int i = 0; i < 3; i++)
			s[i] &= 0xdf;
		if(!strcmp(s, suffix))
		{
			if(count == index)
			{
				//found it
				struct stat st;          
				char *path = (char*)malloc(strlen(BASE_PATH) + len + 2);
				sprintf(path, "%s/%s", BASE_PATH, d->d_name);
				if(stat(path, &st))
				{
					free(path);
					tinyusb_msc_storage_unmount();
					closedir(dh);
					return 0;
				}
				int size = st.st_size;
				*psize = size;
				void *buf = heap_caps_malloc(size, MALLOC_CAP_SPIRAM);
				//void *buf = malloc(size);
				FILE *ptr = fopen(path, "r");
				if (ptr == 0)			
				{
					free(path);
					tinyusb_msc_storage_unmount();
					closedir(dh);
					return 0;
				}
				*psize = fread(buf, 1, size, ptr); 
				fclose(ptr);
				//path[strlen(path) - 1] = '_';
				//ptr = fopen(path, "w");
				//fwrite(buf, 1, size, ptr);
				//fclose(ptr); 
				free(path);
				closedir(dh);
				tinyusb_msc_storage_unmount();
				return buf;
			}
			count++;
		}
    }
	closedir(dh);
    tinyusb_msc_storage_unmount();
	return 0;
}

static esp_err_t storage_init_spiflash(wl_handle_t *wl_handle)
{
    ESP_LOGI(TAG, "Initializing wear levelling");

    const esp_partition_t *data_partition = esp_partition_find_first(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_FAT, NULL);
    if (data_partition == NULL) {
        ESP_LOGE(TAG, "Failed to find FATFS partition. Check the partition table.");
        return ESP_ERR_NOT_FOUND;
    }

    return wl_mount(data_partition, wl_handle);
}

static uint8_t buf[CONFIG_TINYUSB_CDC_RX_BUFSIZE + 1];

void tinyusb_cdc_rx_callback(int itf, cdcacm_event_t *event)
{
    /* initialization */
    size_t rx_size = 0;

    /* read */
    esp_err_t ret = tinyusb_cdcacm_read(itf, buf, CONFIG_TINYUSB_CDC_RX_BUFSIZE, &rx_size);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "Data from channel %d:", itf);
        ESP_LOG_BUFFER_HEXDUMP(TAG, buf, rx_size, ESP_LOG_INFO);
    } else {
        ESP_LOGE(TAG, "Read error");
    }

    /* write back */
    tinyusb_cdcacm_write_queue(itf, buf, rx_size);
    tinyusb_cdcacm_write_flush(itf, 0);
}

void initMassstorage(void)
{
    ESP_LOGI(TAG, "Initializing storage...");

    static wl_handle_t wl_handle = WL_INVALID_HANDLE;
    ESP_ERROR_CHECK(storage_init_spiflash(&wl_handle));

    const tinyusb_msc_spiflash_config_t config_spi = {
        .wl_handle = wl_handle
    };
    ESP_ERROR_CHECK(tinyusb_msc_storage_init_spiflash(&config_spi));

    //mounted in the app by default
    _mount();
/*
    ESP_LOGI(TAG, "USB MSC initialization");
    const tinyusb_config_t tusb_cfg = {
        .device_descriptor = &descriptor_config,
        .string_descriptor = string_desc_arr,
        .string_descriptor_count = sizeof(string_desc_arr) / sizeof(string_desc_arr[0]),
        .external_phy = false,
        .configuration_descriptor = desc_configuration,
    };
    ESP_ERROR_CHECK(tinyusb_driver_install(&tusb_cfg));
    ESP_LOGI(TAG, "USB MSC initialization DONE");*/
	//tinyusb_config_cdcacm_t acm_cfg = { 0 }; // the configuration uses default values
    //ESP_ERROR_CHECK(tusb_cdc_acm_init(&acm_cfg));

    ESP_LOGI(TAG, "USB Composite initialization");
    const tinyusb_config_t tusb_cfg = {
        .device_descriptor = NULL,
        .string_descriptor = NULL,
        .string_descriptor_count = 0,
        .external_phy = false,
        .configuration_descriptor = NULL,
    };
    ESP_ERROR_CHECK(tinyusb_driver_install(&tusb_cfg));

    tinyusb_config_cdcacm_t acm_cfg = {
        .usb_dev = TINYUSB_USBDEV_0,
        .cdc_port = TINYUSB_CDC_ACM_0,
        .rx_unread_buf_sz = 64,
        .callback_rx = &tinyusb_cdc_rx_callback, // the first way to register a callback
        .callback_rx_wanted_char = NULL,
        .callback_line_state_changed = NULL,
        .callback_line_coding_changed = NULL
    };

    ESP_ERROR_CHECK(tusb_cdc_acm_init(&acm_cfg));

    /*esp_console_repl_t *repl = NULL;
    esp_console_repl_config_t repl_config = ESP_CONSOLE_REPL_CONFIG_DEFAULT();*/
    /* Prompt to be printed before each line.
     * This can be customized, made dynamic, etc.
     */
    /*repl_config.prompt = PROMPT_STR ">";
    repl_config.max_cmdline_length = 64;
    esp_console_register_help_command();
    esp_console_dev_uart_config_t hw_config = ESP_CONSOLE_DEV_UART_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_console_new_repl_uart(&hw_config, &repl_config, &repl));
    for (int count = 0; count < sizeof(cmds) / sizeof(esp_console_cmd_t); count++) {
        ESP_ERROR_CHECK( esp_console_cmd_register(&cmds[count]) );
    }
    ESP_ERROR_CHECK(esp_console_start_repl(repl));*/
}
