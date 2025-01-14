#pragma once

#include <GLCore.h>
#include <GLCoreUtils.h>

class custom_error : public std::runtime_error {
	using std::runtime_error::runtime_error;
};

enum class UniformType
{
	FLOAT,
	COLOR,
	BOOL
};

struct Uniform
{
	std::string name;
	std::string displayName;
	UniformType type;
	bool update;

	Uniform(std::string_view name, std::string_view displayName, UniformType type, bool update)
		: name(name), displayName(displayName), type(type), update(update) {}

	virtual void UpdateToShader(GLuint shader) = 0;
};

struct FloatUniform : public Uniform
{
	glm::vec2 range;
	float default_val;
	float val;
	float speed;

	FloatUniform(std::string_view name, std::string_view displayName, const glm::vec2& range, float default_val, float speed, bool update)
		: Uniform(name, displayName, UniformType::FLOAT, update), range(range), default_val(default_val), val(default_val), speed(speed) {}

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

	ColorUniform(std::string_view name, std::string_view displayName, const glm::vec3& default_color, bool update)
		: Uniform(name, displayName, UniformType::COLOR, update), color(default_color), default_color(default_color) {}

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

	BoolUniform(std::string_view name, std::string_view displayName, bool default_val, bool update)
		: Uniform(name, displayName, UniformType::BOOL, update), val(default_val), default_val(default_val) {}

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

	ColorFunction(std::string_view src, std::string_view name);
	ColorFunction(std::string_view name);
	ColorFunction(const ColorFunction& other);
	~ColorFunction();


	static const std::shared_ptr<ColorFunction> Default;

	void Initialize(std::string_view src);

	void UpdateUniformsToShader(GLuint shader) const;

	const std::vector<Uniform*>& GetUniforms() const { return m_uniforms; }

	const std::string& GetName() const { return m_name; }
	const std::string& GetSource() const { return m_src;  }
};