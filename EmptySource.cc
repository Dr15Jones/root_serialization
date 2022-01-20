#include "EmptySource.h"
#include "SourceFactory.h"

using namespace cce::tf;
namespace {
    class Maker : public SourceMakerBase {
  public:
    Maker(): SourceMakerBase("EmptySource") {}
      std::unique_ptr<SharedSourceBase> create(unsigned int iNLanes, unsigned long long iNEvents, ConfigurationParameters const& params) const final {
        return std::make_unique<EmptySource>(iNEvents);
    }
    };

  Maker s_maker;
}
