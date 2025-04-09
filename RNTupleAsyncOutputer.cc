#include <iostream>
#include "RNTupleAsyncOutputer.h"
#include "OutputerFactory.h"
#include "FunctorTask.h"
#include "RNTupleOutputerConfig.h"

#include <ROOT/RNTupleModel.hxx>
#include <ROOT/RField.hxx>
#include <ROOT/RFieldVisitor.hxx>

using namespace cce::tf;

RNTupleAsyncOutputer::RNTupleAsyncOutputer(std::string const& fileName, unsigned int iNLanes, RNTupleOutputerConfig const& iConfig):
    fileName_(fileName),
    laneInfos_(iNLanes),
    config_(iConfig),
    collateTime_{std::chrono::microseconds::zero()},
    parallelTime_{0}
  { }

void RNTupleAsyncOutputer::setupForLane(unsigned int iLaneIndex, std::vector<DataProductRetriever> const& iDPs) {
  if ( iLaneIndex == 0 ) {
    const std::string eventAuxiliaryBranchName{"EventAuxiliary"}; 
    bool hasEventAuxiliaryBranch = false;
    
    auto model = ROOT::RNTupleModel::CreateBare();
    fieldIDs_.reserve(iDPs.size());
    for(auto const& dp: iDPs) {
      // chop last . if present
      if(dp.name() == eventAuxiliaryBranchName) {
        hasEventAuxiliaryBranch = true;
      }
      auto name = dp.name().substr(0, dp.name().find("."));
      if ( config_.verbose_ > 1 ) std::cout << "-------- Creating field for " << name << " of type " << dp.classType()->GetName() << "\n";
      try { 
        auto field = ROOT::RFieldBase::Create(name, dp.classType()->GetName()).Unwrap();
        assert(field);
        if ( config_.verbose_ > 1 ) ROOT::Internal::RPrintSchemaVisitor(std::cout, '*', 1000, 10).VisitField(*field);
        model->AddField(std::move(field));
        fieldIDs_.emplace_back(std::move(name));
        
      }
      catch (ROOT::RException& e) {
         std::cout << "Failed: " << e.what() << "\n";
         throw std::runtime_error("Failed to create field");
      }
    }
    if(not hasEventAuxiliaryBranch) {
      hasEventAuxiliaryBranch_ = false;
      auto field = ROOT::RFieldBase::Create("EventID", "cce::tf::EventIdentifier").Unwrap();
      if ( config_.verbose_ > 1 ) ROOT::Internal::RPrintSchemaVisitor(std::cout, '*', 1000, 10).VisitField(*field);
      assert(field);
      model->AddField(std::move(field));
      fieldIDs_.emplace_back("EventID");
    }
    // https://root.cern/doc/v626/classROOT_1_1Experimental_1_1RNTupleWriteOptions.html
    auto writeOptions = ROOT::RNTupleWriteOptions();
    writeOptions.SetCompression(config_.compressionAlgorithm_, config_.compressionLevel_);
    writeOptions.SetMaxUnzippedPageSize(config_.maxUnzippedPageSize_);
    writeOptions.SetApproxZippedClusterSize(config_.approxZippedClusterSize_);
    writeOptions.SetMaxUnzippedClusterSize(config_.maxUnzippedClusterSize_);
    writeOptions.SetUseBufferedWrite(config_.useBufferedWrite_);
    
    ntuple_ = ROOT::Experimental::RNTupleParallelWriter::Recreate(std::move(model), "Events", fileName_, writeOptions);
  }
  else if ( not ntuple_ ) {
    throw std::logic_error("setupForLane should be sequential");
  }
  laneInfos_[iLaneIndex].retrievers = &iDPs;
  laneInfos_[iLaneIndex].fillContext = ntuple_->CreateFillContext();
}

void RNTupleAsyncOutputer::productReadyAsync(unsigned int iLaneIndex, DataProductRetriever const& iDataProduct, TaskHolder iCallback) const {
}

