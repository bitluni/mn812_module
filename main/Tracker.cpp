#include "Tracker.h"

#include <math.h>
int noteFrequencies[128] = {0};

Cell::Cell()
{
	note = 0;
	velocity = 0;
	instrument = 0;
}

Instrument::Instrument()
{
	sampleCount = 1024 * 8;
	loopStart = 0;
	loopLength = 0;
	samples = 0;
	/*static short *samples = new short[sampleCount];
	this-> samples = samples;
	for(int i = 0; i < sampleCount; i++)
	{
		samples[i] = (int)(sin(M_PI * 2 / 1024. * i) * 32767);
	}*/
}

Instrument::~Instrument()
{
	delete(samples);
}

Pattern::Pattern()
{
}

Tracker::Tracker()
{
	track = 0;
	samplingRate = 0;
	samplesPerCell = 0;
	samplesPerPattern = 0;
	bpm = 120;
	measure[0] = 4;
	measure[1] = 4;
	for(int i = 0; i < 4; i++)
	{
		currentNote[i] = 0;
		currentInstrument[i] = 0;
	}
}

Tracker::~Tracker()
{
	delete(track);
}

void Tracker::init(int samplingRate)
{
	this->samplingRate = samplingRate;
	samplesPerCell = (samplingRate * 60) / bpm / measure[1];
	samplesPerPattern = samplesPerCell * 64;
	for(int i = 0; i < 128; i++)
	{
		float f = 440. * pow(2, (i - 69.) / 12.);
		if(i >= 12)
			noteFrequencies[i] = (int)(f * 1024);
	}
}

void Tracker::nextSample(int &left, int &right)
{
	left = 0;
	right = 0;
	//tick();?
	if(!playing) return;
	if(!track) return;
	for(int i = 0; i < 4; i++)
	{
		int note = track->patterns[track->arrangement[currentPattern]]
			.columns[i]
			.cells[currentCell]
			.note;

		int instrument = track->patterns[track->arrangement[currentPattern]]
			.columns[i]
			.cells[currentCell]
			.instrument;

		if(currentSampleCell == 0 &&
			/*((note != currentNote[i]) || (instrument != currentInstrument[i])) 
			&&*/ (instrument != 0))
		{
			currentSampleNote[i] = 0;
			currentNote[i] = note;
			if(instrument)
				currentInstrument[i] = instrument;
		}
		if(currentInstrument[i] > 0)
		{
			int ci = currentInstrument[i] - 1;
			int pitchPos = (currentSampleNote[i] * noteFrequencies[currentNote[i]]) / ((samplingRate * samplingRate) / 16500);
			/*if(track->instruments[ci].loopLength && pitchPos > track->instruments[ci].loopStart + track->instruments[ci].loopLength)
			{
				currentSampleNote[i] = (track->instruments[ci].loopStart * samplingRate) / noteFrequencies[currentNote[i]];
				pitchPos = track->instruments[ci].loopStart;
			}*/
			if(pitchPos < track->instruments[ci].sampleCount && pitchPos >= 0 && ci >= 0)
			{
				left += track->instruments[ci]
					.samples[pitchPos];
				right += track->instruments[ci]
					.samples[pitchPos];
			}
			else
			{
				currentInstrument[i] = 0;
				currentSampleNote[i] = 0;
			}
		}
		currentSampleNote[i]++;
	}
	/*left >>= 1;
	right >>= 1;*/

	currentSampleCell++;
	currentSamplePattern++;
	currentSampleTrack++;
	if(currentSampleCell == samplesPerCell)
	{
		currentCell++;
		currentSampleCell = 0;
		if(currentCell == 64)
		{
			currentCell = 0;
			currentSamplePattern = 0;
			currentPattern++;
			if(currentPattern >= track->arrangementLength)
			{
				currentPattern = 0;
			}
		}
	}
}

void Tracker::setCell(int program, int note, int velocity)
{
}

void Tracker::showPattern()
{
}

void Tracker::play()
{
	currentSampleCell = 0;
	playing = true;
}

void Tracker::stop()
{
	playing = false;
	currentSampleCell = 0;
}

Track::Track()
{
	patterns = 0;
	patternCount = 0;
}

Track::~Track()
{
	delete(patterns);
}
