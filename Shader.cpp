#include "Shader.h"

#include "utilities/Logging.h"
#include "utilities/StringManipulation.h"

#include <cstdio>

static GLuint load_shader(GLenum type, const char* filename)
{
	// concatenate file name to base shader path
	char path[128];
	concatenate("resources/shaders/", filename, path);

	// open shader file
	FILE* file = fopen(path, "rb");
	if(file == nullptr)
	{
		LOG_ISSUE("couldn't open shader file: %s", filename);
		return 0;
	}

	// get file size
	fseek(file, 0, SEEK_END);
	long size = ftell(file);
	rewind(file);

	// read shader source code and close file
	GLchar* source_code = new GLchar[size + 1];
	source_code[size] = '\0';
	fread(source_code, 1, size, file);
	fclose(file);

	// create shader, load source code to it, and compile
	GLuint shader = glCreateShader(type);
	glShaderSource(shader, 1, &source_code, nullptr);
	delete[] source_code;
	glCompileShader(shader);

	// output shader errors if compile failed
	GLint status = 0;
	glGetShaderiv(shader, GL_COMPILE_STATUS, &status);
	if(status == GL_FALSE)
	{
		int info_log_length = 0;
		glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &info_log_length);
		if(info_log_length > 0)
		{
			char* info_log = new char[info_log_length];
			int bytes_written = 0;
			glGetShaderInfoLog(shader, info_log_length, &bytes_written, info_log);
			LOG_ISSUE("couldn't compile shader %s\n%s", filename, info_log);
			delete[] info_log;
		}

		glDeleteShader(shader);

		return 0;
	}

	return shader;
}

GLuint load_shader_program(const char* vertex_file, const char* fragment_file)
{
	GLuint vertex_shader = load_shader(GL_VERTEX_SHADER, vertex_file);
	GLuint fragment_shader = load_shader(GL_FRAGMENT_SHADER, fragment_file);

	GLuint program = 0;
	if(vertex_shader != 0 && fragment_shader != 0)
	{
		// create program object and link shaders to it
		program = glCreateProgram();
		glAttachShader(program, vertex_shader);
		glAttachShader(program, fragment_shader);
		glLinkProgram(program);

		// check if linking failed and output any errors
		GLint status = 0;
		glGetProgramiv(program, GL_LINK_STATUS, &status);
		if(status == GL_FALSE)
		{
			int info_log_length = 0;
			glGetProgramiv(program, GL_INFO_LOG_LENGTH, &info_log_length);
			if(info_log_length > 0)
			{
				char* info_log = new char[info_log_length];
				int bytes_written = 0;
				glGetProgramInfoLog(program, info_log_length, &bytes_written, info_log);
				LOG_ISSUE("couldn't link program (%s, %s)\n%s",
					vertex_file, fragment_file, info_log);
				delete[] info_log;
			}

			glDeleteProgram(program);
			program = 0;
		}
		else
		{
			// shaders are no longer needed after program object is linked
			glDetachShader(program, vertex_shader);
			glDetachShader(program, fragment_shader);
		}
	}

	glDeleteShader(vertex_shader);
	glDeleteShader(fragment_shader);

	return program;
}