void RNTupleAsyncOutputer::outputAsync(unsigned int iLaneIndex, EventIdentifier const& iEventID, TaskHolder iCallback) const {
  auto runningClock = startClock();
  auto start = std::chrono::high_resolution_clock::now();

  auto context = fillProducts(iEventID, laneInfos_[iLaneIndex]);

  if(context) {
    auto pTime = &flushClusterTime_;
    queue_.push(*iCallback.group(), [context, pTime, callback=std::move(iCallback)]() mutable {
       auto start = std::chrono::high_resolution_clock::now();
       context->FlushCluster();
       auto time = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::high_resolution_clock::now() - start);
       *pTime += time.count();
                                    });
  }
  
  auto time = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::high_resolution_clock::now() - start);
  parallelTime_ += time.count();
  stopClock(runningClock);
}

decltype(std::chrono::high_resolution_clock::now()) RNTupleAsyncOutputer::startClock() const {
  std::lock_guard guard(wallclockMutex_);
  if( 1 == ++nConcurrentCalls_) {
    wallclockStartTime_ = std::chrono::high_resolution_clock::now();
  }
  return wallclockStartTime_;
}

void RNTupleAsyncOutputer::stopClock(decltype(std::chrono::high_resolution_clock::now()) const& start) const {
  bool shouldAdd = false;
  decltype(std::chrono::high_resolution_clock::now()) end;
  {
    std::lock_guard guard(wallclockMutex_);
    if( 0 == --nConcurrentCalls_) {
      shouldAdd = true;
      end = std::chrono::high_resolution_clock::now();
      //for sanity, reset the start to this end since we've already used the start
      wallclockStartTime_ = end;
    }
  }
  if(shouldAdd) {
    wallclockTime_ += std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
  }
}

void RNTupleAsyncOutputer::printSummary() const {
  auto start = std::chrono::high_resolution_clock::now();
  laneInfos_.clear();
  ntuple_.reset();
  auto deleteTime = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::high_resolution_clock::now() - start);

  start = std::chrono::high_resolution_clock::now();

  std::cout <<"RNTupleAsyncOutputer\n"
    "  total wallclock time at end event: "<<wallclockTime_.load()<<"us\n"
    "  total non-serializer parallel time at end event: "<<parallelTime_.load()<<"us\n"
    "  time in FlushCluster: "<<flushClusterTime_<<"us\n"
    "  end of job RNTupleAsyncWriter shutdown time: "<<deleteTime.count()<<"us\n";
}

ROOT::Experimental::RNTupleFillContext* RNTupleAsyncOutputer::fillProducts(
    EventIdentifier const& iEventID,
    RNTupleAsyncOutputer::LaneContainer const& entry ) const
{
  auto start = std::chrono::high_resolution_clock::now();
  auto thisOffset = eventGlobalOffset_++;
  if ( config_.verbose_ > 0 ) std::cout << thisOffset << " event id " << iEventID.run << ", "<< iEventID.lumi<<", "<<iEventID.event<<"\n";

  auto rentry = entry.fillContext->CreateEntry();
  auto temp = iEventID;
  for(size_t i=0; i < entry.retrievers->size(); ++i) {
    void** ptr = (*entry.retrievers)[i].address();
    rentry->BindRawPtr(fieldIDs_[i], *ptr);
  }
  if(not hasEventAuxiliaryBranch_) {
    rentry->BindRawPtr("EventID", &temp);
  }
  ROOT::RNTupleFillStatus status;
  entry.fillContext->FillNoFlush(*rentry, status);
  if(status.ShouldFlushCluster()) {
    //doesn't require calls to the TFile
    entry.fillContext->FlushColumns();
    collateTime_ += std::chrono::duration_cast<decltype(collateTime_)>(std::chrono::high_resolution_clock::now() - start);

    return entry.fillContext.get();
     //this need synchronization across all callers to the TFile
     //fillContext.FlushCluster();
  }
  
  collateTime_ += std::chrono::duration_cast<decltype(collateTime_)>(std::chrono::high_resolution_clock::now() - start);
  return nullptr;
}


namespace {
class Maker : public OutputerMakerBase {
  public:
    Maker(): OutputerMakerBase("RNTupleAsyncOutputer") {}
    std::unique_ptr<OutputerBase> create(unsigned int iNLanes, ConfigurationParameters const& params) const final {

      auto result = parseRNTupleConfig(params);
      if(not result) {
        return {};
      }
      return std::make_unique<RNTupleAsyncOutputer>(result->first, iNLanes, result->second);
    }
};

Maker s_maker;
}
