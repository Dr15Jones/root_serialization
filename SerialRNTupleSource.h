#if !defined(SerialRNTupleSource_h)
#define SerialRNTupleSource_h

#include <string>
#include <memory>
#include <optional>
#include <vector>
#include <atomic>

#include "DataProductRetriever.h"
#include "DelayedProductRetriever.h"

#include "SharedSourceBase.h"
#include "SerialTaskQueue.h"
#include "ROOT/RNTuple.hxx"
#include "ROOT/RNTupleReader.hxx"
#include "SerialRNTupleRetrievers.h"

namespace cce::tf {
  class SerialRNTupleSource : public SharedSourceBase {
  public:
    SerialRNTupleSource(unsigned iNLanes, unsigned long long iNEvents, std::string const& iName, bool iDelayReading);
    size_t numberOfDataProducts() const final {return dataProductsPerLane_[0].size();}

    std::vector<DataProductRetriever>& dataProducts(unsigned int iLane, long iEventIndex) final {
      return dataProductsPerLane_[iLane];
    }
    EventIdentifier eventIdentifier(unsigned int iLane, long iEventIndex) final {
      return identifiers_[iLane];
    }
    
    void printSummary() const final;
    std::chrono::microseconds accumulatedTime() const;
  private:
    void readEventAsync(unsigned int iLane, long iEventIndex,  OptionalTaskHolder) final;

    
    SerialTaskQueue queue_;
    std::unique_ptr<ROOT::Experimental::RNTupleReader> events_;
    long nEvents_;
    std::chrono::microseconds accumulatedTime_;

    //per lane items
    std::vector<std::unique_ptr<ROOT::Experimental::REntry>> entries_;
    std::vector<SerialRNTuplePromptRetriever> promptReaders_;
    std::vector<SerialRNTupleDelayedRetriever> delayedReaders_;
    std::vector<EventIdentifier> identifiers_;
    std::vector<std::vector<void*>> ptrToDataProducts_;
    std::vector<std::vector<DataProductRetriever>> dataProductsPerLane_;
    ROOT::Experimental::DescriptorId_t eventAuxID_;
    
    bool delayReading_;
  };
}

#endif
