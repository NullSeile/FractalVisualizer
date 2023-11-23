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

template<typename T, typename S>
T sin_mult_interp(T a, T b, S t)
{
	return mult_interp(a, b, sine_interp(t));
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
	t -= p1.t;
	const S t1 = p2.t - p1.t;

	const T& r0 = p0.val;
	const T& r1 = p1.val;
	const T& r2 = p2.val;
	const T& r3 = p3.val;

	const T a = -((r0 + r1 - r2 - r3) * t1 - S(4) * r1 + S(4) * r2) / (S(2) * t1 * t1 * t1);
	const T b = ((S(2) * r0 + r1 - S(2) * r2 - r3) * t1 - S(6) * r1 + S(6) * r2) / (S(2) * t1 * t1);
	const T c = (r2 - r0) / S(2);
	const T d = r1;

	return a * t * t * t + b * t * t + c * t + d;
}

template<typename T, typename S>
T CatmullRomDiff(const KeyFrame<T>& p0, const KeyFrame<T>& p1, const KeyFrame<T>& p2, const KeyFrame<T>& p3, S t)
{
	t -= p1.t;
	const S t1 = p2.t - p1.t;

	const T& r0 = p0.val;
	const T& r1 = p1.val;
	const T& r2 = p2.val;
	const T& r3 = p3.val;

	const T a = -((r0 + r1 - r2 - r3) * t1 - S(4) * r1 + S(4) * r2) / (S(2) * t1 * t1 * t1);
	const T b = ((S(2) * r0 + r1 - S(2) * r2 - r3) * t1 - S(6) * r1 + S(6) * r2) / (S(2) * t1 * t1);
	const T c = (r2 - r0) / S(2);
	//const T d = r1;

	return S(3) * a * t * t + S(2) * b * t + c;
}

template<typename T, typename S>
T LogCatmullRom(KeyFrame<T> p0, KeyFrame<T> p1, KeyFrame<T>& p2, KeyFrame<T> p3, S t)
{
	p0.val = std::log(p0.val);
	p1.val = std::log(p1.val);
	p2.val = std::log(p2.val);
	p3.val = std::log(p3.val);
	return std::exp(CatmullRom(p0, p1, p2, p3, t));
}

template<typename T, typename S>
T LogCatmullRomDiff(KeyFrame<T> p0, KeyFrame<T> p1, KeyFrame<T>& p2, KeyFrame<T> p3, S t)
{
	p0.val = std::log(p0.val);
	p1.val = std::log(p1.val);
	p2.val = std::log(p2.val);
	p3.val = std::log(p3.val);
	return std::exp(CatmullRom(p0, p1, p2, p3, t)) * CatmullRomDiff(p0, p1, p2, p3, t);
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

glm::dvec2 HermiteRD(const KeyFrame<glm::dvec2>& p0, const KeyFrame<glm::dvec2>& p1, const glm::dvec2& v0, const glm::dvec2& v1, double t, double dr0_ = 0.0, double dr1_ = 0.0)
{
	t = t - p0.t;
	double t1 = p1.t - p0.t;

	glm::dvec2 dr0 = glm::dvec2(dr0_);
	glm::dvec2 dr1 = glm::dvec2(dr1_);

	glm::dvec2 a = (t1 * (v0 + v1 - dr0 - dr1) + 2.0 * (p0.val - p1.val)) / (t1 * t1 * t1);
	glm::dvec2 b = -(t1 * (2.0 * (v0 - dr0) + v1 - dr1) + 3.0 * (p0.val - p1.val)) / (t1 * t1);
	glm::dvec2 c = v0 - dr0;
	glm::dvec2 d = p0.val;

	return a * t * t * t + b * t * t + c * t + d;
}

//template<typename T, typename S>
glm::dvec2 CDHermite(const KeyFrame<glm::dvec2>& p0, const KeyFrame<glm::dvec2>& p1, const glm::dvec2& v0, const glm::dvec2& v1, double r, double t, double dr0 = 0.0, double dr1 = 0.0)
{
	if (r == 1.0)
		return HermiteRD(p0, p1, v0, v1, t, dr0, dr1);
	
	t = t - p0.t;
	double t1 = p1.t - p0.t;

	glm::dvec2 a = ((r * r - r) * t1 * v0 + (r - 1) * t1 * v1 + 2.0 * (p0.val - p1.val) * r * log(r) - (r-1.0)*t1*dr1 - (r*r * 2.0 * dr0 - r * dr0)*t1) / (r * t1 * t1 * t1 * log(r));
	glm::dvec2 b = -(2.0 * (r * r - r) * t1 * v0 + (r - 1) * t1 * v1 + 3.0 * (p0.val - p1.val) * r * log(r) - (r-1.0)*t1*dr1 - 2.0 * (r*r * dr0 - r * dr0)*t1) / (r * t1 * t1 * log(r));
	glm::dvec2 c = ((r - 1.0) * v0 - r * dr0 + dr0) / log(r);
	glm::dvec2 d = p0.val;

	return a * t * t * t + b * t * t + c * t + d;
}

//double CDCatmullRomInterp(const KeyFrameList<double>& keys, double t)
//{
//	// Assuming ordered keyframes
//	if (t <= keys.front()->t)
//		return keys.front()->val;
//
//	if (t >= keys.back()->t)
//		return keys.back()->val;
//
//	for (int i = 0; i < keys.size() - 1; i++)
//	{
//		auto p1 = *keys[i + 0];
//		auto p2 = *keys[i + 1];
//
//		if (p1.t <= t && t <= p2.t)
//		{
//			auto p0 = i > 0 ? *keys[i - 1] : KeyFrame<double>(2.0 * p1.t - p2.t, 2.0 * p1.val - p2.val);
//			auto p3 = i < keys.size() - 2 ? *keys[i + 2] : KeyFrame<double>(2.0 * p2.t - p1.t, 2.0 * p2.val - p1.val);
//
//			auto v0 = 0.5 * (p2.val - p0.val);
//			auto v1 = 0.5 * (p3.val - p1.val);
//
//			return CDHermite(p1, p2, v0, v1, p1.val/p2.val, t);
//		}
//	}
//	assert(false && "YO WTF?");
//	return double();
//}

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
			auto p0 = i > 0 ? *keys[i - 1] : KeyFrame<T>(S(2) * p1.t - p2.t, S(2) * p1.val - p2.val);
			auto p3 = i < keys.size() - 2 ? *keys[i + 2] : KeyFrame<T>(S(2) * p2.t - p1.t, S(2) * p2.val - p1.val);
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

double VideoRenderer::GetRadius(double t)
{
	auto& keys = radiusKeyFrames;

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
			static auto LogProject = [](double v1, double v2) -> double { return exp(2. * std::log(v1) - std::log(v2)); };
			auto p0 = i > 0 ? *keys[i - 1] : KeyFrame<double>(2. * p1.t - p2.t, LogProject(p1.val, p2.val));
			auto p3 = i < keys.size() - 2 ? *keys[i + 2] : KeyFrame<double>(2.f * p2.t - p1.t, LogProject(p2.val, p1.val));
			return LogCatmullRom(p0, p1, p2, p3, t);
		}
	}
	assert(false && "YO WTF?");
	return 0.0;
}

