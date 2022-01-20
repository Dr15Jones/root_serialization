#include "PDSSource.h"
#include "TClass.h"
#include "SourceFactory.h"
#include "ReplicatedSharedSource.h"

#include "Deserializer.h"
#include "UnrolledDeserializer.h"

using namespace cce::tf;
using namespace cce::tf::pds;


bool PDSSource::readEvent(long iEventIndex) {
  while(iEventIndex != presentEventIndex_) {
    auto skipped = skipToNextEvent(file_);
    if(not skipped) {return false;}
    ++presentEventIndex_;
  }
  if(readEventContent()) {
    ++presentEventIndex_;
    return true;
  }
  return false;
}

bool PDSSource::readEventContent() {
  std::vector<uint32_t> buffer;
  if(not readCompressedEventBuffer(file_, eventID_, buffer)) {
    return false;
  }
  std::vector<uint32_t> uBuffer = uncompressEventBuffer(compression_, buffer);
  deserializeDataProducts(uBuffer.begin(), uBuffer.end(), dataProducts_, deserializers_);

  return true;
}

PDSSource::PDSSource(std::string const& iName) :
                 SourceBase(),
  file_{iName, std::ios_base::binary}
{
  pds::Serialization serialization;
  auto productInfo = readFileHeader(file_, compression_, serialization);

  switch(serialization) {
  case pds::Serialization::kRoot: { 
    deserializers_ = DeserializeStrategy::make<DeserializeProxy<Deserializer>>(); break;
  }
  case pds::Serialization::kRootUnrolled: {
    deserializers_ = DeserializeStrategy::make<DeserializeProxy<UnrolledDeserializer>>(); break;
  }
  }

  dataProducts_.reserve(productInfo.size());
  dataBuffers_.resize(productInfo.size(), nullptr);
  deserializers_.reserve(productInfo.size());
  size_t index =0;
  for(auto const& pi : productInfo) {
    
    TClass* cls = TClass::GetClass(pi.className().c_str());
    dataBuffers_[index] = cls->New();
    assert(cls);
    dataProducts_.emplace_back(index,
			       &dataBuffers_[index],
                               pi.name(),
                               cls,
			       &delayedRetriever_);
    deserializers_.emplace_back(cls);
    ++index;
  }
}

PDSSource::~PDSSource() {
  auto it = dataProducts_.begin();
  for( void * b: dataBuffers_) {
    it->classType()->Destructor(b);
    ++it;
  }
}

namespace {
    class Maker : public SourceMakerBase {
  public:
    Maker(): SourceMakerBase("ReplicatedPDSSource") {}
      std::unique_ptr<SharedSourceBase> create(unsigned int iNLanes, unsigned long long iNEvents, ConfigurationParameters const& params) const final {
        auto fileName = params.get<std::string>("fileName");
        if(not fileName) {
          std::cout <<"no file name given\n";
          return {};
        }
        return std::make_unique<ReplicatedSharedSource<PDSSource>>(iNLanes, iNEvents, *fileName);
    }
    };

  Maker s_maker;
}
