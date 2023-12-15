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
	return -(T)cos(IM_PI * x) * (T)0.5 + (T)0.5;
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

	Update();
}

#if 0
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
#endif

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

#if 0
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
#endif

glm::dvec2 Hermite(const KeyFrame<CenterKey>& p0, const KeyFrame<CenterKey>& p1, double t)
{
	t = t - p0.t;
	double t1 = p1.t - p0.t;

	glm::dvec2 a = (2.0 * (p0.val.pos - p1.val.pos) + t1 * (p0.val.vel + p1.val.vel)) / std::pow(t1, 3);
	glm::dvec2 b = -(3.0 * (p0.val.pos - p1.val.pos) + t1 * (2.0 * p0.val.vel + p1.val.vel)) / std::pow(t1, 2);
	glm::dvec2 c = p0.val.vel;
	glm::dvec2 d = p0.val.pos;

	return a * t * t * t + b * t * t + c * t + d;
}
 
double HermiteLength(const KeyFrame<CenterKey>& p0, const KeyFrame<CenterKey>& p1)
{
	constexpr int segments = 1000;
	double sum = 0.0;
	for (int i = 0; i <= segments; i++)
	{
		double t = map(i / (double)segments, 0.0, 1.0, p0.t, p1.t);
		double t1 = map((i + 1) / (double)segments, 0.0, 1.0, p0.t, p1.t);

		auto p = Hermite(p0, p1, t);
		auto q = Hermite(p0, p1, t1);

		sum += glm::length(q - p);
	}
	return sum;
}

#if 0
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

double CDCatmullRomInterp(const KeyFrameList<double>& keys, double t)
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
			auto p0 = i > 0 ? *keys[i - 1] : KeyFrame<double>(2.0 * p1.t - p2.t, 2.0 * p1.val - p2.val);
			auto p3 = i < keys.size() - 2 ? *keys[i + 2] : KeyFrame<double>(2.0 * p2.t - p1.t, 2.0 * p2.val - p1.val);

			auto v0 = 0.5 * (p2.val - p0.val);
			auto v1 = 0.5 * (p3.val - p1.val);

			return CDHermite(p1, p2, v0, v1, p1.val/p2.val, t);
		}
	}
	assert(false && "YO WTF?");
	return double();
}
#endif

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


// TODO: 
//  - interpolate speed
//  - give more control for radius interpolation

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

double VideoRenderer::GetRadius(double t) const
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

double VideoRenderer::GetRadiusDiff(double t) const
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

void VideoRenderer::Update()
{
	m_SegmentsLength.clear();
	m_SegmentsLength.reserve(centerKeyFrames.size() - 1);
	for (int i = 1; i < centerKeyFrames.size(); i++)
	{
		auto a = centerKeyFrames[i - 1];
		auto b = centerKeyFrames[i];
		m_SegmentsLength.push_back(HermiteLength(*a, *b));
	}
}


glm::dvec2 VideoRenderer::GetCenter(double t)
{
	double dt = t - m_CurrentT;
	m_CurrentT = t;

	auto& center = centerKeyFrames;

	if (t < center.front()->t)
		return center.front()->val.pos;

	if (t > center.back()->t)
		return center.back()->val.pos;

	double sign = dt >= 0 ? 1 : -1;
	auto a = center[m_CenterSegment];
	auto b = center[m_CenterSegment + 1];
	while (m_CurrentT > b->t)
	{
		m_CenterSegment++;
		a = center[m_CenterSegment];
		b = center[m_CenterSegment + 1];
		m_CurrentLocalT = a->t;
		m_CurrentS = 0.0;
	}
	while (m_CurrentT < a->t)
	{
		m_CenterSegment--;
		a = center[m_CenterSegment];
		b = center[m_CenterSegment + 1];
		m_CurrentLocalT = b->t;
		m_CurrentS = m_SegmentsLength[m_CenterSegment];
	}

	const double local_t = map(m_CurrentT, a->t, b->t, 0.0, 1.0);
	const double r0 = GetRadius(a->t);
	const double r1 = GetRadius(b->t);


	const double r = r1 / r0;
	double interp = exp_interp(r, local_t);
	interp = map(GetRadius(m_CurrentT), r0, r1, 0.0, 1.0);
	const double def_t = map(interp, 0.0, 1.0, a->t, b->t);
	
	const double total_length = m_SegmentsLength[m_CenterSegment];
	const double target_length = map(interp, 0.0, 1.0, 0.0, total_length);

	
	double step = 1e-4;
	auto prev = Hermite(*a, *b, m_CurrentLocalT);
	while (m_CurrentS * sign < target_length * sign)
	{
		m_CurrentLocalT += step * sign;
		auto curr = Hermite(*a, *b, m_CurrentLocalT);
		m_CurrentS += glm::length(curr - prev) * sign;
		prev = curr;
	}
	return prev;
}

void VideoRenderer::UpdateIter(double t)
{
	auto new_radius = GetRadius(t);
	fract->SetRadius(new_radius);

	auto new_center = GetCenter(t);
	fract->SetCenter(new_center);

	for (auto& [u, keys] : uniformsKeyFrames)
	{
		auto new_val = CatmullRomInterp(keys, t);
		u->val = new_val;
	}

	glm::dvec2 cValue = {
		cCenter.x + cAmplitude * cos(2.0 * IM_PI * t), 
		cCenter.y + cAmplitude * sin(2.0 * IM_PI * t) 
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
