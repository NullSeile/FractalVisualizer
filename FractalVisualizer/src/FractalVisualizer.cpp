#include "FractalVisualizer.h"

#include <fstream>
#include <filesystem>


static double map(const double& x, const double& x0, const double& x1, const double& y0, const double& y1)
{
	return y0 + ((y1 - y0) / (x1 - x0)) * (x - x0);
}

static double inv_map(const double& y, const double& y0, const double& y1, const double& x0, const double& x1)
{
	return (x0 * y - x1 * y + x1 * y0 - x0 * y1) / (y0 - y1);
}

FractalVisualizer::FractalVisualizer(const std::string& shaderSrcPath)
{
	SetShader(shaderSrcPath);

	glGenVertexArrays(1, &m_QuadVA);
	glBindVertexArray(m_QuadVA);

	float vertices[] = {
		-1.0f, -1.0f,
		 1.0f, -1.0f,
		 1.0f,  1.0f,
		-1.0f,  1.0f
	};

	glGenBuffers(1, &m_QuadVB);
	glBindBuffer(GL_ARRAY_BUFFER, m_QuadVB);
	glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(float) * 2, 0);

	uint32_t indices[] = { 0, 1, 2, 2, 3, 0 };
	glGenBuffers(1, &m_QuadIB);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_QuadIB);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);
}

FractalVisualizer::~FractalVisualizer()
{
	DeleteFramebuffer();
}

void FractalVisualizer::Update()
{
	if (m_Size.x <= 0 || m_Size.y <= 0)
		return;

	if (m_ShouldCreateFramebuffer)
	{
		m_ShouldCreateFramebuffer = false; 

		DeleteFramebuffer();
		CreateFramebuffer();
		ResetRender();
	}

	// Shader uniforms
	m_ColorFunction->UpdateUniformsToShader(m_Shader);

	glUseProgram(m_Shader);
	GLint location;

	location = glGetUniformLocation(m_Shader, "i_Size");
	glUniform2ui(location, m_Size.x, m_Size.y);

	location = glGetUniformLocation(m_Shader, "i_ItersPerFrame");
	glUniform1ui(location, m_IterationsPerFrame);

	location = glGetUniformLocation(m_Shader, "i_Frame");
	glUniform1ui(location, m_Frame);

	location = glGetUniformLocation(m_Shader, "i_MaxEpochs");
	glUniform1ui(location, m_MaxEpochs);

	location = glGetUniformLocation(m_Shader, "i_SmoothColor");
	glUniform1i(location, m_SmoothColor);

	auto [xRange, yRange] = GetRange();

	location = glGetUniformLocation(m_Shader, "i_xRange");
	glUniform2d(location, xRange.x, xRange.y);

	location = glGetUniformLocation(m_Shader, "i_yRange");
	glUniform2d(location, yRange.x, yRange.y);

	location = glGetUniformLocation(m_Shader, "i_Data");
	glUniform1i(location, 0);

	location = glGetUniformLocation(m_Shader, "i_Iter");
	glUniform1i(location, 1);

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, m_InData);

	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_2D, m_InIter);

	// Draw
	glViewport(0, 0, m_Size.x, m_Size.y);
	glBindFramebuffer(GL_FRAMEBUFFER, m_FBO);

	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	if (m_Frame == 0)
		glDisable(GL_BLEND);

	glBindVertexArray(m_QuadVA);
	glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, nullptr);

	// Copy output buffers into input buffers
	{
		glBindFramebuffer(GL_FRAMEBUFFER, m_FBO);
		glReadBuffer(GL_COLOR_ATTACHMENT1); // Out data

		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, m_InData);

		glCopyTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32UI, 0, 0, m_Size.x, m_Size.y, 0);


		glBindFramebuffer(GL_FRAMEBUFFER, m_FBO);
		glReadBuffer(GL_COLOR_ATTACHMENT2); // Out iter

		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, m_InIter);

		glCopyTexImage2D(GL_TEXTURE_2D, 0, GL_RG32UI, 0, 0, m_Size.x, m_Size.y, 0);
	}
	
	m_Frame++;
}

void FractalVisualizer::SetCenter(const glm::dvec2& center)
{
	m_Center = center;
	ResetRender();
}

void FractalVisualizer::SetRadius(double radius)
{
	m_Radius = radius;
	ResetRender();
}

void FractalVisualizer::SetSize(const glm::uvec2& size)
{
	m_Size = size;
	m_ShouldCreateFramebuffer = true;
}

