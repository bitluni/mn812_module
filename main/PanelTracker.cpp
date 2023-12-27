#include "PanelTracker.h"
#include "sounds.h"
#include "audbrd.h"
#include <math.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stdio.h>
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
const int LIST_FILES = 8;
bool fileMode = false;

extern "C" int listFiles(char *filenames, const char *suffix);
extern "C" void* loadFile(int index, const char *suffix, int *psize);
void showFiles()
{
	char fileNames[13 * 16];
	int count = listFiles(fileNames, "MOD");
	for(int i = 0; i < 16; i++)
	{
		int col = i >> 2;
		int cel = i & 3;
		if(i < count)
		{
			audbrd_chardisp_set(col * 4 + cel, &fileNames[i * 13]);
		}
		else
		{
			audbrd_chardisp_set(col * 4 + cel, "    ");
		}
	}
}

void ev_cb(int type, int which, int val) {
	//printf("Ev %d which %d val %d\n", type, which, val);
	/*if (type==AUDBRD_EVT_BTN)
	{
		char s[12];
		sprintf(s, "%i", which);
		audbrd_chardisp_set(0, s);
	}*/
	if(type==AUDBRD_EVT_BTN && which >= 80)
	{
		/*char s[12];
		sprintf(s, "%i", val);
		audbrd_chardisp_set(0, s);*/
		int i = which - 80;
		if(val == 0)
		{
			if(panelTracker.touch[i] != STATE_ON)
			{		
				panelTracker.touchSampleIndex[i] = 0;
				panelTracker.touch[i] = STATE_ON;
			}
		}
		else
		{
			if(panelTracker.touch[i] != STATE_OFF)
				panelTracker.touch[i] = STATE_RELEASE;
		}
	}

	if (type==AUDBRD_EVT_BTN && val) 
	{
		if(fileMode)
		{
			if(which == LIST_FILES)
			{
				fileMode = false;
				panelTracker.updateDisplay();
				audbrd_btn_led_set(LIST_FILES, 0);
			}
			if((which & 15) < 4)
			{
				if(panelTracker.track) delete(panelTracker.track);
				panelTracker.track = 0;
				panelTracker.currentPattern = 0;
				panelTracker.currentCell = 0;
				int size = -1;
				void* file = loadFile((which & 3) + (which >> 4) * 4, "MOD", &size);
				if(file)
				{
					Track *track = ModFile::load(file, size);
					free(file);
					panelTracker.track = track;
					fileMode = false;
					panelTracker.updateDisplay();
					//char s[12];
					//sprintf(s, "%i", size);
					//audbrd_chardisp_set(0, s);					
				}
				else
				{
					if(size == -1)
						audbrd_chardisp_set(0, "fil?");
					else
					{
						char s[12];
						sprintf(s, "%i", size);
						audbrd_chardisp_set(0, s);
					}
				}
			}
		}
		else
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
			if(which == LIST_FILES)
			{
				panelTracker.stop();
				showFiles();
				fileMode = true;
				audbrd_btn_led_set(LIST_FILES, 1);
			}
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
	panelTracker.init(samplingRate);
	panelTracker.updateDisplay();
	xTaskCreate(trackerTask, "tracker", 20000, 0, 5, NULL);
}