#pragma once

#include <ftkInterface.h>

#include <vector>

#include <mex.h>

bool mat2geom( const mxArray* pMexArray, ftkRigidBody& out );

mxArray* geom2mat( const std::vector< ftkRigidBody >& in );
