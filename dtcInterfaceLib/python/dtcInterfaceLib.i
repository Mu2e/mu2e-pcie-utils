%module dtcInterfaceLib
//-----------------------------------------------------------------------------
// SWIG symbols needed to generated wrappers
//-----------------------------------------------------------------------------
%include "std_unique_ptr.i"
%include "std_vector.i"
%include "std_string.i"
%include "stdint.i"
%include "typemaps.i"

%{
#include "mu2e_driver/mu2e_mmap_ioctl.h"
#include "artdaq-core-mu2e/Overlays/DTC_Types/DTC_SimMode.h"
#include "dtcInterfaceLib/CFOandDTC_Registers.h"
#include "dtcInterfaceLib/DTC_Registers.h"
#include "dtcInterfaceLib/DTC.h"
#include "dtcInterfaceLib/DTCSoftwareCFO.h"
#include "dtcInterfaceLib/mu2edev.h"
#include "dtcInterfaceLib/mu2esim.h"
using namespace DTCLib;
%}
//------------------------------------------------------------------------------
// this is for mu2edev::read_register to return the result as the second number
// in the tuple (the first one is the return code)
//-----------------------------------------------------------------------------
%apply uint32_t *OUTPUT { uint32_t *output };
//-----------------------------------------------------------------------------
// Process symbols in the headers
//-----------------------------------------------------------------------------
%include "artdaq-core-mu2e/Overlays/DTC_Types/DTC_SimMode.h"
%include "mu2e_driver/mu2e_mmap_ioctl.h"
%include "dtcInterfaceLib/mu2esim.h"
%include "dtcInterfaceLib/mu2edev.h"
%include "dtcInterfaceLib/CFOandDTC_Registers.h"
%include "dtcInterfaceLib/DTC_Registers.h"
%include "dtcInterfaceLib/DTC.h"
%include "dtcInterfaceLib/DTCSoftwareCFO.h"
%include "dtcInterfaceLib/mu2edev.h"
%include "dtcInterfaceLib/mu2esim.h"