void FractalVisualizer::SetShader(const std::string& shaderSrcPath)
{
	std::ifstream file(shaderSrcPath);
	m_ShaderSrc = std::string((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());

	if (m_ColorFunction)
		SetColorFunction(m_ColorFunction);

	m_ShouldCreateFramebuffer = true;
}

void FractalVisualizer::SetColorFunction(ColorFunction* const colorFunc)
{
	m_ColorFunction = colorFunc;

	std::string source = m_ShaderSrc;
	size_t color_loc = source.find("#color");
	if (color_loc == std::string::npos)
	{
		LOG_ERROR("Shader does not have '#color'");
		exit(EXIT_FAILURE);
	}
	source.erase(color_loc, 6);
	source.insert(color_loc, m_ColorFunction->GetSource());

	if (m_Shader)
		glDeleteProgram(m_Shader);

	m_Shader = GLCore::Utils::CreateShader(source);
	glUseProgram(m_Shader);

	m_ColorFunction->UpdateUniformsToShader(m_Shader);

	int location = glGetUniformLocation(m_Shader, "i_Data");
	glUniform1i(location, 0);

	location = glGetUniformLocation(m_Shader, "i_Iter");
	glUniform1i(location, 1);

	ResetRender();
}

void FractalVisualizer::SetIterationsPerFrame(int iterationsPerFrame)
{
	m_IterationsPerFrame = iterationsPerFrame;
	ResetRender();
}

void FractalVisualizer::SetMaxEpochs(int maxEpochs)
{
	if (maxEpochs != m_MaxEpochs && ((maxEpochs < m_MaxEpochs && maxEpochs != 0) || m_MaxEpochs == 0))
		ResetRender();

	m_MaxEpochs = maxEpochs;
}

void FractalVisualizer::SetSmoothColor(bool smoothColor)
{
	m_SmoothColor = smoothColor;
	ResetRender();
}

void FractalVisualizer::ResetRender()
{
	m_Frame = 0;
}

std::pair<glm::dvec2, glm::dvec2> FractalVisualizer::GetRange() const
{
	double aspect = (double)m_Size.x / (double)m_Size.y;
	return 
	{
		{ m_Center.x - aspect * m_Radius, m_Center.x + aspect * m_Radius },
		{ m_Center.y - m_Radius, m_Center.y + m_Radius }
	};
}

ImVec2 FractalVisualizer::MapPosToCoords(const glm::dvec2& pos) const
{
	auto [xRange, yRange] = GetRange();
	return
	{
		(float)map(pos.x, xRange.x, xRange.y, 0, m_Size.x),
		(float)map(pos.y, yRange.x, yRange.y, m_Size.y, 0)
	};
}

glm::dvec2 FractalVisualizer::MapCoordsToPos(const ImVec2& coords) const
{
	auto [xRange, yRange] = GetRange();
	return
	{
		map(coords.x, 0, m_Size.x, xRange.x, xRange.y),
		map(coords.y, m_Size.y, 0, yRange.x, yRange.y)
	};
}

void FractalVisualizer::DeleteFramebuffer()
{
	glDeleteFramebuffers(1, &m_FBO);

	GLuint textures[] = { m_Texture, m_InData, m_OutData, m_InIter, m_OutIter };
	glDeleteTextures(IM_ARRAYSIZE(textures), textures);
}

void FractalVisualizer::CreateFramebuffer()
{
	glGenFramebuffers(1, &m_FBO);
	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, m_FBO);

	// Main texture
	glGenTextures(1, &m_Texture);
	glBindTexture(GL_TEXTURE_2D, m_Texture);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, m_Size.x, m_Size.y, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, m_FBO);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_Texture, 0);

	// Out data
	glGenTextures(1, &m_OutData);
	glBindTexture(GL_TEXTURE_2D, m_OutData);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32UI, m_Size.x, m_Size.y, 0, GL_RGBA_INTEGER, GL_UNSIGNED_INT, nullptr);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, m_FBO);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D, m_OutData, 0);

	// Out iter
	glGenTextures(1, &m_OutIter);
	glBindTexture(GL_TEXTURE_2D, m_OutIter);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RG32UI, m_Size.x, m_Size.y, 0, GL_RG_INTEGER, GL_UNSIGNED_INT, nullptr);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, m_FBO);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT2, GL_TEXTURE_2D, m_OutIter, 0);

	glBindFramebuffer(GL_FRAMEBUFFER, m_FBO);
	GLenum bufs[] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1, GL_COLOR_ATTACHMENT2 };
	glDrawBuffers(IM_ARRAYSIZE(bufs), bufs);

	if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
	{
		LOG_ERROR("Failed to create fractal framebuffer ({0}, {1})", m_Size.x, m_Size.y);
		exit(EXIT_FAILURE);
	}

	glGenTextures(1, &m_InData);
	glBindTexture(GL_TEXTURE_2D, m_InData);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32UI, m_Size.x, m_Size.y, 0, GL_RGBA_INTEGER, GL_UNSIGNED_INT, nullptr);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

	glGenTextures(1, &m_InIter);
	glBindTexture(GL_TEXTURE_2D, m_InIter);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RG32UI, m_Size.x, m_Size.y, 0, GL_RG_INTEGER, GL_UNSIGNED_INT, nullptr);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
}
