#include "DeferredRenderer.h"
#include "RenderFeatureRenderer.h"

DeferredRenderer::DeferredRenderer(RenderExample example)
	: example(example)
{
}

bool DeferredRenderer::Supports(RenderExample example)
{
	return example == RenderExample::Deferred || example == RenderExample::DeferredAlbedo ||
		example == RenderExample::DeferredNormal || example == RenderExample::DeferredPosition;
}

int DeferredRenderer::Run() const
{
	return RunRenderFeatureRenderer(example, HdrPreference::Auto);
}
