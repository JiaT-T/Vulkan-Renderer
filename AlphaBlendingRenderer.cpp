#include "AlphaBlendingRenderer.h"
#include "RenderFeatureRenderer.h"

bool AlphaBlendingRenderer::Supports(RenderExample example)
{
	return example == RenderExample::AlphaComparison;
}

int AlphaBlendingRenderer::Run() const
{
	return RunRenderFeatureRenderer(RenderExample::AlphaComparison, HdrPreference::Auto);
}
