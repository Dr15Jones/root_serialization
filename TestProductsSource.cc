#include "TestProductsSource.h"
#include "SourceFactory.h"
#include "TClass.h"

#include <iostream>

using namespace cce::tf;

namespace {
  enum ProductIndices {
    kInts = 0,
    kFloats = 1
  };
}

TestDelayedProductRetriever::TestDelayedProductRetriever(std::vector<int>* iInts, std::vector<float>* iFloats):
  ints_(iInts),
  floats_(iFloats),
  eventIndex_(-1)
{
}

void TestDelayedProductRetriever::getAsync(DataProductRetriever& iRetriever, int index, TaskHolder iCallback) {
  switch(index) {
  case kInts: 
    {
      ints_->clear();
      int ei = static_cast<int>(eventIndex_);
      *ints_ = {ei, ei+1, ei+2};
      iRetriever.setSize(sizeof(int)*3);
      break;
    }
  case kFloats: 
    {
      floats_->clear();
      *floats_ = {eventIndex_*1.f, eventIndex_*2.f, eventIndex_*3.f};
      iRetriever.setSize(sizeof(float)*3);
      break;
    }
  }
  iCallback.doneWaiting();
}

void TestDelayedProductRetriever::setEventIndex(long iIndex) {
  eventIndex_ = iIndex;
}


TestProductsSource::TestProductsSource(unsigned int iNLanes, unsigned long long iNEvents):
  SharedSourceBase(iNEvents),
  intsPerLane_(iNLanes),
  floatsPerLane_(iNLanes)
{
  delayedPerLane_.reserve(iNLanes);
  retrieverPerLane_.reserve(iNLanes);
  for(unsigned int lane; lane<iNLanes; ++lane) {
    delayedPerLane_.emplace_back(&intsPerLane_[lane], &floatsPerLane_[lane]);
    std::vector<DataProductRetriever> r;
    r.reserve(2);
    r.emplace_back(kInts, delayedPerLane_[lane].ints(), "ints", TClass::GetClass("vector<int>"), &delayedPerLane_[lane]);
    r.emplace_back(kFloats, delayedPerLane_[lane].floats(), "floats", TClass::GetClass("vector<float>"), &delayedPerLane_[lane]);
    retrieverPerLane_.emplace_back(std::move(r));
  }
}

size_t TestProductsSource::numberOfDataProducts() const {
  return 2;
}

std::vector<DataProductRetriever>& TestProductsSource::dataProducts(unsigned int iLane, long iEventIndex) {
  return retrieverPerLane_[iLane];
}

EventIdentifier TestProductsSource::eventIdentifier(unsigned int iLane, long iEventIndex) {
  return {1, 1, static_cast<unsigned long long>(iEventIndex+1)};
}

void TestProductsSource::printSummary() const {
  std::cout <<"\nSource time: N/A"<<std::endl;
}

void TestProductsSource::readEventAsync(unsigned int iLane, long iEventIndex,  OptionalTaskHolder iTask) {
  delayedPerLane_[iLane].setEventIndex(iEventIndex);
  iTask.runNow();
}

namespace {
    class Maker : public SourceMakerBase {
  public:
    Maker(): SourceMakerBase("TestProductsSource") {}
      std::unique_ptr<SharedSourceBase> create(unsigned int iNLanes, unsigned long long iNEvents, ConfigurationParameters const& params) const final {
        return std::make_unique<TestProductsSource>(iNLanes, iNEvents);
    }
    };

  Maker s_maker;
}
