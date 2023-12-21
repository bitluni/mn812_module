//Simple demo program
/*
 * ----------------------------------------------------------------------------
 * "THE BEER-WARE LICENSE" (Revision 42):
 * Jeroen Domburg <jeroen@spritesmods.com> wrote this file. As long as you retain 
 * this notice you can do whatever you want with this stuff. If we meet some day, 
 * and you think this stuff is worth it, you can buy me a beer in return. 
 * ----------------------------------------------------------------------------
 */

#include <errno.h>
#include <dirent.h>
#include <sys/stat.h>
#include <inttypes.h>
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/sdm.h"
#include "driver/gptimer.h"
#include "esp_partition.h"
#include "esp_check.h"
#include "tinyusb.h"
#include "tusb_msc_storage.h"
#include "massstorage.h"

void panelTrackerInit(int samplingRate);
void panelTrackerNextSample(int *left, int *right);

volatile int16_t audio[2] = {0};
int pattern[16][2] = {{0, 0}}; //vol, freq
const int samplingRate = 24000;
int samplesPerCell = samplingRate * 60 / 120 / 4;
uint64_t sampleIndex = 0;
int cellSampleIndex = 0;
int patternSampleIndex = 0;
int button[64] = {0};
int knob[16] = {0};
int patternIndex = 0;

gptimer_handle_t sampleTimer = 0;

int8_t nextSample()
{
//	return (int)(sin(((M_PI * 2 / samplingRate) * sampleIndex * pattern[patternIndex][0])) * (127 * 100 / 100));
	/*if(pattern[patternIndex][0] > 111 - 4)
	{
		int i = 111 - 4;
		return soundsSamples[soundsOffsets[i] + patternSampleIndex];
	}
	else
		return sinTab[((sampleIndex * noteFrequencies[pattern[patternIndex][0]]) / samplingRate) & 1023] >> 8;
	*/
	return 128;
//	return sinTab[(1024 * (int)(sampleIndex * pattern[patternIndex][0]) / samplingRate) & 1023] >> 8;
}

sdm_channel_handle_t audioSDMChannel[2] = {0};

bool nextSampleCb(gptimer_handle_t timer, const gptimer_alarm_event_data_t *edata, void *user_ctx)
{
	//int s = nextSample();
	int right = 0, left = 0;
	static int prevLeft = 0, prevRight = 0;
	panelTrackerNextSample(&left, &right);
	int newLeft = (prevLeft * 15 + left) >> 4;
	int newRight = (prevRight * 15 + right) >> 4;
	if(newLeft < -128) newLeft = -128;
	if(newRight < -128) newRight = -128;
	if(newLeft > 127) newLeft = 127;
	if(newRight > 127) newRight = 127;
	sdm_channel_set_duty(audioSDMChannel[0], newLeft);
	sdm_channel_set_duty(audioSDMChannel[1], newRight);
	prevLeft = newLeft;
	prevRight = newRight;
	return true;
}

void initSound()
{
	sdm_config_t config = {
		.clk_src = SDM_CLK_SRC_DEFAULT,
		.sample_rate_hz = 48000*256,
		.gpio_num = 6,
	};
	ESP_ERROR_CHECK(sdm_new_channel(&config, &audioSDMChannel[0]));
	sdm_channel_enable(audioSDMChannel[0]);
	config.gpio_num = 7;
	ESP_ERROR_CHECK(sdm_new_channel(&config, &audioSDMChannel[1]));
	sdm_channel_enable(audioSDMChannel[1]);

	gptimer_config_t timer_config = {
		.clk_src = GPTIMER_CLK_SRC_DEFAULT,
		.direction = GPTIMER_COUNT_UP,
		.resolution_hz = samplingRate /** 16*/ * 24,
	};
	ESP_ERROR_CHECK(gptimer_new_timer(&timer_config, &sampleTimer));

	gptimer_alarm_config_t sample_config = {
		.reload_count = 0,
		.alarm_count = 16,
		.flags.auto_reload_on_alarm = true,
	};
	ESP_ERROR_CHECK(gptimer_set_alarm_action(sampleTimer, &sample_config));
	gptimer_event_callbacks_t cbs = {
		.on_alarm = nextSampleCb,
	};
	gptimer_register_event_callbacks(sampleTimer, &cbs, 0);
	gptimer_enable(sampleTimer);
	gptimer_start(sampleTimer);
}

void app_main(void) 
{
	initMassstorage();
	panelTrackerInit(samplingRate);
	initSound();
}
