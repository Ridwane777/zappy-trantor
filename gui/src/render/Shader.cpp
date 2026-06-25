#include "GUIException.hpp"
#include "render/Shader.hpp"
#include <GL/glew.h>
#include <fstream>
#include <sstream>
#include <iostream>
#include <stdexcept>

std::string Shader::readFile(const std::string& path) {
    std::ifstream f(path);
    if (!f.is_open())
        throw GUIException("Shader::readFile: cannot open " + path);
    std::stringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

unsigned int Shader::compileShader(unsigned int type, const std::string& src) {
    unsigned int shader = glCreateShader(type);
    const char* c = src.c_str();
    glShaderSource(shader, 1, &c, nullptr);
    glCompileShader(shader);

    int success;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
