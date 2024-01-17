#include "ColorFunction.h"

using namespace GLCore;
using namespace GLCore::Utils;

static std::string uniform_s = "#uniform";

const std::shared_ptr<ColorFunction> ColorFunction::Default = std::make_shared<ColorFunction>(
	"vec3 get_color(float i) { return vec3(1); }",
	"default"
);

ColorFunction::ColorFunction(std::string_view name)
	: m_name(name)
{
}

ColorFunction::ColorFunction(std::string_view src, std::string_view name) 
	: m_name(name)
{
	Initialize(src);
}

void ColorFunction::Initialize(std::string_view src)
{
	m_src = src;

	for (size_t start; (start = m_src.find(uniform_s + ' ')) != std::string::npos;)
	{
		size_t end = m_src.substr(start).find_first_of(';') + start;

		std::stringstream ss(m_src.substr(start + uniform_s.size(), end - start - uniform_s.size()));

		Uniform* uniform = nullptr;
		std::string uniform_glsl_text;

		std::string type, name, displayName;
		ss >> type >> name >> std::quoted(displayName);

		//LOG_INFO("{1} - {0}", ss.str(), displayName);

		if (type == "float")
		{
			uniform_glsl_text = "uniform float " + name;

			std::string min_s, max_s;
			float val, speed;
			ss >> val >> speed >> min_s >> max_s;

			glm::vec2 range;
			range.x = (min_s == "NULL" ? -FLT_MAX : std::stof(min_s));
			range.y = (max_s == "NULL" ? FLT_MAX : std::stof(max_s));

			uniform = new FloatUniform(name, displayName, range, val, speed, true);
		}
		else if (type == "color")
		{
			uniform_glsl_text = "uniform vec3 " + name;

			glm::vec3 def_color;
			ss >> def_color.r >> def_color.g >> def_color.b;

			uniform = new ColorUniform(name, displayName, def_color, true);
		}
		else if (type == "bool")
		{
			uniform_glsl_text = "uniform bool " + name;

			bool def_val;
			ss >> std::boolalpha >> def_val;

			uniform = new BoolUniform(name, displayName, def_val, true);
		}
		else
		{
			LOG_ERROR("Uniform type `{0}` is not valid", type);
			throw custom_error(std::format("Uniform type `{0}` is not valid", type));
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

ColorFunction::ColorFunction(const ColorFunction& other)
	: m_name(other.m_name), m_src(other.m_src)
{
	m_uniforms.reserve(other.m_uniforms.size());
	for (auto u : other.m_uniforms)
	{
		Uniform* uniform = nullptr;
		switch (u->type)
		{
		case UniformType::FLOAT:
		{
			auto p = dynamic_cast<FloatUniform*>(u);
			uniform = new FloatUniform(*p);
			break;
		}
		case UniformType::COLOR:
		{
			auto p = dynamic_cast<ColorUniform*>(u);
			uniform = new ColorUniform(*p);
			break;
		}
		case UniformType::BOOL:
		{
			auto p = dynamic_cast<BoolUniform*>(u);
			uniform = new BoolUniform(*p);
			break;
		}
		default:
			break;
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
