#pragma once
#include <string>

class Shader {
public:
    Shader(const std::string& vertPath, const std::string& fragPath);
    ~Shader();

    void use() const;
    void setInt  (const std::string& name, int         v) const;
    void setFloat(const std::string& name, float       v) const;
    void setVec2 (const std::string& name, float x, float y) const;
