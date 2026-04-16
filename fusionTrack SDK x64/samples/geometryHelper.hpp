// ============================================================================

/*!
 *
 *   This file is part of the ATRACSYS fusionTrack library.
 *   Copyright (C) 2003-2018 by Atracsys LLC. All rights reserved.
 *
 *   \file geometryHelper.hpp
 *   \brief Helping functions for geometry manipulation.
 *
 */
// ============================================================================

#pragma once

#include <ftkTypes.h>

#include <string>

struct ftkLibraryImp;
typedef ftkLibraryImp* ftkLibrary;
struct ftkBuffer;
struct ftkGeometry;
struct ftkRigidBody;

bool getFullFilePath( ftkLibrary lib,
                      const std::string& fileName,
                      std::string& fullFilePath,
                      bool* fromSystem = nullptr );

bool loadFileInBuffer( const std::string& fullFilePath, ftkBuffer& buffer );

/** \brief Helper function loading a geometry.
 *
 * This function allows to load a geometry file into a ::ftkGeometry instance.
 *
 * \deprecated Please use the loadRigidBody function instead.
 *
 * \param[in] lib initialised library handle.
 * \param[in] sn enumerated device serial number(not used any more).
 * \param[in] fileName name of the file to load (file name only, \e no
 * directory information!).
 * \param[out] geometry instance of ftkGeometry holding the parameters.
 *
 * \retval 0 if everything went fine,
 * \retval 1 if the data were loaded from the system directory,
 * \retval 2 if the data could not be loaded.
 */
#ifdef ATR_MSVC
__declspec( deprecated(
  "You should consider to use loadRigidBody, i.e. moving to ftkRigidBody and the related functions" ) )
#endif
  int loadGeometry( ftkLibrary lib, const uint64& sn, const std::string& fileName, ftkGeometry& geometry )
#if defined( ATR_GCC ) || defined( ATR_CLANG )
    __attribute__( ( deprecated(
      "You should consider to use loadRigidBody, i.e. moving to ftkRigidBody and the related functions" ) ) )
#endif
    ;

/** \brief Helper function loading a geometry.
 *
 * This function allows to load a geometry file into a ::ftkRigidBody instance.
 *
 * \param[in] lib initialised library handle.
 * \param[in] fileName name of the file to load (file name only, \e no
 * directory information!).
 * \param[out] geometry instance of ftkRigidBody holding the parameters.
 *
 * \retval 0 if everything went fine,
 * \retval 1 if the data were loaded from the system directory,
 * \retval 2 if the data could not be loaded.
 */
int loadRigidBody( ftkLibrary lib, const std::string& fileName, ftkRigidBody& geometry );