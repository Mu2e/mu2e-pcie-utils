// This file (util_main.cc) was created by Ron Rechenmacher <ron@fnal.gov> on
// May 13, 2015. "TERMS AND CONDITIONS" governing this file are in the README
// or COPYING file. If you do not have such a file, one can be obtained by
// contacting Ron or Fermi Lab in Batavia IL, 60510, phone: 630-840-3000.
// $RCSfile: .emacs.gnu,v $
// rev="$Revision: 1.23 $$Date: 2012/01/23 15:32:40 $";

#include "Mu2eUtil.h"

int main(int argc, char* argv[])
{
	DTCLib::Mu2eUtil theUtil;
	theUtil.parse_arguments(argc, argv);
	theUtil.run();

	return 0;
}  // main
