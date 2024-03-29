#include "configKeyValuePairs.h"

namespace cce::tf {

  ConfigKeyValueMap configKeyValuePairs(std::string_view iToParse) {
    ConfigKeyValueMap keyValues;
    std::string::size_type start=0;
    std::string::size_type pos;

    bool firstEntry = true;
    do {
      pos = iToParse.find(':',start);
      std::string keyValue;
      if(pos != std::string::npos) {
	keyValue = iToParse.substr(start,pos-start);
      } else {
	keyValue = iToParse.substr(start);
      }
      start = pos;
      if(pos != std::string::npos) {
	++start;
      }
      if(keyValue.empty()) {
	break;
      }
      if( (pos = keyValue.find('=')) != std::string::npos ) {
	keyValues.emplace(keyValue.substr(0,pos), keyValue.substr(pos+1));
      } else {
        if(firstEntry) {
          //this might be a file name
          auto pos = keyValue.find('/',0);
          if(pos != std::string::npos) {
            keyValues.emplace("fileName",keyValue);
          } else {
            pos = keyValue.find('.',0);
            if(pos != std::string::npos) {
              keyValues.emplace("fileName",keyValue);
            } else {
              keyValues.emplace(keyValue,"");
            }
          }
        } else {
          keyValues.emplace(keyValue,"");
        }
      }
      firstEntry = false;
    } while(start != std::string::npos);
    return keyValues;
  }
}
