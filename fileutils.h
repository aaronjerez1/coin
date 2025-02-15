#ifndef FILEUTILS_H
#define FILEUTILS_H

#pragma once

class FileUtils {
public:
    static std::string readText(std::istream &in) {
        in.seekg(0, std::ios::end);
        auto size = static_cast<size_t>(in.tellg());
        if (size <= 0) {
            // LOGE("file size invalid: %lld", size);
            return {};
        }

        std::string fileStr(size + 1, 0);
        in.seekg(0, std::ios::beg);
        in.read(&fileStr[0], static_cast<std::streamsize>(size));

        return fileStr;
    }

    static std::string readText(const char *path) {
        std::fstream in(path, std::ios::in);
        if (!in.is_open()) {
            // LOGE("open file failed: %s", path);
            return {};
        }

        return readText(in);
    }

    static nlohmann::json parseJson(const std::string &str) {
        try {
            return nlohmann::json::parse(str);
        } catch (const nlohmann::json::parse_error &e) {
            // LOGE("parse file error: %s", e.what());
            return {};
        }
    }

    static nlohmann::json parseJson(const char *path) {
        return parseJson(readText(path));
    }

    static nlohmann::json parseJson(std::istream &in) {
        return parseJson(readText(in));
    }
};



#endif // FILEUTILS_H
