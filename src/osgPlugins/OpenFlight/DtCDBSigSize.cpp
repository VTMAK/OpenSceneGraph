/******************************************************************************
** Copyright (c) 2016 MAK Technologies, Inc.
** All rights reserved.
******************************************************************************/

//! \file DtCDBSigSize.cpp
//! \brief CDB Significant Size class (to be filled from ../appData/cdb/SignificantSize_SwitchDistance.xml)
//! Note: A DUPLICATED class also exist in the Openflight Plugin of MakOSG and this one in Vantage osgdb_osgearth_model_cdb
//! TODO REMOVE THIS ONE AND REFERENCE THE MakOsg FLT one once we MERGE IN THE TRUNK
#include "DtCDBSigSize.h"

using namespace makVrv::oe::CDB;


CDBSigSize::CDBSigSize() :
myLod(0),
myMinSigSize(0.0f),
mySwitchInDistance(0.0f)
{

};

CDBSigSize::CDBSigSize(int Lod, float MinSigSize, float SwitchInDistance) :
myLod(Lod),
myMinSigSize(MinSigSize),
mySwitchInDistance(SwitchInDistance)
{

};

CDBSigSize::CDBSigSize(const CDBSigSize& SigSize) :
myLod(SigSize.myLod),
myMinSigSize(SigSize.myMinSigSize),
mySwitchInDistance(SigSize.mySwitchInDistance)
{

}

CDBSigSize::~CDBSigSize()
{

};

int CDBSigSize::getLod() const
{
   return myLod;
}

float CDBSigSize::getMinSigSize() const
{
   return myMinSigSize;
};

float CDBSigSize::getSwitchInDistance() const
{
   return mySwitchInDistance;
}


