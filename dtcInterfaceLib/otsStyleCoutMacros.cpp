#include "dtcInterfaceLib/otsStyleCoutMacros.h"

//==============================================================================
// stackTrace
//	static function
//	https://gist.github.com/fmela/591333/c64f4eb86037bb237862a8283df70cdfc25f01d3
#include <cxxabi.h>    //for abi::__cxa_demangle
#include <execinfo.h>  //for back trace of stack

// #include "TUnixSystem.h"
std::string otsStyleStackTrace()
{
	__SS__ << "ots::stackTrace:\n";

	void* array[10];
	size_t size;

	// get void*'s for all entries on the stack
	size = backtrace(array, 10);
	// backtrace_symbols_fd(array, size, STDERR_FILENO);

	// https://stackoverflow.com/questions/77005/how-to-automatically-generate-a-stacktrace-when-my-program-crashes
	char** messages = backtrace_symbols(array, size);

	// skip first stack frame (points here)
	// char syscom[256];
	for (unsigned int i = 1; i < size && messages != NULL; ++i)
	{
		// mangled name needs to be converted to get nice name and line number
		// line number not working... FIXME

		//		sprintf(syscom,"addr2line %p -e %s",
		//				array[i],
		//				messages[i]); //last parameter is the name of this app
		//		ss << StringMacros::exec(syscom) << __E__;
		//		system(syscom);

		// continue;

		char *mangled_name = 0, *offset_begin = 0, *offset_end = 0;

		// find parentheses and +address offset surrounding mangled name
		for (char* p = messages[i]; *p; ++p)
		{
			if (*p == '(')
			{
				mangled_name = p;
			}
			else if (*p == '+')
			{
				offset_begin = p;
			}
			else if (*p == ')')
			{
				offset_end = p;
				break;
			}
		}

		// if the line could be processed, attempt to demangle the symbol
		if (mangled_name && offset_begin && offset_end && mangled_name < offset_begin)
		{
			*mangled_name++ = '\0';
			*offset_begin++ = '\0';
			*offset_end++ = '\0';

			int status;
			char* real_name = abi::__cxa_demangle(mangled_name, 0, 0, &status);

			// if demangling is successful, output the demangled function name
			if (status == 0)
			{
				ss << "[" << i << "] " << messages[i] << " : " << real_name << "+" << offset_begin << offset_end << std::endl;
			}
			// otherwise, output the mangled function name
			else
			{
				ss << "[" << i << "] " << messages[i] << " : " << mangled_name << "+" << offset_begin << offset_end << std::endl;
			}
			free(real_name);
		}
		// otherwise, print the whole line
		else
		{
			ss << "[" << i << "] " << messages[i] << std::endl;
		}
	}
	ss << std::endl;

	free(messages);

	// call ROOT's stack trace to get line numbers of ALL threads
	// gSystem->StackTrace();

	return ss.str();
}  // end stackTrace
