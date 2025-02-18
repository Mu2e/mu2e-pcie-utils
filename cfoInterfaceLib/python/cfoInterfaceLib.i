%module cfoInterfaceLib
//-----------------------------------------------------------------------------
// SWIG symbols needed to generated wrappers
//-----------------------------------------------------------------------------
%include "std_unique_ptr.i"
%include "std_vector.i"
%include "std_string.i"
%include "stdint.i"
%include "typemaps.i"

%{
#include "dtcInterfaceLib/CFOandDTC_Registers.h"
#include "cfoInterfaceLib/CFO_Registers.h"
#include "cfoInterfaceLib/CFO.h"
#include "cfoInterfaceLib/CFO_Compiler.hh"
using namespace CFOLib;
%}
//------------------------------------------------------------------------------
// this is for mu2edev::read_register to return the result as the second number
// in the tuple (the first one is the return code)
//-----------------------------------------------------------------------------
%apply uint32_t *OUTPUT { uint32_t *output };
//-----------------------------------------------------------------------------
// Process symbols in the headers
//-----------------------------------------------------------------------------
// %include "mu2e_driver/mu2e_mmap_ioctl.h"
%include "dtcInterfaceLib/CFOandDTC_Registers.h"
%include "cfoInterfaceLib/CFO_Registers.h"
%include "cfoInterfaceLib/CFO.h"
%include "cfoInterfaceLib/CFO_Compiler.hh"
