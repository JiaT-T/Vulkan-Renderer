#pragma once
#include "RenderExample.h"

class DeferredRenderer final
{
public:
	explicit DeferredRenderer(RenderExample example);

	static bool Supports(RenderExample example);
	int Run() const;

private:
	RenderExample example;
};
