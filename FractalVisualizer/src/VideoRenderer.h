#pragma once

#include <GLCore.h>

#include "FractalVisualizer.h"

template<typename T>
struct KeyFrame
{
	KeyFrame(double t, T val) : t(t), val(val) {}
	double t;
	T val;
};

struct CenterKey
{
	glm::dvec2 pos;
	glm::dvec2 vel;
};

template<typename T>
using KeyFrameList = std::vector<std::shared_ptr<KeyFrame<T>>>;

class VideoRenderer
{
public:
	void Prepare(const std::string& path, const FractalVisualizer& other);
	void UpdateIter(double t);
	void SetColorFunction(const std::shared_ptr<ColorFunction>& new_color);
	void UpdateToFractal();
	void Update();

	double GetRadius(double t) const;
	double GetRadiusDiff(double t) const;

	glm::dvec2 GetCenter(double t);

	int m_CenterSegment = 0;
	double m_CurrentT = 0.0;
	double m_CurrentLocalT = 0.0;
	double m_CurrentS = 0.0;

	std::string fileName = "output.mp4";
	std::unique_ptr<FractalVisualizer> fract;
	std::shared_ptr<ColorFunction> color;

	glm::ivec2 resolution = { 1920, 1080 };
	float duration = 10.f;
	int fps = 30;
	int steps_per_frame = 10;

	FILE* ffmpeg;
	BYTE* pixels;

	size_t steps;

	int current_iter = 0;

	std::vector<double> m_SegmentsLength;

	KeyFrameList<double> radiusKeyFrames = {
		//std::make_shared<KeyFrame<double>>(0.f, 1.0),
		//std::make_shared<KeyFrame<double>>(1.f, 5.880701162833604e-10),
		//std::make_shared<KeyFrame<double>>(0.f, 1.0)

		std::make_shared<KeyFrame<double>>(0.0, 1.0),
		std::make_shared<KeyFrame<double>>(0.33, 0.008057857721976197),
		std::make_shared<KeyFrame<double>>(0.433, 0.28782969446188766),
		std::make_shared<KeyFrame<double>>(0.632, 0.006049181474278884),
		std::make_shared<KeyFrame<double>>(0.795, 1.172453080986668e-05),
	};
	KeyFrameList<CenterKey> centerKeyFrames = {
		//std::make_shared<KeyFrame<glm::dvec2>>(0.f, glm::dvec2{ -0.5, 0 }),
		//std::make_shared<KeyFrame<glm::dvec2>>(0.27f, glm::dvec2{ -1.1869930090440344, -0.30304765384587895 }),
		//std::make_shared<KeyFrame<glm::dvec2>>(0.7f, glm::dvec2{ -1.183855125737139, -0.3007501720841974 }),
		//std::make_shared<KeyFrame<glm::dvec2>>(1.f, glm::dvec2{ -1.1838554525327514, -0.3007498470218996 })
		//std::make_shared<KeyFrame<glm::dvec2>>(0.f, glm::dvec2{ 0.0, 0.0 })

		std::make_shared<KeyFrame<CenterKey>>(0, CenterKey{ {-0.5, 0}, {0.0, 0.0} }),
		std::make_shared<KeyFrame<CenterKey>>(0.33, CenterKey{{-1.2558024544068163, 0.38112841375594236}, {0.0, 0.0}}),
		std::make_shared<KeyFrame<CenterKey>>(0.433, CenterKey{{-0.8392324486465885, 0.37356936504006194}, {0.0, 0.0}}),
		std::make_shared<KeyFrame<CenterKey>>(0.632, CenterKey{{-0.5973014418167584, 0.6631019637438973}, {0.0, 0.0}}),
		std::make_shared<KeyFrame<CenterKey>>(0.795, CenterKey{{-0.5952023547186579, 0.6680937984694201}, {0.0, 0.0}}),
	};
	std::vector<std::pair<FloatUniform*, KeyFrameList<float>>> uniformsKeyFrames;
	double cAmplitude = 1e-5;
	glm::dvec2 cCenter = { 0.0, 0.0 };
};