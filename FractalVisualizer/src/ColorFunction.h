#pragma once

#include <GLCore.h>
#include <GLCoreUtils.h>

enum class UniformType
{
	FLOAT,
	COLOR,
	BOOL
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

struct BoolUniform : public Uniform
{
	bool val;
	bool default_val;

	BoolUniform(const std::string& name, bool default_val, bool update)
		: Uniform(name, UniformType::BOOL, update), val(default_val), default_val(default_val) {}

	void UpdateToShader(GLuint shader) override
	{
		glUseProgram(shader);
		GLint location = glGetUniformLocation(shader, name.c_str());
		glUniform1i(location, val ? GL_TRUE : GL_FALSE);
	}
};

class ColorFunction
{
private:
	std::vector<Uniform*> m_uniforms;
	std::string m_src;
	std::string m_name;

public:

	ColorFunction(const std::string& src, const std::string& name);
	ColorFunction(const ColorFunction& other);
	~ColorFunction();

	void UpdateUniformsToShader(GLuint shader) const;

	const std::vector<Uniform*>& GetUniforms() const { return m_uniforms; }

	const std::string& GetName() const { return m_name; }
	const std::string& GetSource() const { return m_src;  }
};