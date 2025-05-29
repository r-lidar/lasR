#pragma once

#include <string>
#include <vector>
#include <map>
#include <memory>

class LASRstage {
public:
    std::string algoname;
    std::string output;
    std::string filter;
    bool raster;
    bool vector;
    std::string uid;
    std::map<std::string, std::variant<std::string, int, double, bool, std::vector<std::string>>> args;

    LASRstage(const std::string& algoname, 
              const std::string& output = "", 
              const std::string& filter = "",
              bool raster = false,
              bool vector = false)
        : algoname(algoname), output(output), filter(filter), raster(raster), vector(vector) {
        generate_uid();
    }

    void add_args(const std::string& key, const std::variant<std::string, int, double, bool, std::vector<std::string>>& value) {
        args[key] = value;
    }

    std::variant<std::string, int, double, bool, std::vector<std::string>> get_args(const std::string& key) const {
        auto it = args.find(key);
        if (it != args.end()) {
            return it->second;
        }
        return std::string();
    }

    std::map<std::string, std::string> to_dict() const {
        std::map<std::string, std::string> result;
        result["algoname"] = algoname;
        result["output"] = output;
        result["filter"] = filter;
        result["uid"] = uid;
        
        for (const auto& [key, value] : args) {
            std::visit([&](const auto& v) {
                if constexpr (std::is_same_v<std::decay_t<decltype(v)>, std::string>) {
                    result[key] = v;
                } else if constexpr (std::is_same_v<std::decay_t<decltype(v)>, int>) {
                    result[key] = std::to_string(v);
                } else if constexpr (std::is_same_v<std::decay_t<decltype(v)>, double>) {
                    result[key] = std::to_string(v);
                } else if constexpr (std::is_same_v<std::decay_t<decltype(v)>, bool>) {
                    result[key] = v ? "true" : "false";
                } else if constexpr (std::is_same_v<std::decay_t<decltype(v)>, std::vector<std::string>>) {
                    std::stringstream ss;
                    ss << "[";
                    for (size_t i = 0; i < v.size(); ++i) {
                        if (i > 0) ss << ",";
                        ss << "\"" << v[i] << "\"";
                    }
                    ss << "]";
                    result[key] = ss.str();
                }
            }, value);
        }
        
        return result;
    }

private:
    void generate_uid() {
        static const char charset[] = "abcdef0123456789";
        static const int charset_size = sizeof(charset) - 1;
        static std::random_device rd;
        static std::mt19937 gen(rd());
        static std::uniform_int_distribution<> dis(0, charset_size - 1);

        uid.resize(8);
        for (int i = 0; i < 8; ++i) {
            uid[i] = charset[dis(gen)];
        }
    }
};

class LASRpipeline {
public:
    std::vector<LASRstage> stages;

    LASRpipeline() = default;
    LASRpipeline(LASRstage* stage) {
        if (stage) {
            stages.push_back(*stage);
        }
    }

    void add_stage(const LASRstage& stage) {
        stages.push_back(stage);
    }
}; 