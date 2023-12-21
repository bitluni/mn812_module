#pragma once

#ifdef __cplusplus

class Cell
{
	public:
	Cell();
	int instrument;
	int note;
	int velocity;
};

class Instrument
{
	public:
	Instrument();
	~Instrument();
	signed char *samples;
	int sampleCount;
	int loopStart;
	int loopLength;
};

class Column
{
	public:
	int instrument;
	Cell cells[64];
};

class Pattern
{
	public:
	Pattern();
	Column columns[4];
};

class Track
{
	public:
	Track();
	~Track();
	int arrangementLength;
	int arrangementLoopStart;
	int arrangement[127];
	Pattern *patterns;
	int patternCount;
	Instrument instruments[31];
};

class Tracker
{
	public:
	Tracker();
	~Tracker();
	void init(int samplingRate);
	void nextSample(int &left, int &right);
	int samplingRate;
	int samplesPerCell;
	int samplesPerPattern;
	int bpm;
	int measure[2]; //not used
	Track *track;
	int currentNote[4];
	//Pattern *patterns;
	//Instrument *instruments;
	int currentPattern;
	int currentCell;
	int currentSamplePattern;
	int currentSampleCell;
	int currentSampleNote[4];
	int currentInstrument[4];
	int currentSampleTrack;
	void play();
	void stop();
	bool playing;
	protected:
	void setCell(int program, int note, int velocity);
	void showPattern();
};

#endif