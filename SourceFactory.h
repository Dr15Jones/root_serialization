#if !defined(SourceFactory_h)
#define SourceFactory_h

#include "ComponentFactory.h"
#include "SharedSourceBase.h"
#include "ConfigurationParameters.h"
#include <map>


namespace cce::tf {
  using SourceFactory = ComponentFactory<SharedSourceBase*(unsigned int, unsigned long long, ConfigurationParameters const&)>;
  using SourceMakerBase = SourceFactory::CMakerBase;
}

#endif

