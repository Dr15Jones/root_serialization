#include "sourceFactoryGenerator.h"
#include "SourceFactory.h"
#include "configKeyValuePairs.h"
#include <iostream>

std::function<std::unique_ptr<cce::tf::SharedSourceBase>(unsigned int, unsigned long long)> 
cce::tf::sourceFactoryGenerator(std::string_view iType, std::string_view iOptions) {
  std::function<std::unique_ptr<SharedSourceBase>(unsigned int, unsigned long long)> sourceFactory;

  auto keyValues = cce::tf::configKeyValuePairs(iOptions);
  sourceFactory = [type = std::string(iType), params=ConfigurationParameters(keyValues)]
    (unsigned int iNLanes, unsigned long long iNEvents) {
    auto maker = SourceFactory::get()->create(type, iNLanes, iNEvents, params);
    
    if(not maker) {
      return maker;
    }
    
    //make sure all parameters given were used
    auto unusedOptions = params.unusedKeys();
    if(not unusedOptions.empty()) {
      std::cout <<"Unused options in "<<type<<"\n";
      for(auto const& key: unusedOptions) {
        std::cout <<"  '"<<key<<"'"<<std::endl;
      }
      return decltype(maker)();
    }
    
    return maker;
  };

  return sourceFactory;
}
