#pragma once

#include <GLCore.h>

#include "FractalVisualizer.h"

template<typename T>
struct KeyFrame
{
	KeyFrame(float t, T val) : t(t), val(val) {}
	float t;
	T val;
};

template<typename T>
using KeyFrameList = std::vector<std::shared_ptr<KeyFrame<T>>>;

class VideoRenderer
{
public:
	void Prepare(const std::string& path, const FractalVisualizer& other);
	void UpdateIter(float t);
	void SetColorFunction(const std::shared_ptr<ColorFunction>& new_color);
	void UpdateToFractal();

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


	KeyFrameList<double> radiusKeyFrames = {
		std::make_shared<KeyFrame<double>>(0.f, 1.0),
		std::make_shared<KeyFrame<double>>(1.f, 5.880701162833604e-10),
		//std::make_shared<KeyFrame<double>>(0.f, 1.0)
	};
	KeyFrameList<glm::dvec2> centerKeyFrames = {
		std::make_shared<KeyFrame<glm::dvec2>>(0.f, glm::dvec2{ -0.5, 0 }),
		std::make_shared<KeyFrame<glm::dvec2>>(0.27f, glm::dvec2{ -1.1869930090440344, -0.30304765384587895 }),
		std::make_shared<KeyFrame<glm::dvec2>>(0.7f, glm::dvec2{ -1.183855125737139, -0.3007501720841974 }),
		std::make_shared<KeyFrame<glm::dvec2>>(1.f, glm::dvec2{ -1.1838554525327514, -0.3007498470218996 })
		//std::make_shared<KeyFrame<glm::dvec2>>(0.f, glm::dvec2{ 0.0, 0.0 })
	};
	std::vector<std::pair<FloatUniform*, KeyFrameList<float>>> uniformsKeyFrames;
	double cAmplitude = 1e-5;
	glm::dvec2 cCenter = { 0.0, 0.0 };
};