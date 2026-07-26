#pragma once
#include "RenderExample.h"

class AlphaBlendingRenderer final
{
public:
	static bool Supports(RenderExample example);
	int Run() const;
};
