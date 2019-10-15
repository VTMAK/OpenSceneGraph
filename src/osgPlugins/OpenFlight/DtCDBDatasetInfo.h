/******************************************************************************
** Copyright(c) 2016 MAK Technologies, Inc.
** All rights reserved.
******************************************************************************/

//! \file DtCDBDatasetInfo.h
//! \brief CDB Class that hold the Dataset information 
//! \ingroup osgdb_osgearth_model_cdb
//! Note: A DUPLICATED class also exist in the Openflight Plugin of MakOSG and this one in Vantage osgdb_osgearth_model_cdb
//! TODO REMOVE THIS ONE AND REFERENCE THE MakOsg FLT one once we MERGE IN THE TRUNK

#pragma once  
#include <sstream> 
#include <iostream> 
#include <iomanip>

namespace makVrv {
   namespace oe {
      namespace CDB
      {
         //! \brief   CDB Dataset information class
         class DatasetInfo
         {
         public:

            //! \brief CTOR
            DatasetInfo() :
               myDataset(0),
               myComponentSelector1(0),
               myComponentSelector2(0)
            {
               setStr();
            };
            //! \brief CTOR
            DatasetInfo(int Dataset, int ComponentSelector1, int ComponentSelector2) :
               myDataset(Dataset),
               myComponentSelector1(ComponentSelector1),
               myComponentSelector2(ComponentSelector2)
            {
               setStr();
            };

            //! \brief Smaller than operator for the Dataset Info class
            friend bool operator< (const DatasetInfo& lhs, const DatasetInfo& rhs)
            {
               if (lhs.myDataset < rhs.myDataset)
               {
                  return true;
               }
               else if (lhs.myDataset > rhs.myDataset)
               {
                  return false;
               }
               else
               {
                  if (lhs.myComponentSelector1 < rhs.myComponentSelector1)
                  {
                     return true;
                  }
                  else if (lhs.myComponentSelector1 > rhs.myComponentSelector2)
                  {
                     return false;
                  }
                  else
                  {
                     if (lhs.myComponentSelector2 <= rhs.myComponentSelector2)
                     {
                        return true;
                     }
                     else
                     {
                        return false;
                     }
                  }
               }
            };
            //! \brief  Return the string representation of this dataset.
            std::string str() const { return myStringRepresentation; };
            //! \brief  Change the dataset information
            void setDatasetInfo(int Dataset, int ComponentSelector1, int ComponentSelector2)
            {
               myDataset = Dataset;
               myComponentSelector1 = ComponentSelector1;
               myComponentSelector2 = ComponentSelector2;
               setStr();
            };
            //! \brief  Return the CDB dataset code (Dxxx)
            int getDataset() const { return myDataset; };
            //! \brief  Return the CDB component selector 1 (Sxxx)
            int getComponentSelector1() const { return myComponentSelector1; };
            //! \brief  Return the CDB component selector 2 (Txxx)
            int getComponentSelector2() const { return myComponentSelector2; };

         private:

            //! \brief  Get the string representation of this dataset.
            void setStr()
            {
               std::stringstream format_stream_2;
               format_stream_2 << "[D" << std::setfill('0') << std::setw(3) << myDataset;  // dataset code
               format_stream_2 << "_S" << std::setfill('0') << std::setw(3) << myComponentSelector1;  // Component Selector 1
               format_stream_2 << "_T" << std::setfill('0') << std::setw(3) << myComponentSelector2;  // Component Selector 2
               format_stream_2 << "]";
               myStringRepresentation = format_stream_2.str();
            };

            //! \brief  CDB dataset code (Dxxx)
            int myDataset;
            //! \brief  CDB component selector 1 (Sxxx)
            int myComponentSelector1;
            //! \brief  CDB component selector 2 (Txxx)
            int myComponentSelector2;
            //! \brief  CDB Dataset string information representation (Dxxx_Sxxx_Txxx)
            std::string myStringRepresentation;
         };
      }
   }
} // namespace makVrv::oe::CDB