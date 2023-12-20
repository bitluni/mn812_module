#pragma once
#include "Tracker.h"

/*extern "C" void panelTrackerInit(int samplingRate);
extern "C" void panelTrackerNextSample(int *left, int *right);
*/
class PanelTracker : public Tracker
{
	public:
	PanelTracker();
	void updateDisplay();
	void init(int samplingRate);
	void handleInput(int type, int which, int val);
	protected:
};
