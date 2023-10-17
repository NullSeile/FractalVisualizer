#include "VideoRenderer.h"

#define _USE_MATH_DEFINES
#include <math.h>
#include <imgui_internal.h>

template<typename T>
T map(const T& x, const T& x0, const T& x1, const T& y0, const T& y1)
{
	return y0 + ((y1 - y0) / (x1 - x0)) * (x - x0);
}

template<typename T>
T sine_interp(const T& x)
{
	return -(T)cos(M_PI * x) * (T)0.5 + (T)0.5;
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

static inline ImVec2 operator*(const float scalar, const ImVec2& vec) { return vec * scalar; }

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
T Interpolate(const KeyFrameList<T>& keyFrames, float t, F interp)
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
	assert(false && "YO WTF?");
	return T();
}

template<typename T, typename F>
T InterpolateSimple(const KeyFrameList<T>& keyFrames, float t, F interp)
{
	return Interpolate(keyFrames, t, [&interp](const KeyFrame<T>& a, const KeyFrame<T>& b, float t) { return interp(a.val, b.val, t); });
}

template<typename T, typename S>
T CatmullRom(const KeyFrame<T>& p0, const KeyFrame<T>& p1, const KeyFrame<T>& p2, const KeyFrame<T>& p3, S t)
{
	float t0 = p0.t;
	float t1 = p1.t;
	float t2 = p2.t;
	float t3 = p3.t;
	T A1 = (t1 - t) / (t1 - t0) * p0.val + (t - t0) / (t1 - t0) * p1.val;
	T A2 = (t2 - t) / (t2 - t1) * p1.val + (t - t1) / (t2 - t1) * p2.val;
	T A3 = (t3 - t) / (t3 - t2) * p2.val + (t - t2) / (t3 - t2) * p3.val;
	T B1 = (t2 - t) / (t2 - t0) * A1 + (t - t0) / (t2 - t0) * A2;
	T B2 = (t3 - t) / (t3 - t1) * A2 + (t - t1) / (t3 - t1) * A3;
	T C = (t2 - t) / (t2 - t1) * B1 + (t - t1) / (t2 - t1) * B2;
	return C;
}

template<typename T, typename S>
T Hermite(const KeyFrame<T>& p0, const KeyFrame<T>& p1, const T& v0, const T& v1, S t)
{
	t = t - p0.t;
	S t1 = p1.t - p0.t;

	T a = (S(2) * (p0.val - p1.val) + t1 * (v0 + v1)) / std::pow(t1, 3);
	T b = -(S(3) * (p0.val - p1.val) + t1 * (S(2) * v0 + v1)) / std::pow(t1, 2);
	T c = v0;
	T d = p0.val;

	return a * t * t * t + b * t * t + c * t + d;
}

