#pragma once

#include <GLCore.h>
#include <GLCoreUtils.h>

#include "ColorFunction.h"

std::pair<glm::dvec2, glm::dvec2> GetRange(const glm::uvec2& resolution, double radius, const glm::dvec2& center);
ImVec2 MapPosToCoords(const glm::uvec2& resolution, double radius, const glm::dvec2& center, const glm::dvec2& pos);
glm::dvec2 MapCoordsToPos(const glm::uvec2& resolution, double radius, const glm::dvec2& center, const ImVec2& coords);

class FractalVisualizer
{
public:
	FractalVisualizer(const std::string& shaderSrcPath);
	~FractalVisualizer();

	void Update();

	void SetCenter(const glm::dvec2& center);
	glm::dvec2 GetCenter() const { return m_Center; }

	void SetRadius(double radius);
	double GetRadius() const { return m_Radius; }

	void SetSize(const glm::uvec2& size);
	glm::uvec2 GetSize() const { return m_Size; }

	void SetShader(const std::string& shaderSrcPath);

	void SetSetColor(const glm::vec3& setColor);
	glm::vec3 GetSetColor() const { return m_SetColor; };

	void SetColorFunction(const std::shared_ptr<ColorFunction>& colorFunc);
	std::shared_ptr<ColorFunction> GetColorFunction() const { return m_ColorFunction; }

	void SetIterationsPerFrame(int iterationsPerFrame);
	int GetIterationsPerFrame() const { return m_IterationsPerFrame; }

	void SetFadeThreshold(int fadeThreshold);
	int GetFadeThreshold() const { return m_FadeThreshold; }

	void SetMaxEpochs(int maxEpochs);
	int GetMaxEpochs() const { return m_MaxEpochs; }

	void SetSmoothColor(bool smoothColor);
	bool GetSmoothColor() const { return m_SmoothColor; }

	void SetEqExponent(int eqExponent);
	int GetEqExponent() const { return m_EqExponent; }

	//void SetUniform()
	GLuint GetShader() const { return m_Shader; }

	// Starts calculating from scratch
	void ResetRender();

	GLuint GetTexture() const { return m_Texture; }

	ImVec2 MapPosToCoords(const glm::dvec2& pos) const;
	glm::dvec2 MapCoordsToPos(const ImVec2& coords) const;

	std::pair<glm::dvec2, glm::dvec2> GetRange() const;

private:

	void DeleteFramebuffer();
	void CreateFramebuffer();

	// Shoulds
	bool m_ShouldCreateFramebuffer = true;

	// Fractal stuff
	glm::dvec2 m_Center = { 0.0, 0.0 };
	double m_Radius = 1.0;

	glm::uvec2 m_Size = { 1, 1 };
	int m_IterationsPerFrame = 100;
	glm::vec3 m_SetColor = { 0.f, 0.f, 0.f };
	int m_MaxEpochs = 0;
	int m_Frame = 0;
	bool m_SmoothColor = false;
	int m_FadeThreshold = 0;

	int m_EqExponent = 2;

	std::shared_ptr<ColorFunction> m_ColorFunction;

	// Shader
	std::string m_ShaderSrc;
	GLuint m_Shader = 0;

	// Drawing stuff
	GLuint m_FBO, m_Texture;
	GLuint m_InData, m_OutData;
	GLuint m_InIter, m_OutIter;
	GLuint m_QuadVA, m_QuadVB, m_QuadIB;
};

