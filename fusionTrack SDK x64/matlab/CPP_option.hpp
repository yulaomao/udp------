#pragma once

#include <ftkInterface.h>

#include <vector>

#include <mex.h>

mxArray* opt2mat( const std::vector< ftkOptionsInfo >& in );