double VideoRenderer::GetRadiusDiff(double t)
{
	auto& keys = radiusKeyFrames;

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
			static auto LogProject = [](double v1, double v2) -> double { return exp(2. * std::log(v1) - std::log(v2)); };
			auto p0 = i > 0 ? *keys[i - 1] : KeyFrame<double>(2. * p1.t - p2.t, LogProject(p1.val, p2.val));
			auto p3 = i < keys.size() - 2 ? *keys[i + 2] : KeyFrame<double>(2.f * p2.t - p1.t, LogProject(p2.val, p1.val));
			return LogCatmullRomDiff(p0, p1, p2, p3, t);
		}
	}
	assert(false && "YO WTF?");
	return 0.0;
}

glm::dvec2 VideoRenderer::GetCenter(double t)
{
	auto& center = centerKeyFrames;

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
		return center.front()->val;

	if (t >= center.back()->t)
		return center.back()->val;

	for (int i = 0; i < center.size() - 1; i++)
	{
		auto a = center[i];
		auto b = center[i + 1];

		if (a->t <= t && t <= b->t)
		{
			const double local_t = map(t, a->t, b->t, 0.0, 1.0);

			const double r0 = GetRadius(a->t);
			const double r1 = GetRadius(b->t);

			const double r = r1 / r0;
			const double interp = exp_interp(r, local_t);
			const double def_t = map(interp, 0.0, 1.0, a->t, b->t);

			const auto c1 = *center[i + 0];
			const auto c2 = *center[i + 1];

			// Projected points to calculate velocity at points
			//const auto p0 = i > 0 ? *proj[i - 1] : KeyFrame<glm::dvec2>(2.0 * c1.t - c2.t, 2.0 * c1.val - c2.val);
			//const auto p1 = *proj[i + 0];
			//const auto p2 = *proj[i + 1];
			//const auto p3 = i < proj.size() - 2 ? *proj[i + 2] : KeyFrame<glm::dvec2>(2.0 * c2.t - c1.t, 2.0 * c2.val - c1.val);
			const auto p0 = i > 0 ? *center[i - 1] : KeyFrame<glm::dvec2>(2.0 * c1.t - c2.t, 2.0 * c1.val - c2.val);
			const auto p1 = *center[i + 0];
			const auto p2 = *center[i + 1];
			const auto p3 = i < center.size() - 2 ? *center[i + 2] : KeyFrame<glm::dvec2>(2.0 * c2.t - c1.t, 2.0 * c2.val - c1.val);

			const auto v0 = glm::normalize(p2.val - p0.val) * r0 * 10.0;
			const auto v1 = glm::normalize(p3.val - p1.val) * r1 * 10.0;

			const auto dr0 = 0.0; //GetRadiusDiff(a->t);
			const auto dr1 = 0.0; //GetRadiusDiff(b->t);

			//return CDHermite(c1, c2, v0, v1, 1.0, def_t, dr0, dr1);
			return CDHermite(c1, c2, v0, v1, r, def_t, dr0, dr1);
		}
	}
}

