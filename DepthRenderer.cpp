#include "DepthRenderer.h"
#include "RenderFeatureRenderer.h"

DepthRenderer::DepthRenderer(RenderExample example)
	: example(example)
{
}

bool DepthRenderer::Supports(RenderExample example)
{
	return example == RenderExample::DepthTest || example == RenderExample::DepthDisabled ||
		example == RenderExample::DepthRaw || example == RenderExample::DepthLinear;
}

int DepthRenderer::Run() const
{
	return RunRenderFeatureRenderer(example, HdrPreference::Auto);
}
