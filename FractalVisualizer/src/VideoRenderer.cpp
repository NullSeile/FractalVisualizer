#include "VideoRenderer.h"

#define _USE_MATH_DEFINES
#include <math.h>

#define IMGUI_DEFINE_MATH_OPERATORS
#include <imgui_internal.h>

template<typename T>
T map(const T& x, const T& x0, const T& x1, const T& y0, const T& y1)
{
	return y0 + ((y1 - y0) / (x1 - x0)) * (x - x0);
}

template<typename T>
T sine_interp(const T& x)
{
	return -cos(M_PI * x) * 0.5 + 0.5;
}

template<typename T, typename S>
T lerp(T a, T b, S t)
{
	return b * t + a * (S(1) - t);
}

template<typename T, typename S>
T mult_interp(T a, T b, S t)
{
	return a * std::pow(b / a, t);
}

void VideoRenderer::Prepare(const std::string& path, const FractalVisualizer& other)
{
	fract = std::make_unique<FractalVisualizer>(path);
	fract->SetColorFunction(color);
	fract->SetSmoothColor(other.GetSmoothColor());
	fract->SetFadeThreshold(other.GetFadeThreshold());
	fract->SetIterationsPerFrame(other.GetIterationsPerFrame());
	fract->SetSize(resolution);

	steps = (size_t)std::ceil(fps * duration);

	current_iter = 0;
}

template<typename T, typename F>
T Interpolate(const std::vector<std::shared_ptr<KeyFrame<T>>>& keyFrames, float t, F interp)
{
	// Assuming ordered keyframes
	if (t <= keyFrames.front()->t)
		return keyFrames.front()->val;

	if (t >= keyFrames.back()->t)
		return keyFrames.back()->val;

	for (int i = 0; i < keyFrames.size() - 1; i++)
	{
		auto& a = keyFrames[i];
		auto& b = keyFrames[i + 1];
		if (a->t <= t && t <= b->t)
		{
			float lt = map(t, a->t, b->t, 0.f, 1.f);
			return interp(*keyFrames[i], *keyFrames[i + 1], lt);
		}
	}
}

template<typename T, typename F>
T InterpolateSimple(const std::vector<std::shared_ptr<KeyFrame<T>>>& keyFrames, float t, F interp)
{
	return Interpolate(keyFrames, t, [&interp](const KeyFrame<T>& a, const KeyFrame<T>& b, float t) { return interp(a.val, b.val, t); });
}

glm::dvec2 CenterInterp(const glm::ivec2& resolution, double initial_radius, double current_radius, const KeyFrame<glm::dvec2>& a, const KeyFrame<glm::dvec2>& b, float t)
{
	// Coords of end.center should lerp to the center of the screen
	const auto initial_coords = MapPosToCoords(resolution, initial_radius, a.val, b.val); // Screen coordinates of end.center at t=0
	const auto final_coords = ImVec2{ (float)resolution.x / 2.f, (float)resolution.y / 2.f };
	const auto target_coords = lerp(initial_coords, final_coords, (float)t); // Coordinates end.center should be at the current t

	// Move the center to set the coords of end.center to be target_coords
	const auto target_pos = MapCoordsToPos(resolution, current_radius, a.val, target_coords);
	const auto delta = target_pos - b.val;
	glm::dvec2 new_center = a.val - delta;
	return new_center;
}

void VideoRenderer::UpdateIter(float t)
{
	t = sine_interp(t);

	static const auto GetRadius = [this](float t) {
		return InterpolateSimple(radiusKeyFrames, t, mult_interp<double, float>);
	};
	auto new_radius = GetRadius(t);
	fract->SetRadius(new_radius);

	auto center_interp = [this, new_radius](const KeyFrame<glm::dvec2>& a, const KeyFrame<glm::dvec2>& b, float t) {
		return CenterInterp(resolution, GetRadius(a.t), new_radius, a, b, t);
	};
	auto new_center = Interpolate(centerKeyFrames, t, center_interp);
	fract->SetCenter(new_center);

	for (auto& [u, keys] : uniformsKeyFrames)
	{
		auto new_val = InterpolateSimple(keys, t, lerp<float, float>);
		u->val = new_val;
	}

	for (int f = 0; f < steps_per_frame; f++)
		fract->Update();
}

void VideoRenderer::SetColorFunction(const std::shared_ptr<ColorFunction>& new_color)
{
	color = std::make_shared<ColorFunction>(*new_color);

	uniformsKeyFrames.clear();
	for (auto u : color->GetUniforms())
	{
		if (u->type == UniformType::FLOAT)
		{
			auto p = dynamic_cast<FloatUniform*>(u);
			auto& keyFrames = uniformsKeyFrames.emplace_back();
			keyFrames.first = p;
			keyFrames.second.emplace_back(std::make_shared<KeyFrame<float>>(0.0, p->val));
		}
	}
}

void VideoRenderer::UpdateToFractal()
{
	fract->SetSize(resolution);
	fract->SetColorFunction(color);
}
