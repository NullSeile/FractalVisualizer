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

class VideoRenderer
{
public:
	void Prepare(const std::string& path, const FractalVisualizer& other);
	void UpdateIter(float t);
	void SetColorFunction(const std::shared_ptr<ColorFunction>& new_color);


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


	std::vector<std::shared_ptr<KeyFrame<double>>> radiusKeyFrames = {
		std::make_shared<KeyFrame<double>>(0.f, 1.0)
	};
	std::vector<std::shared_ptr<KeyFrame<glm::dvec2>>> centerKeyFrames = {
		std::make_shared<KeyFrame<glm::dvec2>>(0.f, glm::dvec2{ 0.0, 0.0 })
	};
	std::vector<std::pair<FloatUniform*, std::vector<std::shared_ptr<KeyFrame<float>>>>> uniformsKeyFrames;
};