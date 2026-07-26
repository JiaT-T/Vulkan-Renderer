#pragma once
#include "RenderExample.h"

class DepthRenderer final
{
public:
	explicit DepthRenderer(RenderExample example);

	static bool Supports(RenderExample example);
	int Run() const;

private:
	RenderExample example;
};
