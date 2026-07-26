#include "RenderExample.h"
#include "AlphaBlendingRenderer.h"
#include "ColorOutputRenderer.h"
#include "DeferredRenderer.h"
#include "DepthRenderer.h"
#include "OffscreenRenderer.h"

bool ParseRenderExample(std::string_view argument, RenderExample& example)
{
	if (!argument.starts_with("--example="))
		return false;
	const std::string_view value = argument.substr(10);
	if (value == "forward") example = RenderExample::Forward;
	else if (value == "offscreen") example = RenderExample::Offscreen;
	else if (value == "depth") example = RenderExample::DepthTest;
	else if (value == "depth-off") example = RenderExample::DepthDisabled;
	else if (value == "depth-raw") example = RenderExample::DepthRaw;
	else if (value == "depth-linear") example = RenderExample::DepthLinear;
	else if (value == "deferred") example = RenderExample::Deferred;
	else if (value == "gbuffer-albedo") example = RenderExample::DeferredAlbedo;
	else if (value == "gbuffer-normal") example = RenderExample::DeferredNormal;
	else if (value == "gbuffer-position") example = RenderExample::DeferredPosition;
	else if (value == "alpha") example = RenderExample::AlphaComparison;
	else if (value == "srgb") example = RenderExample::SRGBTest;
	else if (value == "sdr") example = RenderExample::SDRToneMapping;
	else if (value == "hdr") example = RenderExample::HDR;
	else return false;
	return true;
}

bool ParseHdrPreference(std::string_view argument, HdrPreference& preference)
{
	if (!argument.starts_with("--hdr="))
		return false;
	const std::string_view value = argument.substr(6);
	if (value == "auto") preference = HdrPreference::Auto;
	else if (value == "sdr") preference = HdrPreference::ForceSDR;
	else if (value == "request") preference = HdrPreference::RequestHDR;
	else return false;
	return true;
}

const char* RenderExampleName(RenderExample example)
{
	switch (example)
	{
	case RenderExample::Offscreen: return "Offscreen";
	case RenderExample::DepthTest: return "DepthTest";
	case RenderExample::DepthDisabled: return "DepthDisabled";
	case RenderExample::DepthRaw: return "DepthRaw";
	case RenderExample::DepthLinear: return "DepthLinear";
	case RenderExample::Deferred: return "Deferred";
	case RenderExample::DeferredAlbedo: return "DeferredAlbedo";
	case RenderExample::DeferredNormal: return "DeferredNormal";
	case RenderExample::DeferredPosition: return "DeferredPosition";
	case RenderExample::AlphaComparison: return "AlphaComparison";
	case RenderExample::SRGBTest: return "SRGBTest";
	case RenderExample::SDRToneMapping: return "SDRToneMapping";
	case RenderExample::HDR: return "HDR";
	default: return "Forward";
	}
}

int RunRenderExample(RenderExample example, HdrPreference hdrPreference)
{
	if (example == RenderExample::Offscreen)
		return OffscreenRenderer().Run();
	if (DepthRenderer::Supports(example))
		return DepthRenderer(example).Run();
	if (DeferredRenderer::Supports(example))
		return DeferredRenderer(example).Run();
	if (AlphaBlendingRenderer::Supports(example))
		return AlphaBlendingRenderer().Run();
	if (ColorOutputRenderer::Supports(example))
		return ColorOutputRenderer(example, hdrPreference).Run();
	return -1;
}
