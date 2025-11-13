/*
 * This library is open source and may be redistributed and/or modified under
 * the terms of the OpenSceneGraph Public License (OSGPL) version 0.0 or (at
 * your option) any later version. The full license is in the LICENSE file
 * included with this distribution, and on the openscenegraph.org website.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * OpenSceneGraph Public License for more details.
*/

//
// Copyright(c) 2008 Skew Matrix Software LLC.
//

#ifndef __FLTEXP_UTILS_H__
#define __FLTEXP_UTILS_H__ 1


// FLTEXP_DELETEFILE macro is used to delete temp files created during file export.
// (Too bad OSG doesn't use Boost.)

#if defined(_WIN32)

    #include <windows.h>
    #define FLTEXP_DELETEFILE(file) DeleteFile((file))

#else   // Unix

    #include <stdio.h>
    #define FLTEXP_DELETEFILE(file) remove((file))

#endif

// VRV_PATCH BEGIN
    #include <string>
    #include <tuple>
    #include <unordered_map>
    #include <boost/container_hash/hash.hpp>
    //!@{
    //! DtDamageStateMappings is loaded from appData/importConfig/damage_state_mappings.csv
    //! The Key is a tuple of <Number of damage states, damage switch comment, damage state ID>
    //! The Value is the dynamic terrain state ID that is specified in VRF (dynamic-terrain.sysdef) 
    //! and is sent across a network connection.
    //! See DtDynamicTerrainParser / DtDynamicTerrainParserSettings
    struct TupleHasher
    {
        std::size_t operator()( const std::tuple<unsigned int, std::string, std::string>& tuple ) const
        {
            std::size_t seed = 0;
            boost::hash_combine( seed, std::get<0>( tuple ) );  // number of switch states
            boost::hash_combine( seed, std::get<1>( tuple ) );  // optional switch comments
            boost::hash_combine( seed, std::get<2>( tuple ) );  // dynamic terrain state ID
            return seed;
        }
    };
    using DtDamageStateMappings = std::unordered_map<std::tuple<unsigned int, std::string, std::string>, std::string, TupleHasher>;
    //!@}
// VRV_PATCH END
#endif
