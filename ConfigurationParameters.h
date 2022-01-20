#if !defined(ConfigurationParameters_h)
#define ConfigurationParameters_h

#include <map>
#include <vector>
#include <string>
#include <string_view>
#include <optional>

namespace cce::tf {

  class ConfigurationParameters {
  public:
    using KeyValueMap = std::map<std::string, std::string, std::less<>>;

    explicit ConfigurationParameters(KeyValueMap iKeyValues):
    keyValues_{std::move(iKeyValues)} {
      unusedKeys_.reserve(keyValues_.size());
      for(auto const& kv: keyValues_) {
        unusedKeys_.push_back(kv.first);
      }
    }

    ConfigurationParameters() = delete;

    template<typename T> 
      std::optional<T> get(std::string_view iName) const {

      auto itFound = keyValues_.find(iName);
      if(itFound == keyValues_.end()) {
        return std::nullopt;
      }
      keyUsed(iName);

      return convert<T>(itFound->second);
    }

    template<typename T>
      T get(std::string_view iName, T iDefault) const {
      auto v = get<T>(iName);
      if(v) {
        return *v;
      }
      return iDefault;
    }

    std::vector<std::string_view> unusedKeys() const {
      return unusedKeys_;
    }

  private:
    template<typename T> static T convert(std::string const& iValue);

    void keyUsed(std::string_view iName) const {
      auto itUsed = std::lower_bound(unusedKeys_.begin(), unusedKeys_.end(), iName);
      if(itUsed != unusedKeys_.end() and *itUsed == iName) {
        unusedKeys_.erase(itUsed);
      }
    }

    KeyValueMap keyValues_;
    mutable std::vector<std::string_view> unusedKeys_;
  };

  template<>
    std::string ConfigurationParameters::convert<std::string>(std::string const& iValue);
  
  template<>
    int ConfigurationParameters::convert<int>(std::string const& iValue);

  template<>
    unsigned int ConfigurationParameters::convert<unsigned int>(std::string const& iValue);

  template<>
    bool ConfigurationParameters::convert<bool>(std::string const& iValue);


}

#endif
