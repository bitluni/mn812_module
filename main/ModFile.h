#pragma once

#include "Tracker.h"

class ModFile
{
	public:
	static Track* load(const void *data, int length);
};