#pragma once
#include "RenderExample.h"

class ColorOutputRenderer final
{
public:
	ColorOutputRenderer(RenderExample example, HdrPreference hdrPreference);

	static bool Supports(RenderExample example);
	int Run() const;

private:
	RenderExample example;
	HdrPreference hdrPreference;
};
