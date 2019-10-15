/******************************************************************************
** Copyright (c) 2016 MAK Technologies, Inc.
** All rights reserved.
******************************************************************************/

//! \file DtCDBSigSize.h
//! \brief CDB Significant Size class (to be filled from ../appData/cdb/SignificantSize_SwitchDistance.xml)
//! Note: A DUPLICATED class also exist in the Openflight Plugin of MakOSG and this one in Vantage osgdb_osgearth_model_cdb
//! TODO REMOVE THIS ONE AND REFERENCE THE MakOsg FLT one once we MERGE IN THE TRUNK


#pragma once

namespace makVrv {
   namespace oe {
      namespace CDB
      {
         //! \brief Switch in distance associated to Significant size
         //! \ingroup osgdb_osgearth_model_cdb
         class CDBSigSize
         {
         public:

            //! \brief CTOR
            CDBSigSize();
            //! \brief Copy CTOR
            CDBSigSize(const CDBSigSize& SigSize);
            //! \brief CTOR
            CDBSigSize(int Lod, float MinSigSize, float SwitchInDistance);
            //! \brief DTOR
            virtual ~CDBSigSize();
            //! \brief Return the current Sig Size CDB lod
            int getLod() const;
            //! \brief Return Minimun significant size height in meter for this Lod
            float getMinSigSize() const;
            //! \brief Return the switch distance value to use instead of the SigSize
            float getSwitchInDistance() const;

         private:

            //! \brief Current CDB Lod (-10 to 23)
            int   myLod;
            //! \brief Minimun significant size height in meter for this Lod
            float myMinSigSize;
            //! \brief Switch in value to use instead of the SigSize
            float mySwitchInDistance;
         };

      }
   }
} // namespace makVrv::oe::CDB

