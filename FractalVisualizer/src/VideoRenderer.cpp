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

static inline ImVec2 operator*(const float scalar, const ImVec2& vec) { return vec * scalar; }

template<typename T>
T CatmullRom(const KeyFrame<T>& p0, const KeyFrame<T>& p1, const KeyFrame<T>& p2, const KeyFrame<T>& p3, double t)
{
	t -= p1.t;
	const double t1 = p2.t - p1.t;

	const T& r0 = p0.val;
	const T& r1 = p1.val;
	const T& r2 = p2.val;
	const T& r3 = p3.val;

	const T a = (T)(-((r0 + r1 - r2 - r3) * t1 - 4.0 * r1 + 4.0 * r2) / (2.0 * t1 * t1 * t1));
	const T b = (T)(((2.0 * r0 + r1 - 2.0 * r2 - r3) * t1 - 6.0 * r1 + 6.0 * r2) / (2.0 * t1 * t1));
	const T c = (T)((r2 - r0) / 2.0);
	const T d = (T)r1;

	return (T)(a * t * t * t + b * t * t + c * t + d);
}

template<typename T>
T CatmullRomInterp(const KeyFrameList<T>& keys, double t)
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
			auto p0 = i > 0 ? *keys[i - 1] : KeyFrame<T>(2.0 * p1.t - p2.t, T(2.0 * p1.val) - p2.val);
			auto p3 = i < keys.size() - 2 ? *keys[i + 2] : KeyFrame<T>(2.0 * p2.t - p1.t, T(2.0 * p2.val) - p1.val);
			return CatmullRom(p0, p1, p2, p3, t);
		}
	}
	assert(false && "YO WTF?");
	return T();
}

template<typename T>
T Hermite(const KeyFrame<T>& p0, const KeyFrame<T>& p1, const T& v0, const T& v1, double t)
{
	t = t - p0.t;
	double t1 = p1.t - p0.t;

	T a = (2.0 * (p0.val - p1.val) + t1 * (v0 + v1)) / std::pow(t1, 3);
	T b = -(3.0 * (p0.val - p1.val) + t1 * (2.0 * v0 + v1)) / std::pow(t1, 2);
	T c = v0;
	T d = p0.val;

	return a * t * t * t + b * t * t + c * t + d;
}

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

void VideoRenderer::Prepare(std::filesystem::path path, const FractalVisualizer& other)
{
	fract = std::make_unique<FractalVisualizer>(path);
	fract->SetColorFunction(color);
	fract->SetSmoothColor(other.GetSmoothColor());
	fract->SetFadeThreshold(other.GetFadeThreshold());
	fract->SetIterationsPerFrame(other.GetIterationsPerFrame());
	fract->SetSize(resolution);

	steps = (size_t)std::ceil(fps * duration);

	current_iter = 0;

	Invalidate();
	// Update();
}

void VideoRenderer::Invalidate()
{
	InvalidateCenter();
	InvalidateRadius();
}

void VideoRenderer::InvalidateCenter()
{
	m_CurrentT = 0.0;
	m_CurrentLocalT = centerKeyFrames.front()->t;
	m_CurrentS = 0.0;
	m_CenterSegment = 0;

	m_SegmentsLength.clear();
	m_SegmentsLength.reserve(centerKeyFrames.size() - 1);
	for (int i = 1; i < centerKeyFrames.size(); i++)
	{
		auto a = centerKeyFrames[i - 1];
		auto b = centerKeyFrames[i];
		m_SegmentsLength.push_back(HermiteLength(*a, *b));
	}
}

void VideoRenderer::InvalidateRadius(double dt)
{
	m_RadiusIntegPoints.clear();

	double acc = 0.0;
	const int samples = (int)(1.0 / dt);
	dt = 1.0 / (double)(samples - 1);
	m_RadiusIntegPoints.reserve(samples);
	for (int i = 0; i < samples; i++)
	{
		const double t = i / (double)(samples - 1);
		acc += GetRadius(t) * dt;
		m_RadiusIntegPoints.push_back(acc);
	}
}

double VideoRenderer::GetRadius(double t) const
{
	assert(0.0 <= t && t <= 1.0);

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
			static auto LogProject = [](double v1, double v2) -> double { return exp(2.0 * std::log(v1) - std::log(v2)); };
			auto p0 = i > 0 ? *keys[i - 1] : KeyFrame(2.0 * p1.t - p2.t, LogProject(p1.val, p2.val));
			auto p3 = i < keys.size() - 2 ? *keys[i + 2] : KeyFrame(2.0 * p2.t - p1.t, LogProject(p2.val, p1.val));

			p0.val = std::log(p0.val);
			p1.val = std::log(p1.val);
			p2.val = std::log(p2.val);
			p3.val = std::log(p3.val);

			double v0 = (p2.val - p0.val) / (p2.t - p0.t);
			double v1 = (p3.val - p1.val) / (p3.t - p1.t);

			return std::exp(Hermite(p1, p2, v0, v1, t));
		}
	}
	assert(false && "YO WTF?");
	return 0.0;
}

double VideoRenderer::GetRadiusInteg(double t) const
{
	assert(0.0 <= t && t <= 1.0);

	double x = map(t, 0.0, 1.0, 0.0, (double)(m_RadiusIntegPoints.size() - 1));

	if (x == std::floor(x))
		return m_RadiusIntegPoints[(int)x];

	int n = (int)x;
	double lt = x - n;
	double a = m_RadiusIntegPoints[n];
	double b = m_RadiusIntegPoints[n+1];
	return a * (1.0 - lt) + b * lt;
}


glm::dvec2 VideoRenderer::GetCenter(double t)
{
	assert(0.0 <= t && t <= 1.0);

	double dt = t - m_CurrentT;
	m_CurrentT = t;

	auto& center = centerKeyFrames;

	if (t <= center.front()->t)
		return center.front()->val.pos;

	if (t >= center.back()->t)
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
	
	const double total_length = m_SegmentsLength[m_CenterSegment];
	const double target_length = map(
		GetRadiusInteg(t),
		GetRadiusInteg(a->t),
		GetRadiusInteg(b->t),
		0.0, 
		total_length
	);

	
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
	assert(0.0 <= t && t <= 1.0);

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
			keyFrames.second.emplace_back(std::make_shared<KeyFrame<float>>(0.0, p->val));
		}
	}
}

void VideoRenderer::UpdateToFractal()
{
	fract->SetSize(resolution);
	fract->SetColorFunction(color);
}
