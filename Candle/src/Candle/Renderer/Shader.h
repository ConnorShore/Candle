#pragma once

#include "Candle/Core.h"

#include <string>
#include <unordered_map>

namespace Candle {
	
	class CANDLE_API Shader
	{
	public:
		virtual ~Shader() { };

		virtual void Bind() const = 0;
		virtual void Unbind() const = 0;

		static Ref<Shader> Create(const std::string& name, const std::string& vertexSrc, const std::string& fragmentSrc);
		static Ref<Shader> Create(const std::string& filePath);

		virtual const std::string& GetName() const { return _name; }

	protected:
		std::string _name;
	};

	class CANDLE_API ShaderLibrary
	{
	public:
		void Add(const std::string& name, const Ref<Shader>& shader);
		void Add(const Ref<Shader>& shader);
		Ref<Shader> Load(const std::string& filePath);
		Ref<Shader> Load(const std::string& name, const std::string& filePath);

		Ref<Shader> Get(const std::string& name);

		bool Exists(const std::string& name) const;

	private:
		std::unordered_map<std::string, Ref<Shader>> _shaders;	// key: name, value: shader
	};
}