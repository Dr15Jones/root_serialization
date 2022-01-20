#include "RootSource.h"
#include "SourceFactory.h"
#include "ReplicatedSharedSource.h"

#include <iostream>
#include "TBranch.h"
#include "TTree.h"
#include "TFile.h"
#include "TClass.h"

using namespace cce::tf;

RootSource::RootSource(std::string const& iName) :
  file_{TFile::Open(iName.c_str())},
  eventAuxReader_{*file_}
{
  events_ = file_->Get<TTree>("Events");
  auto l = events_->GetListOfBranches();

  const std::string eventAuxiliaryBranchName{"EventAuxiliary"}; 
  const std::string eventIDBranchName{"EventID"}; 

  dataProducts_.reserve(l->GetEntriesFast());
  branches_.reserve(l->GetEntriesFast());

  TBranch* aux_branch{nullptr};
  for( int i=0; i< l->GetEntriesFast(); ++i) {
    auto b = dynamic_cast<TBranch*>((*l)[i]);
    //std::cout<<b->GetName()<<std::endl;
    //std::cout<<b->GetClassName()<<std::endl;
    if(eventIDBranchName == b->GetName()) {
      eventIDBranch_ = b;
      continue;
    }
    b->SetupAddresses();
    TClass* class_ptr=nullptr;
    EDataType type;
    b->GetExpectedType(class_ptr,type);

    dataProducts_.emplace_back(i,
			       reinterpret_cast<void**>(b->GetAddress()),
                               b->GetName(),
                               class_ptr,
			       &delayedReader_);
    branches_.emplace_back(b);
    if(eventAuxiliaryBranchName == dataProducts_.back().name()) {
      eventAuxBranch_ = b;
    }
  }
}

EventIdentifier RootSource::eventIdentifier() {
  if(eventIDBranch_) {
    return id_;
  }
  return eventAuxReader_.doWork(eventAuxBranch_);
}

long RootSource::numberOfEvents() { 
  return events_->GetEntriesFast();
}

bool RootSource::readEvent(long iEventIndex) {
  if(iEventIndex<numberOfEvents()) {
    if(eventIDBranch_) {
      eventIDBranch_->SetAddress(&id_);
      eventIDBranch_->GetEntry(iEventIndex);
    }

    auto it = dataProducts_.begin();
    for(auto b: branches_) {
      (it++)->setSize( b->GetEntry(iEventIndex) );
    }
    return true;
  }
  return false;
}

namespace {
    class Maker : public SourceMakerBase {
  public:
    Maker(): SourceMakerBase("ReplicatedRootSource") {}
      std::unique_ptr<SharedSourceBase> create(unsigned int iNLanes, unsigned long long iNEvents, ConfigurationParameters const& params) const final {
        auto fileName = params.get<std::string>("fileName");
        if(not fileName) {
          std::cout <<"no file name given\n";
          return {};
        }
        return std::make_unique<ReplicatedSharedSource<RootSource>>(iNLanes, iNEvents, *fileName);
    }
    };

  Maker s_maker;
}
