// Author: Enrico Guiraud, CERN  08/2020

/*************************************************************************
 * Copyright (C) 1995-2020, Rene Brun and Fons Rademakers.               *
 * All rights reserved.                                                  *
 *                                                                       *
 * For the licensing terms see $ROOTSYS/LICENSE.                         *
 * For the list of contributors see $ROOTSYS/README/CREDITS.             *
 *************************************************************************/

#include <ROOT/RDF/ColumnReaders.hxx>
#include <ROOT/RDF/RCustomColumnBase.hxx>
#include <ROOT/RDF/Utils.hxx> // TypeID2TypeName
#include <TClass.h>

#include <stdexcept> // std::runtime_error
#include <string>
#include <typeinfo>

void ROOT::Internal::RDF::CheckCustomColumn(RCustomColumnBase *customColumn, const std::type_info &tid)
{
   // Here we compare names and not typeinfos since they may come from two different contexts: a compiled
   // and a jitted one.
   const auto diffTypes = (0 != std::strcmp(customColumn->GetTypeId().name(), tid.name()));
   auto inheritedType = [&]() {
      auto colTClass = TClass::GetClass(customColumn->GetTypeId());
      return colTClass && colTClass->InheritsFrom(TClass::GetClass(tid));
   };

   if (diffTypes && !inheritedType()) {
      const auto tName = TypeID2TypeName(tid);
      const auto colTypeName = TypeID2TypeName(customColumn->GetTypeId());
      std::string errMsg = "RColumnValue: type specified for column \"" + customColumn->GetName() + "\" is ";
      if (tName.empty()) {
         errMsg += tid.name();
         errMsg += " (extracted from type info)";
      } else {
         errMsg += tName;
      }
      errMsg += " but temporary column has type ";
      if (colTypeName.empty()) {
         auto &id = customColumn->GetTypeId();
         errMsg += id.name();
         errMsg += " (extracted from type info)";
      } else {
         errMsg += colTypeName;
      }
      throw std::runtime_error(errMsg);
   }
}
