#pragma once

#include <GLCore.h>
#include <GLCoreUtils.h>

enum class UniformType
{
	FLOAT,
	COLOR
};

struct Uniform
{
	std::string name;
	UniformType type;
	bool update;

	Uniform(const std::string& name, UniformType type, bool update)
		: name(name), type(type), update(update) {}

	virtual void UpdateToShader(GLuint shader) = 0;
};

struct FloatUniform : public Uniform
{
	glm::vec2 range;
	float default_val;
	float val;
	float speed;

	FloatUniform(const std::string& name, const glm::vec2& range, float default_val, float speed, bool update)
		: Uniform(name, UniformType::FLOAT, update), range(range), default_val(default_val), val(default_val), speed(speed) {}

	void UpdateToShader(GLuint shader) override
	{
		glUseProgram(shader);
		GLint location = glGetUniformLocation(shader, name.c_str());
		glUniform1f(location, val);
	}
};

struct ColorUniform : public Uniform
{
	glm::vec3 color;
	glm::vec3 default_color;

	ColorUniform(const std::string& name, const glm::vec3& default_color, bool update)
		: Uniform(name, UniformType::COLOR, update), color(default_color), default_color(default_color) {}

	void UpdateToShader(GLuint shader) override
	{
		glUseProgram(shader);
		GLint location = glGetUniformLocation(shader, name.c_str());
		glUniform3f(location, color.r, color.g, color.b);
	}
};

class ColorFunction
{
private:
	std::vector<Uniform*> m_uniforms;
	std::string m_src;

public:

	ColorFunction(const std::string& src);
	~ColorFunction();

	void UpdateUniformsToShader(GLuint shader) const;

	std::vector<Uniform*>& GetUniforms();

	const std::vector<Uniform*>& GetUniforms() const;

	const std::string& GetSource() const;
};