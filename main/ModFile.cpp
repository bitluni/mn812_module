#include "ModFile.h"
#include <string.h>


Track *ModFile::load(const void *data, int length)
{
	Track *track = new Track();
	//parse
	const char *d = (const char*)data;
	int p = 0;
	char name[21] = {0};
	strcpy(name, &d[p]);
	p += 20;
	//instrument info
	int sampleLength[31];
	int repeatStart[31];
	int repeatLength[31];
	for(int i = 0; i < 31; i++)
	{
		char sampleName[23] = {0};
		strcpy(sampleName, &d[p]);
		p += 22;
		sampleLength[i] = (((int)((unsigned char)d[p]) << 8) + (unsigned char)d[p + 1]) * 2;
		p += 2;
		int fineTune = d[p++]; //SNibble
		int volume = (unsigned char)d[p++];
		repeatStart[i] = (((int)((unsigned char)d[p]) << 8) + (unsigned char)d[p + 1]) * 2;
		p += 2;
		repeatLength[i] = (((int)((unsigned char)d[p]) << 8) + (unsigned char)d[p + 1]) * 2;
		p += 2;

		track->instruments[i].sampleCount = sampleLength[i];
		track->instruments[i].loopStart = repeatStart[i];
		track->instruments[i].loopLength = repeatLength[i];
	}
	//track
	int trackLength = (unsigned char)d[p++];
	int trackLoopIndex = (unsigned char)d[p++];	//>=127 -> does not loop
	unsigned char song[128] = {0};
	int patternCount = 0;
	track->arrangementLength = trackLength;
	track->arrangementLoopStart = trackLoopIndex;
	for(int i = 0; i < 128; i++)
	{
		song[i] = (unsigned char)d[p++];
		if(patternCount < song[i] + 1)
			patternCount = song[i] + 1;
		track->arrangement[i] = song[i];
	}
	track->patternCount = patternCount;
	track->patterns = new Pattern[patternCount];
	//tag
	unsigned long tag = *(unsigned long*)&d[p];
	p += 4;
	int channelCount = 0;
	int instrumentCount = 0;
	switch(tag)
	{

		case 0x2e4b2e4d:
		default:
			channelCount = 4;
			instrumentCount = 31;
		break;
	} 
	//cell is 4 bytes
	//pattern data
	for(int i = 0; i < patternCount; i++)
		for(int r = 0; r < 64; r++)
			for(int c = 0; c < channelCount; c++)
			{
				//4 bytes per cell
				int b1 = (unsigned char)d[p++];
				int b2 = (unsigned char)d[p++];
				int b3 = (unsigned char)d[p++];
				int b4 = (unsigned char)d[p++];
				int sampleNumber =  (b1 & 0xf0) | (b3 >> 4);
				int period = ((b1 & 0xf) << 8) | b2;
				int effect = ((b3 & 0xf) << 8) | b4;
				//period to midi note
				int period2note[12*5] = {
					1712, 1616, 1525, 1440, 1357, 1281, 1209, 1141, 1077, 1017, 961, 907,
					856, 808, 762, 720, 678, 640, 604, 570, 538, 508, 480, 453,
					428, 404, 381, 360, 339, 320, 302, 285, 269, 254, 240, 226,
					214, 202, 190, 180, 170, 160, 151, 143, 135, 127, 120, 113,
					107, 101,  95,  90,  85,  80,  76,  71,  67,  64,  60,  57};
				int note = 0;
				if(sampleNumber > 0)
					for(int n = 0; n < sizeof(period2note); n++)
						if(period2note[n] == period)
						{
							note = n - 12;// + 24;
							break;
						}
				track->patterns[i].columns[c].cells[r].note = note;
				track->patterns[i].columns[c].cells[r].instrument = sampleNumber;
			}
	//sample data
	for(int i = 0; i < 31; i++)
	{
		if(track->instruments[i].sampleCount)
		{
			track->instruments[i].samples = new signed char[track->instruments[i].sampleCount];
			for(int j = 0; j < track->instruments[i].sampleCount; j++)
			{
				//j < 2 might need to be ignored because of AMIGA
				int s = d[p++];
				track->instruments[i].samples[j] = s;
			}
		}
	}
	return track;
}
