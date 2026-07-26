#include "ColorOutputRenderer.h"
#include "RenderFeatureRenderer.h"

ColorOutputRenderer::ColorOutputRenderer(RenderExample example, HdrPreference hdrPreference)
	: example(example), hdrPreference(hdrPreference)
{
}

bool ColorOutputRenderer::Supports(RenderExample example)
{
	return example == RenderExample::SRGBTest || example == RenderExample::SDRToneMapping ||
		example == RenderExample::HDR;
}

int ColorOutputRenderer::Run() const
{
	return RunRenderFeatureRenderer(example, hdrPreference);
}
