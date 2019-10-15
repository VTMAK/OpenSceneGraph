/******************************************************************************
** Copyright (c) 2016 MAK Technologies, Inc.
** All rights reserved.
******************************************************************************/

//! \file DtCDBSigSizeTable.h
//! \brief CDB Significant Size Table class (to be filled from ../appData/cdb/SignificantSize_SwitchDistance.xml)
//! Note: A DUPLICATED class also exist in the Openflight Plugin of MakOSG and this one in Vantage osgdb_osgearth_model_cdb
//! TODO REMOVE THIS ONE AND REFERENCE THE MakOsg FLT one once we MERGE IN THE TRUNK

#pragma once
#include <vector>
#include "DtCDBSigSize.h"
#include "DtCDBDatasetInfo.h"

namespace makVrv {
   namespace oe {
      namespace CDB
      {
         //! \brief Hold all the Significant size information of one dataset
         //! \ingroup osgdb_osgearth_model_cdb
         class CDBSigSizeTable
         {
         public:

            //! \brief CTOR
            CDBSigSizeTable();

            //! \brief return true if the xml table was loaded
            bool isValid() const;

            //! \brief Return the switch in distance for a Significant size
            float getSwitchInDistance(float sigSize) const;

            //! \brief Return the switch in distance for a CDB LOD
            float getSwitchInDistanceFromLod(int cdblod) const;

            //! \brief Only add to the table when the Openflight DLL is loaded (constructor)
            void add(const CDBSigSize& lodSigSize);

            //! \brief return the number of item in the mySigSizeTable
            int size() const;

            //! \brief clear the table to fill it again
            void clear();

         private:
            //! \brief Table that contain all the Significant Size of one dataset
            std::vector<CDBSigSize> mySigSizeTable;
         };

         class CDBSigSizeTableUtils
         {
         public:
            static void loadFromXML(CDBSigSizeTable& sigSizeTable, const DatasetInfo& datasetInfo, const std::string& fileName);
         };

      }
   }
} // namespace makVrv::oe::CDB




//class CDBRecordOfSigSizeTables
//{
//public:
//   //! \brief Get the unique instance 
//   static CDBRecordOfSigSizeTables& getInstance();
//
//   bool hasTable(const CDBDatasetInfo& info) const;
//   CDBSigSizeTable* createTable(const CDBDatasetInfo& info);
//   const CDBSigSizeTable* getSigSizeTable(const CDBDatasetInfo& info) const;
//private:
//   typedef std::map<CDBDatasetInfo, CDBSigSizeTable*> MapDataSetInfoToSigSizeTable;
//   MapDataSetInfoToSigSizeTable myMapDataSetInfoToSigSizeTable;
//};
//
//class CDBSigSizeTableUtils
//{
//public:
//   static void loadFromXML(CDBRecordOfSigSizeTables& sigSizeTableconst, const std::string& fileName);
//};