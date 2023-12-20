#include "PanelTracker.h"
#include "sounds.h"
#include "audbrd.h"
#include <math.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stdio.h>
#include "zoids.mod.h"
#include "haste.mod.h"
#include "the_main_event.mod.h"
#include "submariners.mod.h"
#include "drama_mix_spirit.mod.h"
#include "aspartam.mod.h"
#include "ModFile.h"

const char *noteNameBase[] = {"C-","C#","D-","D#","E-","F-","F#","G-","G#","A-","A#","B-"};
char noteName[128][5] = {{"    "}};
char noteFreqStr[128][5] = {{"    "}};

PanelTracker panelTracker;

PanelTracker::PanelTracker()
 :Tracker()
{
}

void PanelTracker::init(int samplingRate)
{
	Tracker::init(samplingRate);
	updateDisplay();
}

void PanelTracker::handleInput(int type, int which, int val)
{
}

void PanelTracker::updateDisplay()
{
	for(int cel = 0; cel < 4; cel++)
	{
		char str[16];
		int cellIndex = cel + currentCell;
		if(cellIndex < 64 && track != 0)
		{
			sprintf(str, "%2i  ", cellIndex);
			for(int col = 0; col < 4; col++)
			{
				int n = track->patterns[track->arrangement[currentPattern]]
					.columns[col]
					.cells[cellIndex]
					.note;
				audbrd_chardisp_set(col * 4 + cel, noteName[n]);
				//audbrd_chardisp_set(col * 4 + cel, str);
			}
		}
		else
			for(int col = 0; col < 4; col++)
				audbrd_chardisp_set(col * 4 + cel, "    ");
	}
}

void setRowBtn(int btn, int val)
{
	audbrd_btn_led_set(((btn >> 2) << 4) + (btn & 3), val);
}

const int NOTE_MODE = 4;
const int CELL_UP = 5;
const int CELL_DOWN = 6;

void ev_cb(int type, int which, int val) {
	//printf("Ev %d which %d val %d\n", type, which, val);
	if (type==AUDBRD_EVT_BTN && val) 
	{
		//button[which] = (button[which] + 1) & 3;
		if(which == NOTE_MODE)
		{
			if(panelTracker.playing)
			{
				panelTracker.stop();
				audbrd_btn_led_set(NOTE_MODE, 0);
			}
			else
			{
				panelTracker.play();
				audbrd_btn_led_set(NOTE_MODE, 1);
			}
		}
		if(which == CELL_DOWN && !panelTracker.playing)
		{
			panelTracker.currentCell++;
			if(panelTracker.currentCell == 64)
				panelTracker.currentCell = 0;
		}
		if(which == CELL_UP && !panelTracker.playing)
		{
			panelTracker.currentCell--;
			if(panelTracker.currentCell == -1)
				panelTracker.currentCell = 63;
		}
	}
	if (type==AUDBRD_EVT_ROTARY) 
	{
		int cellIndex = panelTracker.currentCell + (which & 3);
		if(cellIndex < 64 && panelTracker.track != 0)
		{
			panelTracker.track->patterns[panelTracker.currentPattern]
				.columns[which >> 2]
				.cells[cellIndex]
				.note = val;
			panelTracker.updateDisplay();
		}
	/*	if(val) val += 11;
		knob[which] = val;
		pattern[which][0] = val;
		if(button[NOTE_MODE])
			audbrd_chardisp_set(which, noteFreqStr[val]);
		else
			audbrd_chardisp_set(which, noteName[val]);*/
		//char buf[32];
		//sprintf(buf, "%d%%", val);
		//audbrd_chardisp_set(which, buf);
	}
}

void trackerTask(void *param)
{
	static int oldCell = 0;
	while(1)
	{
		if(panelTracker.currentCell != oldCell)
		{
			//setRowBtn(oldPatternIndex, 0);
			//setRowBtn(patternIndex, 1);
			panelTracker.updateDisplay();
			oldCell = panelTracker.currentCell;
		}
		vTaskDelay(pdMS_TO_TICKS(10));
	}
}

void noteNameInit()
{
	for(int i = 0; i < 128; i++)
	{
		float f = 440. * pow(2, (i - 69.) / 12.);
		if(i >= 12)
		{
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

extern "C" void panelTrackerNextSample(int *left, int *right)
{
	panelTracker.nextSample(*left, *right);
}

extern "C" void panelTrackerInit(int samplingRate)
{
	audbrd_init(ev_cb);
	noteNameInit();
//	Track *track = ModFile::load(zoids, sizeof(zoids));
//	Track *track = ModFile::load(haste, sizeof(haste));
//	Track *track = ModFile::load(the_main_event, sizeof(haste));
	Track *track = ModFile::load(submariners, sizeof(haste));
	panelTracker.init(samplingRate);
	panelTracker.track = track;
	panelTracker.updateDisplay();
	xTaskCreate(trackerTask, "tracker", 20000, 0, 5, NULL);
}