template<typename T, typename S>
T CatmullRomInterp(const KeyFrameList<T>& keys, S t)
{
	// Assuming ordered keyframes
	if (t <= keys.front()->t)
		return keys.front()->val;

	if (t >= keys.back()->t)
		return keys.back()->val;

	for (int i = 0; i < keys.size() - 1; i++)
	{
		auto p1 = *keys[i + 0];
		auto p2 = *keys[i + 1];

		if (p1.t <= t && t <= p2.t)
		{
			auto p0 = i > 0 ? *keys[i - 1] : KeyFrame<T>(2.0 * p1.t - p2.t, 2.0 * p1.val - p2.val);
			auto p3 = i < keys.size() - 2 ? *keys[i + 2] : KeyFrame<T>(2.f * p2.t - p1.t, 2.0 * p2.val - p1.val);
			return CatmullRom(p0, p1, p2, p3, t);
		}
	}
	assert(false && "YO WTF?");
	return T();
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

double exp_interp(double ratio, double t)
{
	if (ratio == 1)
		return t;
	else
		return (std::pow(ratio, t) - 1.0) / (ratio - 1.0);
}

double exp_interp_deriv(double ratio, double t)
{
	if (ratio == 1)
		return 1;
	else
		return std::pow(ratio, t) * std::log(ratio) / (1 - ratio);
}

void VideoRenderer::UpdateIter(float t)
{
	//t = sine_interp(t);

	static const auto GetRadius = [this](float t) {
		return InterpolateSimple(radiusKeyFrames, t, mult_interp<double, float>);
	};
	auto new_radius = GetRadius(t);
	fract->SetRadius(new_radius);

	// Center
	{
		auto& center = centerKeyFrames;

		glm::dvec2 new_center(0.0, 0.0);

		//KeyFrameList<ImVec2> coords = { std::make_shared<KeyFrame<ImVec2>>(keyFrames[0]->t, ImVec2(0.f, 0.f))};
		//for (int i = 0; i < keyFrames.size() - 1; i++)
		//{
		//	auto& key0 = *keyFrames[i];
		//	auto& key1 = *keyFrames[i + 1];

		//	auto delta = MapPosToCoords(resolution, GetRadius(key0.t), key0.val, key1.val) - ImVec2{ resolution.x / 2.f, resolution.y / 2.f };
		//	//LOG_TRACE("({}, {})", delta.x, delta.y);
		//	coords.push_back(std::make_shared<KeyFrame<ImVec2>>(key1.t, ImVec2(coords[i]->val + delta)));
		//}

		KeyFrameList<glm::dvec2> proj;
		proj.push_back(std::make_shared<KeyFrame<glm::dvec2>>(center[0]->t, center[0]->val));
		for (size_t i = 1; i < center.size(); i++)
		{
			const auto c0 = center[i - 1];
			const auto c1 = center[i];
			const auto r0 = GetRadius(c0->t);
			const auto r1 = GetRadius(c1->t);

			glm::dvec2 new_c = (c1->val - c0->val) / r0 + proj[i - 1]->val;
			proj.push_back(std::make_shared<KeyFrame<glm::dvec2>>(c1->t, new_c));
		}

		if (t <= center.front()->t)
			new_center = center.front()->val;

		if (t >= center.back()->t)
			new_center = center.back()->val;

		for (int i = 0; i < center.size() - 1; i++)
		{
			auto& a = center[i];
			auto& b = center[i + 1];
			if (a->t <= t && t <= b->t)
			{
				const double local_t = map(t, a->t, b->t, 0.f, 1.f);

				const double r = radiusKeyFrames.back()->val / radiusKeyFrames.front()->val;
				const double def_t = exp_interp(r, t);
				/*
				const double r = GetRadius(b->t) / GetRadius(a->t);
				const double interp = exp_interp(r, local_t);
				const double def_t = map(interp, 0.0, 1.0, (double)a->t, (double)b->t);
				*/

				// 0 -> Projection Catmul-Rom
				// 1 -> Lineal
				// 2 -> Hermite
				// 3 -> Projection Hermite
#define CENTER_INTERP_METHOD 2
#if CENTER_INTERP_METHOD == 0
				//Projection Catmull-Rom
				const auto projected_center = CatmullRomInterp(proj, def_t);

				const auto r0 = GetRadius(a->t);
				const auto p0 = proj[i];
				new_center = (projected_center - p0->val) * r0 + a->val;

#elif CENTER_INTERP_METHOD == 1
				// Lineal
				new_center = lerp(a->val, b->val, (double)interp);
				

#elif CENTER_INTERP_METHOD == 2
				//Hermite
				const auto c1 = *center[i + 0];
				const auto c2 = *center[i + 1];

				const auto p0 = i > 0 ? *proj[i - 1] : KeyFrame<glm::dvec2>(2.f * c1.t - c2.t, 2.0 * c1.val - c2.val);
				const auto p1 = *proj[i + 0];
				const auto p2 = *proj[i + 1];
				const auto p3 = i < proj.size() - 2 ? *proj[i + 2] : KeyFrame<glm::dvec2>(2.f * c2.t - c1.t, 2.0 * c2.val - c1.val);

				const double r0 = GetRadius(c1.t);
				const double r1 = GetRadius(c2.t);

				auto v0 = glm::normalize(p2.val - p0.val) * r0 * 5.0;
				auto v1 = glm::normalize(p3.val - p1.val) * r1 * 5.0;

				//v1 /= exp_interp_deriv(r, 1.0);

				new_center = Hermite(c1, c2, v0, v1, (double)def_t);

#elif CENTER_INTERP_METHOD == 3
				// Projection Hermite
				//const auto projected_center = CatmullRomInterp(proj, def_t);

				auto p1 = *proj[i + 0];
				auto p2 = *proj[i + 1];

				auto p0 = i > 0 ? *proj[i - 1] : KeyFrame<glm::dvec2>(2.f * p1.t - p2.t, 2.0 * p1.val - p2.val);
				auto p3 = i < proj.size() - 2 ? *proj[i + 2] : KeyFrame<glm::dvec2>(2.f * p2.t - p1.t, 2.0 * p2.val - p1.val);

				auto v0 = (p2.val - p0.val) * 5.0;
				auto v1 = (p3.val - p1.val) * 5.0;

				//if (i >= 1)
				//	v0 *= 

				const auto projected_center = Hermite(p1, p2, v0, v1, (double)def_t);

				const auto r0 = GetRadius(a->t);
				new_center = (projected_center - p1.val) * r0 + a->val;
#endif

				//new_center = CatmullRomInterp(keyFrames, def_t);

				//const auto& p1 = *coords[i];
				//const auto& p2 = *coords[i + 1];
				//const auto p0 = i > 0 ? *coords[i - 1] : KeyFrame<ImVec2>(2.f * p1.t - p2.t, 2.f * p1.val - p2.val);
				//const auto p3 = i < coords.size() - 2 ? *coords[i + 2] : KeyFrame<ImVec2>(2.f * p2.t - p1.t, 2.f * p2.val - p1.val);

				//const float interp_t = map(t, a->t, b->t, b->t, a->t); // Invert the interpolation order
				//const auto target_coords = CatmullRom(p0, p1, p2, p3, interp_t) - coords[i]->val + ImVec2{ resolution.x / 2.f, resolution.y / 2.f };

				//const float interp_t = t; // Invert the interpolation order
				//const auto target_coords = CatmullRomInterp(coords, interp_t) - coords[i]->val + ImVec2{ resolution.x / 2.f, resolution.y / 2.f };

				/*
				// Coords of end.center should lerp to the center of the screen
				const auto initial_coords = MapPosToCoords(resolution, GetRadius(a->t), a->val, b->val); // Screen coordinates of end.center at t=0
				const auto final_coords = ImVec2{ (float)resolution.x / 2.f, (float)resolution.y / 2.f };
				const auto target_coords = lerp(initial_coords, final_coords, (float)t); // Coordinates end.center should be at the current t

				// Move the center to set the coords of end.center to be target_coords
				const auto target_pos = MapCoordsToPos(resolution, GetRadius(t), a->val, target_coords);
				const auto delta = target_pos - b->val;
				new_center = a->val - delta;
				*/

				//const auto final_coords = MapPosToCoords(resolution, GetRadius(b->t), b->val, a->val); // Screen coordinates of a.center at t=1
				//const auto initial_coords = ImVec2{ (float)resolution.x / 2.f, (float)resolution.y / 2.f };
				//const auto target_coords = lerp(initial_coords, final_coords, (float)t); // Coordinates a.center should be at the current t

				// Move the center to set the coords of end.center to be target_coords
				//const auto target_pos = MapCoordsToPos(resolution, GetRadius(t), a->val, target_coords);
				//new_center = target_pos;
				//const auto delta = target_pos - a->val;
				//glm::dvec2 new_center = a.val - delta;
			}
		}

		//LOG_INFO("{");
		//for (auto c : coords)
		//	LOG_INFO("  ({}, ui::Vec2f{{{}, {}}}),", c->t, c->val.x, c->val.y);
		//LOG_INFO("}");

		fract->SetCenter(new_center);
	}

	//auto center_interp = [this, new_radius](const KeyFrame<glm::dvec2>& a, const KeyFrame<glm::dvec2>& b, float t) {
	//	return CenterInterp(resolution, GetRadius(a.t), new_radius, a, b, t);
	//};
	//auto new_center = Interpolate(centerKeyFrames, t, center_interp);
	////auto new_center = InterpolateSimple(centerKeyFrames, t, lerp<glm::dvec2, double>);
	//fract->SetCenter(new_center);

	for (auto& [u, keys] : uniformsKeyFrames)
	{
		auto new_val = CatmullRomInterp(keys, t);
		u->val = new_val;
	}

	glm::dvec2 cValue = 
	{ 
		cCenter.x + cAmplitude * cos(2.0 * M_PI * t), 
		cCenter.y + cAmplitude * sin(2.0 * M_PI * t) 
	};

	glUseProgram(fract->GetShader());
	GLint loc = glGetUniformLocation(fract->GetShader(), "i_JuliaC");
	glUniform2d(loc, cValue.x, cValue.y);

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
			keyFrames.second.emplace_back(std::make_shared<KeyFrame<float>>(0.f, p->val));
		}
	}
}

void VideoRenderer::UpdateToFractal()
{
	fract->SetSize(resolution);
	fract->SetColorFunction(color);
}
