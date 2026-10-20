#pragma once
#include <string>
#include <string_view>
#include <cstdio>
#include <sstream>

namespace fmt {

namespace detail {
    inline void format_one(std::ostringstream& oss, const std::string& spec, int val) {
        if (spec.find("02d") != std::string::npos) {
            char buf[16];
            snprintf(buf, sizeof(buf), "%02d", val);
            oss << buf;
        } else {
            oss << val;
        }
    }
    inline void format_one(std::ostringstream& oss, const std::string& spec, unsigned int val) {
        if (spec.find("02d") != std::string::npos) {
            char buf[16];
            snprintf(buf, sizeof(buf), "%02u", val);
            oss << buf;
        } else {
            oss << val;
        }
    }
    inline void format_one(std::ostringstream& oss, const std::string& spec, long val) {
        oss << val;
    }
    inline void format_one(std::ostringstream& oss, const std::string& spec, unsigned long val) {
        oss << val;
    }
    inline void format_one(std::ostringstream& oss, const std::string& spec, float val) {
        oss << val;
    }
    inline void format_one(std::ostringstream& oss, const std::string& spec, double val) {
        oss << val;
    }
    inline void format_one(std::ostringstream& oss, const std::string& spec, const std::string& val) {
        oss << val;
    }
    inline void format_one(std::ostringstream& oss, const std::string& spec, std::string_view val) {
        oss << val;
    }
    inline void format_one(std::ostringstream& oss, const std::string& spec, const char* val) {
        oss << (val ? val : "");
    }
}

template<typename... Args>
inline std::string format(std::string_view fmt_pattern, Args&&... args) {
    std::ostringstream oss;
    size_t i = 0;
    auto format_next = [&](auto&& arg) {
        size_t open_brace = fmt_pattern.find('{', i);
        if (open_brace != std::string_view::npos) {
            oss << fmt_pattern.substr(i, open_brace - i);
            size_t close_brace = fmt_pattern.find('}', open_brace);
            if (close_brace != std::string_view::npos) {
                std::string spec(fmt_pattern.substr(open_brace + 1, close_brace - open_brace - 1));
                detail::format_one(oss, spec, std::forward<decltype(arg)>(arg));
                i = close_brace + 1;
            } else {
                i = open_brace;
            }
        }
    };
    (format_next(std::forward<Args>(args)), ...);
    if (i < fmt_pattern.size()) {
        oss << fmt_pattern.substr(i);
    }
    return oss.str();
}

} // namespace fmt
