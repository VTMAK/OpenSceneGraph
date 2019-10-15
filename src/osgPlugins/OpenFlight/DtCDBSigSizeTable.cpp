/******************************************************************************
** Copyright (c) 2016 MAK Technologies, Inc.
** All rights reserved.
******************************************************************************/

//! \file DtCDBSigSizeTable.cpp
//! \brief CDB Significant Size class (to be filled from ../appData/cdb/SignificantSize_SwitchDistance.xml)
//! Note: A DUPLICATED class also exist in the Openflight Plugin of MakOSG and this one in Vantage osgdb_osgearth_model_cdb
//! TODO REMOVE THIS ONE AND REFERENCE THE MakOsg FLT one once we MERGE IN THE TRUNK

#include "DtCDBSigSizeTable.h"

#include <osgDB/FileUtils>
#include <osgDB/XmlParser>

using namespace makVrv::oe::CDB;

CDBSigSizeTable::CDBSigSizeTable()
{

}

bool CDBSigSizeTable::isValid() const
{
   if (mySigSizeTable.size() == 34) //(34 CDB LODs in the table)
      return true;
   else
      return false;
}

float CDBSigSizeTable::getSwitchInDistance(float sigSize) const
{
   for (std::vector<CDBSigSize>::const_iterator it = mySigSizeTable.begin(); it != mySigSizeTable.end(); ++it)
   {
      if (sigSize > it->getMinSigSize())
      {
         return it->getSwitchInDistance();
      }
   }
   return 0.1f;
}

float CDBSigSizeTable::getSwitchInDistanceFromLod(int cdblod)  const
{
   const CDBSigSize& sigSizeInfo = mySigSizeTable[cdblod + 10]; // pass the cdb position in the vector
   return sigSizeInfo.getSwitchInDistance();
}

int CDBSigSizeTable::size() const
{
   return mySigSizeTable.size();
}

void CDBSigSizeTable::add(const CDBSigSize& LodSigSize)
{
   // add to sig. size table 
   mySigSizeTable.push_back(LodSigSize);
}

void CDBSigSizeTable::clear()
{
   mySigSizeTable.clear();
}

using namespace osgDB;
void CDBSigSizeTableUtils::loadFromXML(CDBSigSizeTable& sigSizeTable, const DatasetInfo& datasetInfo, const std::string& fileName)
{
   if (osgDB::fileExists(fileName))
   {
      XmlNode* topXmlNode = readXmlFile(fileName);
      if (topXmlNode != NULL)
      {
         for (XmlNode::Children::iterator itSigSize = topXmlNode->children.begin(); itSigSize != topXmlNode->children.end(); ++itSigSize)
         {
            // top xml node "SigSize"
            if ((*itSigSize)->name == "SigSize")
            {
               bool foundDataset = false;
               // Dataset
               std::string datasetstr = (*itSigSize)->properties["cdb_dataset"];
               int dataset = std::atoi(datasetstr.c_str());
               // Component Selector 1
               std::string cmpsel1str = (*itSigSize)->properties["cdb_component_selector1"];
               int cmpsel1 = std::atoi(cmpsel1str.c_str());
               // Component Selector 2
               std::string cmpsel2str = (*itSigSize)->properties["cdb_component_selector2"];
               int cmpsel2 = std::atoi(cmpsel2str.c_str());

               // Check for more sig size section for a specific dataset
               if (datasetInfo.getDataset() == dataset &&
                  datasetInfo.getComponentSelector1() == cmpsel1 &&
                  datasetInfo.getComponentSelector2() == cmpsel2)
               {
                  // we clear the default values to fill it with a datasets specific values
                  sigSizeTable.clear();
                  foundDataset = true;
               }

               if (dataset == 0 || foundDataset)
               {
                  for (XmlNode::Children::iterator itLodSigSize = (*itSigSize)->children.begin(); itLodSigSize != (*itSigSize)->children.end(); ++itLodSigSize)
                  {
                     if ((*itLodSigSize)->name == "CDBLOD")
                     {
                        // lod
                        std::string lodstr = (*itLodSigSize)->properties["lod"];
                        int lod = std::atoi(lodstr.c_str());
                        // significant Size
                        std::string sigsizestr = (*itLodSigSize)->properties["significantSize"];
                        float sigsize = std::atof(sigsizestr.c_str());
                        // switchIn Distance 
                        std::string switchstr = (*itLodSigSize)->properties["switchInDistance"];
                        float switchInDistance = std::atof(switchstr.c_str());
                        // Add the values to the table                      
                        sigSizeTable.add(CDBSigSize(lod, sigsize, switchInDistance));
                     }
                  } //  each CDBLOD
               }
               if (datasetInfo.getDataset() == dataset &&
                  datasetInfo.getComponentSelector1() == cmpsel1 &&
                  datasetInfo.getComponentSelector2() == cmpsel2)
               {
                  // we found the values for this specific datasets ... stop reading the file
                  break;
               }

            }
         } //  each SigSize (just 1)
      }
      else
      {
         OSG_WARN << "Error reading file:" << fileName << std::endl;
      }
   }
   else
   {
      OSG_WARN << " Unable to find CDB Significant file: " << fileName << ".  Using default values" << std::endl;
   }
   if (sigSizeTable.size() != 34)
   {
      OSG_WARN << "Could not find proper Significant Size table in file [" << fileName << "] for dataset " << datasetInfo.str() << std::endl;
   }
}
