#include "OffscreenRenderer.h"
#include "RenderFeatureRenderer.h"

int OffscreenRenderer::Run() const
{
	return RunRenderFeatureRenderer(RenderExample::Offscreen, HdrPreference::Auto);
}