void VideoRenderer::UpdateIter(double t)
{
	//t = sine_interp(t);

	/*
	static const auto GetRadius = [this](float t) {

		static const auto toLog = [](const KeyFrame<double>& key) -> KeyFrame<double> {
			auto out = key;
			out.val = std::log(key.val);
			return out;
		};

		auto& keys = radiusKeyFrames;

		// Assuming ordered keyframes
		if (t <= keys.front()->t)
			return keys.front()->val;

		if (t >= keys.back()->t)
			return keys.back()->val;

		for (int i = 0; i < keys.size() - 1; i++)
		{
			auto p1 = toLog(*keys[i + 0]);
			auto p2 = toLog(*keys[i + 1]);

			if (p1.t <= t && t <= p2.t)
			{
				auto p0 = i > 0 ? toLog(*keys[i - 1]) : KeyFrame<double>(2.0 * p1.t - p2.t, 2.0 * p1.val - p2.val);
				auto p3 = i < keys.size() - 2 ? toLog(*keys[i + 2]) : KeyFrame<double>(2.f * p2.t - p1.t, 2.0 * p2.val - p1.val);
				return std::exp(CatmullRom(p0, p1, p2, p3, t));
			}
		}
		assert(false && "YO WTF?");
		return 0.0;

		//return CDCatmullRomInterp(radiusKeyFrames, t);
		//return InterpolateSimple(radiusKeyFrames, t, sin_mult_interp<double, float>);
		//return InterpolateSimple(radiusKeyFrames, t, mult_interp<double, float>);
	};*/
	auto new_radius = GetRadius(t);
	fract->SetRadius(new_radius);

	auto new_center = GetCenter(t);
	fract->SetCenter(new_center);

#if 0
	// Center
	{
		auto& center = centerKeyFrames;

		glm::dvec2 new_center(0.0, 0.0);

		//KeyFrameList<ImVec2> coords = { std::make_shared<KeyFrame<ImVec2>>(keyFrames[0]->t, ImVec2(0.f, 0.f))};
		//for (int i = 0; i < keyFrames.size() - 1; i++)
		//{
		//	auto& key0 = *keyFrames[i];
		//	auto& key1 = *keyFrames[i + 1];
		//
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
				const double local_t = map(t, a->t, b->t, 0.0, 1.0);
				
				const double r = GetRadius(b->t) / GetRadius(a->t);
				const double interp = exp_interp(r, local_t);
				const double def_t = map(interp, 0.0, 1.0, a->t, b->t);
				
				// 0 -> Projection Catmul-Rom
				// 1 -> Lineal
				// 2 -> Hermite
				// 3 -> Projection Hermite
				// 4 -> Good Derivative Hermite
#define CENTER_INTERP_METHOD 4
#if CENTER_INTERP_METHOD == 0
				//Projection Catmull-Rom
				const auto projected_center = CatmullRomInterp(proj, def_t);

				const auto r0 = GetRadius(a->t);
				const auto p0 = proj[i];
				new_center = (projected_center - p0->val) * r0 + a->val;

#elif CENTER_INTERP_METHOD == 1
				// Lineal
				//new_center = lerp(a->val, b->val, (double)local_t);
				//new_center = lerp(a->val, b->val, (double)exp_local_t);
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

				//new_center = Hermite(c1, c2, v0, v1, (double)t);
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
#elif CENTER_INTERP_METHOD == 4
				// Good Derivative Hermite
				const auto c1 = *center[i + 0];
				const auto c2 = *center[i + 1];

				// Projected points to calculate velocity at points
				const auto p0 = i > 0 ? *proj[i - 1] : KeyFrame<glm::dvec2>(2.0 * c1.t - c2.t, 2.0 * c1.val - c2.val);
				const auto p1 = *proj[i + 0];
				const auto p2 = *proj[i + 1];
				const auto p3 = i < proj.size() - 2 ? *proj[i + 2] : KeyFrame<glm::dvec2>(2.0 * c2.t - c1.t, 2.0 * c2.val - c1.val);

				const double r0 = GetRadius(c1.t);
				const double r1 = GetRadius(c2.t);

				auto v0 = glm::normalize(p2.val - p0.val) * r0 * 5.0;
				auto v1 = glm::normalize(p3.val - p1.val) * r1 * 5.0;
				
				new_center = CDHermite(c1, c2, v0, v1, r, def_t);
#endif
			}
		}

		//LOG_INFO("{");
		//for (auto c : coords)
		//	LOG_INFO("  ({}, ui::Vec2f{{{}, {}}}),", c->t, c->val.x, c->val.y);
		//LOG_INFO("}");
		fract->SetCenter(new_center);
	}
#endif

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
