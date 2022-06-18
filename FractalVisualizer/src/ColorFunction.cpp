#include "ColorFunction.h"

using namespace GLCore;
using namespace GLCore::Utils;

static std::string uniform_s = "#uniform";

ColorFunction::ColorFunction(const std::string& src) 
	: m_src(src)
{
	for (size_t start; (start = m_src.find(uniform_s + ' ')) != std::string::npos;)
	{
		size_t end = m_src.substr(start).find_first_of(';') + start;

		std::stringstream ss(m_src.substr(start + uniform_s.size(), end - start - uniform_s.size()));

		Uniform* uniform = nullptr;
		std::string uniform_glsl_text;

		std::string type, name;
		ss >> type >> name;
		if (type == "float")
		{
			uniform_glsl_text = "uniform float " + name;

			std::string min_s, max_s;
			float val, speed;
			ss >> val >> speed >> min_s >> max_s;

			glm::vec2 range;
			range.x = (min_s == "NULL" ? FLT_MIN : std::stof(min_s));
			range.y = (max_s == "NULL" ? FLT_MAX : std::stof(max_s));

			uniform = new FloatUniform(name, range, val, speed, true);
		}
		else if (type == "color")
		{
			uniform_glsl_text = "uniform vec3 " + name;

			glm::vec3 def_color;
			ss >> def_color.r >> def_color.g >> def_color.b;

			uniform = new ColorUniform(name, def_color, true);
		}
		else
		{
			LOG_ERROR("Uniform type `{0}' is not valid", type);
			exit(EXIT_FAILURE);
		}

		m_src.erase(start, end - start);
		m_src.insert(start, uniform_glsl_text);

		if (ss.rdbuf()->in_avail() > 0)
		{
			std::string update_s;
			ss >> update_s;
			if (update_s == "false")
				uniform->update = false;
		}

		m_uniforms.push_back(uniform);
	}
}

ColorFunction::~ColorFunction()
{
	for (auto u : m_uniforms)
		delete u;
}

void ColorFunction::UpdateUniformsToShader(GLuint shader) const
{
	for (auto uniform : m_uniforms)
		uniform->UpdateToShader(shader);
}

std::vector<Uniform*>& ColorFunction::GetUniforms()
{
	return m_uniforms;
}

const std::vector<Uniform*>& ColorFunction::GetUniforms() const
{
	return m_uniforms;
}

const std::string& ColorFunction::GetSource() const
{
	return m_src;
}
