//Simple demo program
/*
 * ----------------------------------------------------------------------------
 * "THE BEER-WARE LICENSE" (Revision 42):
 * Jeroen Domburg <jeroen@spritesmods.com> wrote this file. As long as you retain 
 * this notice you can do whatever you want with this stuff. If we meet some day, 
 * and you think this stuff is worth it, you can buy me a beer in return. 
 * ----------------------------------------------------------------------------
 */

#include <stdio.h>
#include <inttypes.h>
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "audbrd.h"
#include "driver/sdm.h"
#include "driver/gptimer.h"
#include <math.h>
#include "sounds.h"

int sinTab[1024];
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

int noteFrequencies[128] = {0};
const char *noteNameBase[] = {"C-","C#","D-","D#","E-","F-","F#","G-","G#","A-","A#","B-"};
char noteName[128][5] = {{"    "}};
char noteFreqStr[128][5] = {{"    "}};

const int NOTE_MODE = 4;

void ev_cb(int type, int which, int val) {
	//printf("Ev %d which %d val %d\n", type, which, val);
	if (type==AUDBRD_EVT_BTN && val) 
	{
		button[which] = (button[which] + 1) & 3;
		if(which == NOTE_MODE)
		{
			button[NOTE_MODE] &= 1;			
			audbrd_btn_led_set(which, button[which]);
			for(int i = 0; i < 16; i++)			
				if(button[NOTE_MODE])
					audbrd_chardisp_set(i, noteFreqStr[pattern[i][0]]);
				else
					audbrd_chardisp_set(i, noteName[pattern[i][0]]);
		}
	}
	if (type==AUDBRD_EVT_ROTARY) 
	{
		if(val) val += 11;
		knob[which] = val;
		pattern[which][0] = val;
		if(button[NOTE_MODE])
			audbrd_chardisp_set(which, noteFreqStr[val]);
		else
			audbrd_chardisp_set(which, noteName[val]);
		//char buf[32];
		//sprintf(buf, "%d%%", val);
		//audbrd_chardisp_set(which, buf);
	}
}

gptimer_handle_t sampleTimer = 0;

int8_t nextSample()
{
//	return (int)(sin(((M_PI * 2 / samplingRate) * sampleIndex * pattern[patternIndex][0])) * (127 * 100 / 100));
	if(pattern[patternIndex][0] > 111 - 4)
	{
		int i = 111 - 4;
		return soundsSamples[soundsOffsets[i] + patternSampleIndex];
	}
	else
		return sinTab[((sampleIndex * noteFrequencies[pattern[patternIndex][0]]) / samplingRate) & 1023] >> 8;
	
//	return sinTab[(1024 * (int)(sampleIndex * pattern[patternIndex][0]) / samplingRate) & 1023] >> 8;
}

sdm_channel_handle_t audioSDMChannel[2] = {0};

bool nextSampleCb(gptimer_handle_t timer, const gptimer_alarm_event_data_t *edata, void *user_ctx)
{
	int s = nextSample();
	static int ls = 0;
	int ns = (ls * 15 + s) >> 4;
	ls = ns;
	sdm_channel_set_duty(audioSDMChannel[0], ns);
	sdm_channel_set_duty(audioSDMChannel[1], ns);
	sampleIndex++;
	cellSampleIndex++;
	patternSampleIndex++;
	if(cellSampleIndex == samplesPerCell)
	{
		patternIndex = patternIndex + 1;
		if(patternIndex == 16)
		{
			patternIndex = 0;
			patternSampleIndex = 0;
		}
		cellSampleIndex = 0;
	}
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
		.resolution_hz = samplingRate*16,
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

	for(int i = 0; i < 1024; i++)
	{
		sinTab[i] = (int)(sin(M_PI * 2 / 1024. * i) * 32767);
	}
	for(int i = 0; i < 128; i++)
	{
		float f = 440. * pow(2, (i - 69.) / 12.);
		if(i >= 12)
		{
			noteFrequencies[i] = (int)(f * 1024);
			sprintf(noteName[i], "%s%i", noteNameBase[i%12], i/12 - 1);
			if(f < 10)
				sprintf(noteFreqStr[i], "%.2f", f);
			else if(f < 100)
				sprintf(noteFreqStr[i], "%.1f", f);
			else if(f < 1000)
				sprintf(noteFreqStr[i], "%.0f", f);
			else
				sprintf(noteFreqStr[i], "%.1fk", f / 1000);
		}
	}
}

void setRowBtn(int btn, int val)
{
	audbrd_btn_led_set(((btn >> 2) << 4) + (btn & 3), val);
}

void trackerTask(void *param)
{
	static int oldPatternIndex = 0;
	while(1)
	{
		if(patternIndex != oldPatternIndex)
		{
			setRowBtn(oldPatternIndex, 0);
			setRowBtn(patternIndex, 1);
			oldPatternIndex = patternIndex;
		}
		
		vTaskDelay(pdMS_TO_TICKS(10));
	}
}

void app_main(void) 
{
	initSound();
	audbrd_init(ev_cb);
	xTaskCreate(trackerTask, "tracker", 20000, 0, 5, NULL);
}
