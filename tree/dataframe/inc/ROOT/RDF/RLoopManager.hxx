// Author: Enrico Guiraud, Danilo Piparo CERN  03/2017

/*************************************************************************
 * Copyright (C) 1995-2018, Rene Brun and Fons Rademakers.               *
 * All rights reserved.                                                  *
 *                                                                       *
 * For the licensing terms see $ROOTSYS/LICENSE.                         *
 * For the list of contributors see $ROOTSYS/README/CREDITS.             *
 *************************************************************************/

#ifndef ROOT_RLOOPMANAGER
#define ROOT_RLOOPMANAGER

#include "ROOT/RDF/RNodeBase.hxx"
#include "ROOT/RDF/NodesUtils.hxx"

#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>

// forward declarations
class TTreeReader;

namespace ROOT {
namespace RDF {
class RCutFlowReport;
class RDataSource;
} // ns RDF

namespace Internal {
namespace RDF {
ColumnNames_t GetBranchNames(TTree &t, bool allowDuplicates = true);

class RActionBase;
class GraphNode;

namespace GraphDrawing {
class GraphCreatorHelper;
} // ns GraphDrawing
} // ns RDF
} // ns Internal

namespace Detail {
namespace RDF {
using namespace ROOT::TypeTraits;
namespace RDFInternal = ROOT::Internal::RDF;

class RFilterBase;
class RRangeBase;
using ROOT::RDF::RDataSource;

/// The base class of the head nodes of a RDF computation graph.
/// This class is responsible of running the event loop.
class RLoopManagerBase : public RNodeBase {
   enum class ELoopType { kROOTFiles, kROOTFilesMT, kNoFiles, kNoFilesMT, kDataSource, kDataSourceMT };
   using Callback_t = std::function<void(unsigned int)>;
   class TCallback {
      const Callback_t fFun;
      const ULong64_t fEveryN;
      std::vector<ULong64_t> fCounters;

   public:
      TCallback(ULong64_t everyN, Callback_t &&f, unsigned int nSlots)
         : fFun(std::move(f)), fEveryN(everyN), fCounters(nSlots, 0ull)
      {
      }

      void operator()(unsigned int slot)
      {
         auto &c = fCounters[slot];
         ++c;
         if (c == fEveryN) {
            c = 0ull;
            fFun(slot);
         }
      }
   };

   class TOneTimeCallback {
      const Callback_t fFun;
      std::vector<int> fHasBeenCalled; // std::vector<bool> is thread-unsafe for our purposes (and generally evil)

   public:
      TOneTimeCallback(Callback_t &&f, unsigned int nSlots) : fFun(std::move(f)), fHasBeenCalled(nSlots, 0) {}

      void operator()(unsigned int slot)
      {
         if (fHasBeenCalled[slot] == 1)
            return;
         fFun(slot);
         fHasBeenCalled[slot] = 1;
      }
   };

   std::vector<RDFInternal::RActionBase *> fBookedActions; ///< Non-owning pointers to actions to be run
   std::vector<RDFInternal::RActionBase *> fRunActions;    ///< Non-owning pointers to actions already run
   std::vector<RFilterBase *> fBookedFilters;
   std::vector<RFilterBase *> fBookedNamedFilters; ///< Contains a subset of fBookedFilters, i.e. only the named filters
   std::vector<RRangeBase *> fBookedRanges;

   /// Shared pointer to the input TTree. It does not delete the pointee if the TTree/TChain was passed directly as an
   /// argument to RDataFrame's ctor (in which case we let users retain ownership).
   std::shared_ptr<TTree> fTree{nullptr};
   const ColumnNames_t fDefaultColumns;
   const ULong64_t fNEmptyEntries{0};
   const unsigned int fNSlots{1};
   bool fMustRunNamedFilters{true};
   const ELoopType fLoopType; ///< The kind of event loop that is going to be run (e.g. on ROOT files, on no files)
   const std::unique_ptr<RDataSource> fDataSource; ///< Owning pointer to a data-source object. Null if no data-source
   std::map<std::string, std::string> fAliasColumnNameMap; ///< ColumnNameAlias-columnName pairs
   std::vector<TCallback> fCallbacks;                      ///< Registered callbacks
   std::vector<TOneTimeCallback> fCallbacksOnce; ///< Registered callbacks to invoke just once before running the loop
   unsigned int fNRuns{0}; ///< Number of event loops run

   /// Cache of the tree/chain branch names. Never access directy, always use GetBranchNames().
   ColumnNames_t fValidBranchNames;

