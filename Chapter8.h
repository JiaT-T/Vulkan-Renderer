#pragma once
#include <string_view>

enum class RenderExample
{
	Forward,
	Offscreen,
	DepthTest,
	DepthDisabled,
	DepthRaw,
	DepthLinear,
	Deferred,
	DeferredAlbedo,
	DeferredNormal,
	DeferredPosition,
	AlphaComparison,
	SRGBTest,
	SDRToneMapping,
	HDR
};

enum class HdrPreference
{
	Auto,
	ForceSDR,
	RequestHDR
};

bool ParseRenderExample(std::string_view argument, RenderExample& example);
bool ParseHdrPreference(std::string_view argument, HdrPreference& preference);
const char* RenderExampleName(RenderExample example);
int RunChapter8Example(RenderExample example, HdrPreference hdrPreference);