   void CheckIndexedFriends();
   void RunEmptySourceMT();
   void RunEmptySource();
   void RunTreeProcessorMT();
   void RunTreeReader();
   void RunDataSourceMT();
   void RunDataSource();
   void RunAndCheckFilters(unsigned int slot, Long64_t entry);
   void InitNodeSlots(TTreeReader *r, unsigned int slot);
   void InitNodes();
   void CleanUpNodes();
   void CleanUpTask(unsigned int slot);
   void EvalChildrenCounts();

public:
   RLoopManagerBase(TTree *tree, const ColumnNames_t &defaultBranches);
   RLoopManagerBase(ULong64_t nEmptyEntries);
   RLoopManagerBase(std::unique_ptr<RDataSource> ds, const ColumnNames_t &defaultBranches);
   RLoopManagerBase(const RLoopManagerBase &) = delete;
   RLoopManagerBase &operator=(const RLoopManagerBase &) = delete;

   void JitDeclarations();
   void Jit();
   RLoopManagerBase *GetLoopManagerUnchecked() final { return this; }
   void Run();
   const ColumnNames_t &GetDefaultColumnNames() const;
   TTree *GetTree() const;
   ::TDirectory *GetDirectory() const;
   ULong64_t GetNEmptyEntries() const { return fNEmptyEntries; }
   RDataSource *GetDataSource() const { return fDataSource.get(); }
   void Book(RDFInternal::RActionBase *actionPtr);
   void Deregister(RDFInternal::RActionBase *actionPtr);
   void Book(RFilterBase *filterPtr);
   void Deregister(RFilterBase *filterPtr);
   void Book(RRangeBase *rangePtr);
   void Deregister(RRangeBase *rangePtr);
   bool CheckFilters(unsigned int, Long64_t) final;
   unsigned int GetNSlots() const { return fNSlots; }
   void Report(ROOT::RDF::RCutFlowReport &rep) const final;
   /// End of recursive chain of calls, does nothing
   void PartialReport(ROOT::RDF::RCutFlowReport &) const final {}
   void SetTree(const std::shared_ptr<TTree> &tree) { fTree = tree; }
   void IncrChildrenCount() final { ++fNChildren; }
   void StopProcessing() final { ++fNStopsReceived; }
   void ToJitExec(const std::string &) const;
   void AddColumnAlias(const std::string &alias, const std::string &colName) { fAliasColumnNameMap[alias] = colName; }
   const std::map<std::string, std::string> &GetAliasMap() const { return fAliasColumnNameMap; }
   void RegisterCallback(ULong64_t everyNEvents, std::function<void(unsigned int)> &&f);
   unsigned int GetNRuns() const { return fNRuns; }

   /// End of recursive chain of calls, does nothing
   void AddFilterName(std::vector<std::string> &) {}
   /// For each booked filter, returns either the name or "Unnamed Filter"
   std::vector<std::string> GetFiltersNames();

   /// For all the actions, either booked or run
   std::vector<RDFInternal::RActionBase *> GetAllActions();

   std::vector<RDFInternal::RActionBase *> GetBookedActions() { return fBookedActions; }
   std::shared_ptr<ROOT::Internal::RDF::GraphDrawing::GraphNode> GetGraph();

   const ColumnNames_t &GetBranchNames();
};
} // namespace RDF
} // namespace Detail

namespace Internal {
namespace RDF {

template <typename DataSource>
struct DSTypeHelper {
   static_assert(std::is_base_of<RDataSource, DataSource>::value, "");
   using type = DataSource;
};

template <>
struct DSTypeHelper<TTree> {
   using type = RDataSource;
};

template <>
struct DSTypeHelper<void> {
   using type = RDataSource;
};

template <typename DataSource>
using DS_t = typename DSTypeHelper<DataSource>::type;

} // namespace RDF
} // namespace Internal

namespace Detail {
namespace RDF {

/// The head node of a computation graph. Most logic is implemented in RLoopManagerBase.
/// \tparam DataSource This type indicates where the data comes from, if it is known at compile-time. It can be either TTree, a concrete type inheriting from RDataSource, or void (which indicates the information is not known at compile-time).
template <typename DataSource>
class RLoopManager final : public RLoopManagerBase {
public:
   RLoopManager(TTree *tree, const ColumnNames_t &defaultBranches) : RLoopManagerBase(tree, defaultBranches) {}
   RLoopManager(ULong64_t nEmptyEntries) : RLoopManagerBase(nEmptyEntries) {}
   RLoopManager(std::unique_ptr<RDFInternal::DS_t<DataSource>> ds, const ColumnNames_t &defaultBranches)
      : RLoopManagerBase(std::move(ds), defaultBranches) { }
};

} // namespace RDF
} // namespace Detail
} // namespace ROOT

#endif
