#include "cfoInterfaceLib/CFO_Compiler.hh"

#include <iostream>
#include <set>

#define TRACE_NAME "CFO_Compiler"

#include "dtcInterfaceLib/otsStyleCoutMacros.h"

template<class T>
std::string vectorToString(  // defined in included .icc source
	const std::vector<T>& setToReturn,
	const std::string& delimeter = ", ");

void getVectorFromString(
	const std::string& inputString,
	std::vector<std::string>& listToReturn,
	const std::set<char>& delimiter = {',', '|', '&'},
	const std::set<char>& whitespace = {' ', '\t', '\n', '\r'},
	std::vector<char>* listOfDelimiters = 0 /*,
//bool													decodeURIComponents = false */
);

template<class T>
bool getNumber(const std::string& s, T& retValue);

const std::string CFOLib::CFO_Compiler::MAIN_GOTO_LABEL = "MAIN";

const uint64_t CFOLib::CFO_Compiler::FPGAClock_ = (1e9 / (40e6) /* 40MHz FPGAClock for calculating delays */);  // period of FPGA clock in ns

//========================================================================
std::string CFOLib::CFO_Compiler::processFile(const std::string& sourceCodeFile,
											  const std::string& binaryOutputFile)
try
{
	__COUT_INFO__ << "CFO_Compiler::processFile BEGIN";

	FILE* fp = fopen(sourceCodeFile.c_str(), "r");
	if (!fp)
	{
		__SS__ << "Input File (" << sourceCodeFile << ") didn't open. Does it exist?" << __E__;
		__SS_THROW__;
	}

	std::string line;
	char lineChars[100];

	txtLineNumber_ = 0;
	binLineNumber_ = 0;
	hasRequiredPlanEndOp_ = false;  // require at least one END, REPEAT, or GOTO MAIN command to give a handle on switch Run Plan buffers to the CFO firmware

	loopStack_.clear();
	labelMap_.clear();

	output_.clear();

	// main line processing loop
	while (fgets(lineChars, 100, fp))
	{
		line = lineChars;
		while (strlen(lineChars) && lineChars[strlen(lineChars) - 1] != '\n' && fgets(lineChars, 100, fp))
			line += lineChars;

		++txtLineNumber_;
		__COUTT__ << "Line number " << txtLineNumber_ << ": " << line << __E__;

		if (isComment(line))
		{
			__COUTT__ << txtLineNumber_ << ": is comment" << __E__;
			continue;
		}

		// read all arguments
		opArguments_.clear();
		getVectorFromString(line, opArguments_,
							{',', ' ', '\t', ';', '='} /*delimiter*/,
							{'\n', '\r'} /*white space (empty because white space is a delimiter)*/);

		// clean up empty strings (because of white space delimiter) and comments
		for (size_t i = 0; i < opArguments_.size(); ++i)
			if (opArguments_[i].length() == 0)
				opArguments_.erase(opArguments_.begin() + i--);  // erase and rewind
			else if (opArguments_[i].length() >= 2 &&
					 opArguments_[i][0] == '/' && opArguments_[i][1] == '/')  // comment to the end
			{
				// erase remainder of arguments because they are commented out
				while (i < opArguments_.size())
					opArguments_.erase(opArguments_.begin() + i);  // erase
				break;
			}

		__COUTTV__(opArguments_.size());
		__COUTTV__(vectorToString(opArguments_));

		if (!opArguments_.size())  // skip no arguments
		{
			__COUTT__ << txtLineNumber_ << ": is empty" << __E__;
			continue;
		}

		// CFOLib::CFO_Compiler::CFO_MACRO macroOpTest = parseMacro(opArguments_[0]);
		// if(macroOpTest == CFO_MACRO::NON_MACRO)
		processOp();
		// else
		// 	processMacro(macroOpTest);

	}  // end main processing loop

	if (!hasRequiredPlanEndOp_)
	{
		__SS__ << "The Run Plan is missing an concluding operation that can be used as a moment to dynamically swith to the next run plan."
				  " At least one of these commands is required in your run plan: END, REPEAT, or GOTO MAIN."
			   << __E__;
		__SS_THROW__;
	}

	std::stringstream resultSs;
	resultSs << "Run plan text file: " << sourceCodeFile << __E__ << "was compiled to binary: " << binaryOutputFile << __E__;
	resultSs << "\n\nBinary Result (Op count = " << output_.size() / 8 << "):\n";
	int cnt = 0;

	// to view output file with 8-byte rows
	// hexdump -e '"%08_ax " 1/8 "%016x "' -e '"\n"' srcs/mu2e_pcie_utils/cfoInterfaceLib/Commands.bin
	std::ofstream outFile;
	outFile.open(binaryOutputFile.c_str(), std::ios::out | std::ios::binary);

	if (!(outFile.is_open()))
	{
		__SS__ << "Output File (" << binaryOutputFile << ") didn't open. Does it exist?" << __E__;
		__SS_THROW__;
	}
	char outStr[20];
	std::string binaryLine;  // build least-significant byte on right display
	std::string tabStr = "";
	int markerCnt = 0;
	int binaryLineNumber = 0;
	for (auto c : output_)
	{
		outFile << c;
		if (cnt % 8 == 0)
		{
			binaryLine = "";  // clear
			sprintf(outStr, "%5d", (cnt / 8 + 1));
			resultSs << "\n"
					 << tabStr << outStr << ": 0x";
		}
		else if (cnt % 4 == 0)
			binaryLine = " " + binaryLine;

		sprintf(outStr, "%2.2x", (uint16_t)c & 0x0FF);
		binaryLine = outStr + binaryLine;

		// show command for easier human understanding of binary
		if (cnt % 8 == 7)  // last byte in output is most-significant (and the opcode)
		{
			auto instr = CFOLib::CFO_Compiler::CFO_INSTR(c);
			resultSs << binaryLine << "     // " << translateOpCode(instr);
			if (instr == CFO_INSTR::MARKER)
				resultSs << " -----> #" << ++markerCnt;
			__COUTT__ << "binaryLine #" << ++binaryLineNumber << " = " << binaryLine << __E__;
			if (instr == CFO_INSTR::LOOP)
				tabStr += '\t';
			else if (instr == CFO_INSTR::DO_LOOP && tabStr.length())
				tabStr = tabStr.substr(0, tabStr.length() - 1);
		}

		++cnt;
	}
	resultSs << "\n";
	outFile.close();

	// to view output file with 8-byte rows
	// hexdump -e '"%08_ax " 1/8 "%016x "' -e '"\n"' srcs/mu2e_pcie_utils/cfoInterfaceLib/Commands.bin

	__COUT_INFO__ << "CFO_Compiler::processFile Parsing Complete!";
	return resultSs.str();
}  // end processFile()
catch (const std::runtime_error& e)
{
	__SS__ << "Error caught wile compiling source text at '"
		   << "<FILE>" << sourceCodeFile << "</FILE>' into binary run plan at '"
		   << "<FILE>" << binaryOutputFile << "</FILE>.'\n"
		   << e.what() << __E__;

	__SS_THROW__;
}
catch (...)
{
	__SS__ << "Unknown error caught wile compiling source text at '"
		   << "<FILE>" << sourceCodeFile << "</FILE>' into binary run plan at '"
		   << "<FILE>" << binaryOutputFile << "</FILE>.'\n"
		   << __E__;
	try
	{
		throw;
	}  // one more try to printout extra info
	catch (const std::exception& e)
	{
		ss << "Exception message: " << e.what();
	}
	catch (...)
	{}
	__COUT_ERR__ << "\n"
				 << ss.str();
	throw;
}  // end processFile() error handling

//========================================================================
// Boolean Operators
bool CFOLib::CFO_Compiler::isComment(const std::string& line)  // Checks if line has a comment.
{
	for (size_t i = 0; i < line.length() - 1; ++i)
	{
		if (line[i] == ' ' ||
			line[i] == '\t' ||
			line[i] == '\r' ||
			line[i] == '\n') continue;  // skip white space

		if (line[i] != '/') return false;  // some non-comment char found
		if (line[i + 1] != '/')
		{
			__SS__ << "Single slash found; missing double slash for comment. (Line " << txtLineNumber_ << ")" << __E__;
			__SS_THROW__;
		}
		return true;  // comment // string found
	}
	return true;  // for empty string, consider as comment
}  // end isComment()

const std::map<std::string, CFOLib::CFO_Compiler::CFO_INSTR> CFOLib::CFO_Compiler::OP_to_CODE_TRANSLATION = {
	{"HEARTBEAT", CFO_INSTR::HEARTBEAT},
	{"MARKER", CFO_INSTR::MARKER},
	{"DATA_REQUEST", CFO_INSTR::DATA_REQUEST},
	{"SET_TAG", CFO_INSTR::SET_TAG},
	{"INC_TAG", CFO_INSTR::INC_TAG},
	{"LOOP", CFO_INSTR::LOOP},
	{"DO_LOOP", CFO_INSTR::DO_LOOP},
	{"REPEAT", CFO_INSTR::REPEAT},
	{"WAIT", CFO_INSTR::WAIT},
	{"END", CFO_INSTR::END},
	{"GOTO_LABEL", CFO_INSTR::GOTO},
	{"LABEL", CFO_INSTR::LABEL},
	{"CLEAR_MODE_BITS", CFO_INSTR::CLEAR_MODE_BITS},
	{"SET_MODE_BITS", CFO_INSTR::SET_MODE_BITS},
	{"AND_MODE_BITS", CFO_INSTR::AND_MODE_BITS},
	{"OR_MODE_BITS", CFO_INSTR::OR_MODE_BITS},
	{"OR_SINGLESHOT_MODE_BITS", CFO_INSTR::OR_SINGLESHOT_MODE_BITS},
	{"SET_MODE", CFO_INSTR::SET_MODE},
};

//========================================================================
// Switch Conversions
// Turns strings into integers for switch statements.
CFOLib::CFO_Compiler::CFO_INSTR CFOLib::CFO_Compiler::parseInstruction(const std::string& op)
{
	try
	{
		return OP_to_CODE_TRANSLATION.at(op);
	}
	catch (...)
	{
		return CFO_INSTR::INVALID;
	}
}  // end parseInstruction()

const std::map<uint8_t, std::string> CFOLib::CFO_Compiler::CODE_to_OP_TRANSLATION = {
	{(uint8_t)CFOLib::CFO_Compiler::CFO_INSTR::HEARTBEAT, "HEARTBEAT"},
	{(uint8_t)CFOLib::CFO_Compiler::CFO_INSTR::MARKER, "MARKER"},
	{(uint8_t)CFOLib::CFO_Compiler::CFO_INSTR::DATA_REQUEST, "DATA_REQUEST"},
	{(uint8_t)CFOLib::CFO_Compiler::CFO_INSTR::SET_TAG, "SET_TAG"},
	{(uint8_t)CFOLib::CFO_Compiler::CFO_INSTR::INC_TAG, "INC_TAG"},
	{(uint8_t)CFOLib::CFO_Compiler::CFO_INSTR::LOOP, "LOOP"},
	{(uint8_t)CFOLib::CFO_Compiler::CFO_INSTR::DO_LOOP, "DO_LOOP"},
	{(uint8_t)CFOLib::CFO_Compiler::CFO_INSTR::REPEAT, "REPEAT"},
	{(uint8_t)CFOLib::CFO_Compiler::CFO_INSTR::WAIT, "WAIT"},
	{(uint8_t)CFOLib::CFO_Compiler::CFO_INSTR::END, "END"},
	{(uint8_t)CFOLib::CFO_Compiler::CFO_INSTR::GOTO, "GOTO_LABEL"},
	{(uint8_t)CFOLib::CFO_Compiler::CFO_INSTR::LABEL, "LABEL"},
	{(uint8_t)CFOLib::CFO_Compiler::CFO_INSTR::NOOP, "LABEL"},
	{(uint8_t)CFOLib::CFO_Compiler::CFO_INSTR::CLEAR_MODE_BITS, "CLEAR_MODE_BITS"},
	{(uint8_t)CFOLib::CFO_Compiler::CFO_INSTR::SET_MODE_BITS, "SET_MODE_BITS"},
	{(uint8_t)CFOLib::CFO_Compiler::CFO_INSTR::AND_MODE_BITS, "AND_MODE_BITS"},
	{(uint8_t)CFOLib::CFO_Compiler::CFO_INSTR::OR_MODE_BITS, "OR_MODE_BITS"},
	{(uint8_t)CFOLib::CFO_Compiler::CFO_INSTR::OR_SINGLESHOT_MODE_BITS, "OR_SINGLESHOT_MODE_BITS"},
	{(uint8_t)CFOLib::CFO_Compiler::CFO_INSTR::SET_MODE, "SET_MODE"},
};

//========================================================================
const std::string& CFOLib::CFO_Compiler::translateOpCode(CFOLib::CFO_Compiler::CFO_INSTR opCode)
{
	try
	{
		return CODE_to_OP_TRANSLATION.at((uint8_t)opCode);
	}
	catch (...)
	{
		__SS__ << "No tranlation found for opCode " << (int)opCode << __E__;
		__SS_THROW__;
	}
}  // end translateOpCode()

//========================================================================
void CFOLib::CFO_Compiler::outParameter(uint64_t paramBuf)  // Writes the 6 byte parameter out based on a given integer.
{
	paramBuf &= 0xFFFFFFFFFFFF;  // Enforce 6 bytes
	auto paramBufPtr = reinterpret_cast<int8_t*>(&paramBuf);
	for (int i = 0; i < 6; ++i)
	{
		output_.push_back(paramBufPtr[i]);
		__COUTT__ << "[" << output_.size() << "] ==> output_ 0x" << std::hex << ((uint16_t)output_.back() & 0x0FF) << __E__;
	}
}  // end outParameter()

//========================================================================
// processOp
// 		Outputs a byte stream based on the buffers.
//		opArguments_ must be checked before to be size >= 1
void CFOLib::CFO_Compiler::processOp()
{
	// calculate line numbers that match this hexdump call (all usage of bin is relative/differences):
	//		hexdump -e '"%08_ax | " 1/8 "%016x "' -e '"\n"'  CFOCommands.bin | cat -n
	binLineNumber_ = 1 + output_.size() / 8;
	__COUTT__ << "binLineNumber_: " << (int)binLineNumber_ << __E__;

	CFO_INSTR instructionOpcode = parseInstruction(opArguments_[0]);
	if (instructionOpcode == CFO_INSTR::INVALID)
	{
		__SS__ << "On Line " << txtLineNumber_ << ", invalid instruction '" << opArguments_[0] << "' found. " << __E__;
		__SS_THROW__;
	}

	int64_t parameterCalc = calculateParameterAndErrorCheck(instructionOpcode);
	__COUTTV__((int)instructionOpcode);
	__COUTTV__(parameterCalc);

	if (instructionOpcode == CFO_INSTR::SET_MODE_BITS ||
		instructionOpcode == CFO_INSTR::SET_MODE)
	{
		__COUTT__ << "MASK off 0x" << std::hex << modeClearMask_ << __E__;
		// treat as two ops: an AND and an OR
		outParameter(modeClearMask_);
		output_.push_back(0x00);
		__COUTT__ << "[" << output_.size() << "] ==> output_ 0x" << std::hex << std::setfill('0') << std::setprecision(2) << (uint16_t)output_.back() << __E__;
		output_.push_back(static_cast<char>(CFO_INSTR::AND_MODE_BITS));
		__COUTT__ << "[" << output_.size() << "] ==> output_ 0x" << std::hex << std::setfill('0') << std::setprecision(2) << (uint16_t)output_.back() << __E__;

		instructionOpcode = CFO_INSTR::OR_MODE_BITS;
		__COUTT__ << "MASK on 0x" << std::hex << parameterCalc << __E__;
	}

	outParameter(parameterCalc);

	if (instructionOpcode == CFO_INSTR::REPEAT ||
		instructionOpcode == CFO_INSTR::END ||
		instructionOpcode == CFO_INSTR::GOTO)
		hasRequiredPlanEndOp_ = true;
	else if (instructionOpcode == CFO_INSTR::LABEL)  // in binary, treat as NOOP
		instructionOpcode = CFO_INSTR::NOOP;
	else if (instructionOpcode == CFO_INSTR::CLEAR_MODE_BITS)  // treat clear as AND
		instructionOpcode = CFO_INSTR::AND_MODE_BITS;

	output_.push_back(0x00);
	__COUTT__ << "[" << output_.size() << "] ==> output_ 0x" << std::hex << std::setfill('0') << std::setprecision(2) << (uint16_t)output_.back() << __E__;
	output_.push_back(static_cast<char>(instructionOpcode));
	__COUTT__ << "[" << output_.size() << "] ==> output_ 0x" << std::hex << std::setfill('0') << std::setprecision(2) << (uint16_t)output_.back() << __E__;

}  // end processOp()

//========================================================================
// Parameter Calculations
// 	Calculates the parameter for the instruction (if needed) and does error checking
uint64_t CFOLib::CFO_Compiler::calculateParameterAndErrorCheck(CFO_INSTR instructionOpcode)
{
	size_t opArgCount = opArguments_.size();
	modeClearMask_ = 0;  // clear

	__COUTT__ << "calculateParameterAndErrorCheck... Instruction: " << opArguments_[0] << ", Parameter count: " << opArgCount << __E__;

	uint64_t value;

	switch (instructionOpcode)
	{
		case CFO_INSTR::HEARTBEAT:
			if (opArgCount != 3 || opArguments_[1] != "event_mode")
			{
				__SS__ << "On Line " << txtLineNumber_ << ", invalid '" << opArguments_[0] << "' arguments found. There must be 1 named parameter, and " << opArgCount / 2 << " were found. Here is the syntax := \"" << opArguments_[0] << " event_mode=[value]\" ... "
																																																											"The source instruction was \""
					   << vectorToString(opArguments_, " " /*delimieter*/) << ".\"\n\n"
																			  "The event_mode value can be the string \"registered\" to use the locally maintained event_mode value of the compiler,"
																			  " otherwise a number (hex 0x### and binary b### syntax allowed) for event_mode should be specified (0 for null HEARTBEAT)."
					   << __E__;
				__SS_THROW__;
			}

			if (opArguments_[2] == "registered")
				return -1;  // indicate to FPGA to use registered event mode

			if (!getNumber(opArguments_[2], value))
			{
				__SS__ << "On Line (" << txtLineNumber_ << "), for instruction '" << opArguments_[0] << ",' "
					   << "the parameter value '" << opArguments_[1] << " = " << opArguments_[2] << "' is not a valid number. "
					   << "Use 0x### to indicate hex and b### to indicate binary; otherwise, decimal is inferred." << __E__;
				__SS_THROW__;
			}
			return value;

		case CFO_INSTR::MARKER:
			if (opArgCount != 1)
			{
				__SS__ << "On Line " << txtLineNumber_ << ", '" << opArguments_[0] << "' has extraneous arguments; there must be no additional arguments." << std::endl;
				__SS_THROW__;
			}
			break;

		case CFO_INSTR::DATA_REQUEST:
			if (opArgCount != 3 || opArguments_[1] != "request_tag")
			{
				__SS__ << "On Line " << txtLineNumber_ << ", invalid '" << opArguments_[0] << "' arguments found. There must be 1 named parameter, and " << opArgCount / 2 << " were found. Here is the syntax := \"" << opArguments_[0] << " request_tag=[value]\n\n\""
																																																											" The request_tag value can be the string \"current\" to request the most recent Event Window Tag data,"
																																																											" otherwise a number (hex 0x### and binary b### syntax allowed) for request_tag should be specified."
					   << __E__;
				__SS_THROW__;
			}
			if (opArguments_[2] == "current")
				return -1;
			if (!getNumber(opArguments_[2], value))
			{
				__SS__ << "On Line (" << txtLineNumber_ << "), for instruction '" << opArguments_[0] << ",' "
					   << "the parameter value '" << opArguments_[1] << " = " << opArguments_[2] << "' is not a valid number. "
					   << "Use 0x### to indicate hex and b### to indicate binary; otherwise, decimal is inferred." << __E__;
				__SS_THROW__;
			}
			return value;

		case CFO_INSTR::SET_TAG:
			if (opArgCount != 2)
			{
				__SS__ << "On Line " << txtLineNumber_ << ", invalid '" << opArguments_[0] << "' arguments found. There must be 2 arguments, and " << opArgCount << " were found. Here is the syntax := \"" << opArguments_[0] << " [value]\" ... "
																																																								  "The source instruction was \""
					   << vectorToString(opArguments_, " " /*delimieter*/) << ".\"\n\n"
																			  "For the set tag number, hex 0x### and binary b### syntax is allowed, otherwise decimal is inferred."
					   << __E__;
				__SS_THROW__;
			}
			if (!getNumber(opArguments_[1], value))
			{
				__SS__ << "On Line (" << txtLineNumber_ << "), for instruction '" << opArguments_[0] << ",' "
					   << "the parameter value '" << opArguments_[1] << "' is not a valid number. "
					   << "Use 0x### to indicate hex and b### to indicate binary; otherwise, decimal is inferred." << __E__;
				__SS_THROW__;
			}
			return value;

		case CFO_INSTR::INC_TAG:
			if (opArgCount == 1)  // default to increment by 1 if no value
				return 1;
			if (opArgCount != 3 || opArguments_[1] != "add_value")
			{
				__SS__ << "On Line " << txtLineNumber_ << ", invalid '" << opArguments_[0] << "' arguments found. There must be 0 or 1 named parameter, and " << opArgCount << " were found. Here is the syntax := \"" << opArguments_[0] << " add_value=[value]\" ... "
																																																											 "The source instruction was \""
					   << vectorToString(opArguments_, " " /*delimieter*/) << ".\"\n\n"
																			  " If no named parameter, then increment by 1 is assumed,"
																			  " otherwise a number (hex 0x### and binary b### syntax allowed) for add_value should be specified."
					   << __E__;
				__SS_THROW__;
			}

			if (!getNumber(opArguments_[2], value))
			{
				__SS__ << "On Line (" << txtLineNumber_ << "), for instruction '" << opArguments_[0] << ",' "
					   << "the parameter value '" << opArguments_[1] << " = " << opArguments_[2] << "' is not a valid number. "
					   << "Use 0x### to indicate hex and b### to indicate binary; otherwise, decimal is inferred." << __E__;
				__SS_THROW__;
			}
			return value;

		case CFO_INSTR::WAIT: {
			if (opArgCount != 3 ||
				(opArguments_[2] != "clocks" && opArguments_[2] != "ns" &&
				 opArguments_[2] != "s" && opArguments_[2] != "ms" && opArguments_[2] != "us" &&
				 opArguments_[2] != "RF0") ||
				(opArguments_[1] != "NEXT" && opArguments_[2] == "RF0"))
			{
				__SS__ << "On Line " << txtLineNumber_ << ", invalid '" << opArguments_[0] << "' arguments found. There must be 3 arguments, and " << opArgCount << " were found. Here is the syntax := \"" << opArguments_[0] << " [value] [units]\" or \"" << opArguments_[0] << " NEXT RF0\" ... "
																																																																				   "The source instruction was \""
					   << vectorToString(opArguments_, " " /*delimieter*/) << ".\"\n\n"
																			  "For the units, accepted unit strings are clocks, ns, us, ms, and s.\n\n"
					   << "If NEXT RF0 is provided, then the '" << opArguments_[0] << "' will last until the next RF-0 accelerator marker is received.\n\n"
					   << "For the wait value, hex 0x### and binary b### syntax is allowed, otherwise decimal is inferred." << __E__;
				__SS_THROW__;
			}
			if (opArguments_[1] == "NEXT")
				return -1;  // use all 1s to indicate wait for next RF-0 marker

			if (!getNumber(opArguments_[1], value))
			{
				__SS__ << "On Line (" << txtLineNumber_ << "), for instruction '" << opArguments_[0] << ",' "
					   << "the parameter value '" << opArguments_[1] << " " << opArguments_[2] << "' is not a valid number. "
					   << "Use 0x### to indicate hex and b### to indicate binary; otherwise, decimal is inferred." << __E__;
				__SS_THROW__;
			}
			// test floating point in case integer conversion dropped something
			double timeValue = strtod(opArguments_[1].c_str(), 0);
			__COUTTV__(timeValue);
			if (timeValue < value)
				timeValue = value;

			__COUTTV__(FPGAClock_);
			__COUTTV__(value);
			__COUTTV__(timeValue);

			if (opArguments_[2] == "s")  // Wait wanted in seconds
				return timeValue * 1e9 / FPGAClock_;
			else if (opArguments_[2] == "ms")  // Wait wanted in milliseconds
				return timeValue * 1e6 / FPGAClock_;
			else if (opArguments_[2] == "us")  // Wait wanted in microseconds
				return timeValue * 1e3 / FPGAClock_;
			else if (opArguments_[2] == "ns")  // Wait wanted in nanoseconds
			{
				if ((value % FPGAClock_) != 0)
				{
					__SS__ << "FPGA can only wait in multiples of " << FPGAClock_ << " ns: the input value '" << value << "' yields a remainder of " << (value % FPGAClock_) << __E__;
					__SS_THROW__;
				}
				return value / FPGAClock_;
			}
			else if (opArguments_[2] == "clocks")  // Wait wanted in FPGA clocks
				return value;
			else  // impossible
			{
				__SS__ << "WAIT command is missing unit type after parameter. Accepted unit types are clocks, ns, us, ms, and s." << __E__;
				__SS_THROW__;
			}
		}
		case CFO_INSTR::LOOP:
			if (opArgCount != 2)
			{
				__SS__ << "On Line " << txtLineNumber_ << ", invalid '" << opArguments_[0] << "' arguments found. There must be 2 arguments, and " << opArgCount << " were found. Here is the syntax := \"" << opArguments_[0] << " [value]\" ... "
																																																								  "The source instruction was \""
					   << vectorToString(opArguments_, " " /*delimieter*/) << ".\"\n\n"
																			  "For the loop count, hex 0x### and binary b### syntax is allowed, otherwise decimal is inferred."
					   << __E__;
				__SS_THROW__;
			}
			if (!getNumber(opArguments_[1], value))
			{
				__SS__ << "On Line (" << txtLineNumber_ << "), for instruction '" << opArguments_[0] << ",' "
					   << "the parameter value '" << opArguments_[1] << "' is not a valid number. "
					   << "Use 0x### to indicate hex and b### to indicate binary; otherwise, decimal is inferred." << __E__;
				__SS_THROW__;
			}
			__COUTT__ << "loopStack_ = " << binLineNumber_ << " at " << loopStack_.size() << __E__;
			loopStack_.push_back(binLineNumber_);
			return value;

		case CFO_INSTR::DO_LOOP:
			if (opArgCount != 1)
			{
				__SS__ << "On Line " << txtLineNumber_ << ", '" << opArguments_[0] << "' has extraneous arguments; there must be no additional arguments." << std::endl;
				__SS_THROW__;
			}
			if (!loopStack_.empty())
			{
				uint64_t loopLine;
				loopLine = loopStack_.back();
				value = (binLineNumber_ - loopLine);
				__COUTT__ << "DO_LOOP from [" << binLineNumber_ << "], loop line popped = " << loopLine << ", must go back	" << value << " lines.";
				loopStack_.pop_back();
				__COUTT__ << "DO_LOOP loopStack_ " << loopStack_.size() << " w/parameterCalc=" << value;
				return value;
			}
			else
			{
				__SS__ << "Identified at Line " << txtLineNumber_ << ", LOOP/DO_LOOP counts don't match. (There are more DO_LOOP's)";
				__SS_THROW__;
			}
			break;  // impossible to get here
		case CFO_INSTR::REPEAT:
		case CFO_INSTR::END:
			if (opArgCount != 1)
			{
				__SS__ << "On Line " << txtLineNumber_ << ", '" << opArguments_[0] << "' has extraneous arguments; there must be no additional arguments." << std::endl;
				__SS_THROW__;
			}
			if (!loopStack_.empty())  // loop stack must be empty at END or REPEAT
			{
				__SS__ << "Identified at Line " << txtLineNumber_ << ", LOOP/DO_LOOP counts don't match (There are less DO_LOOP's)";
				__SS_THROW__;
			}
			break;

		case CFO_INSTR::GOTO:
			if (opArgCount != 1)
			{
				__SS__ << "On Line " << txtLineNumber_ << ", '" << opArguments_[0] << "' has extraneous arguments; there must be no additional arguments." << std::endl;
				__SS_THROW__;
			}

			if (!loopStack_.empty())  // loop stack must be empty at GOTO
			{
				__SS__ << "Identified at Line " << txtLineNumber_ << ", LOOP/DO_LOOP counts don't match (There are less DO_LOOP's); a GOTO can not be used to exit a loop.";
				__SS_THROW__;
			}
			opArguments_.push_back(MAIN_GOTO_LABEL);  // force label MAIN because only one GOTO ever makes sense if jumping in and out of loops is not allowed (since there are on IF statements)
			opArguments_[1] = MAIN_GOTO_LABEL;        // in case of comments, push_back doesnt work
			__COUTT__ << "Goto lookup label '" << opArguments_[1] << "'" << __E__;

			if (labelMap_.find(opArguments_[1]) == labelMap_.end())
			{
				for (auto& labelPair : labelMap_)
					__COUTTV__(labelPair.first);
				__SS__ << "Missing label '" << opArguments_[1] << "' needed for GOTO at line " << txtLineNumber_ << __E__;
				__SS_THROW__;
			}
			return labelMap_.at(opArguments_[1]);

		case CFO_INSTR::LABEL:
			if (opArgCount != 1)
			{
				__SS__ << "On Line " << txtLineNumber_ << ", '" << opArguments_[0] << "' has extraneous arguments; there must be no additional arguments." << std::endl;
				__SS_THROW__;
			}

			if (!loopStack_.empty())  // loop stack must be empty at GOTO
			{
				__SS__ << "Identified at Line " << txtLineNumber_ << ", LOOP/DO_LOOP counts don't match (There are less DO_LOOP's); a GOTO_LABEL can not be used to enter a loop.";
				__SS_THROW__;
			}

			// for now, leave label map in case if statements exist in the future, and more lables are allowed.
			opArguments_.push_back(MAIN_GOTO_LABEL);
			opArguments_[1] = MAIN_GOTO_LABEL;  // in case of comments, push_back doesnt work

			__COUTT__ << "Saving map for label '" << opArguments_[1] << "' to line " << binLineNumber_ << __E__;
			if (labelMap_.find(opArguments_[1]) != labelMap_.end())
			{
				__SS__ << "On Line " << txtLineNumber_ << ", " << opArguments_[0] << " encountered when one already exists in the run plan. Only one " << opArguments_[0] << " allowed in a run plan." << __E__;
				__SS_THROW__;
			}
			labelMap_[opArguments_[1]] = binLineNumber_;
			return 1;  // indicate MAIN LABEL to CFO firmware as valid location to dynamically switch run plans
					   // otherwise, a normal noop label
		case CFO_INSTR::SET_MODE_BITS:
		case CFO_INSTR::AND_MODE_BITS:
		case CFO_INSTR::OR_MODE_BITS:
		case CFO_INSTR::OR_SINGLESHOT_MODE_BITS: {
			if (opArgCount != 7 ||
				opArguments_[1] != "start_bit" ||
				opArguments_[3] != "bit_count" ||
				opArguments_[5] != "value")
			{
				__SS__ << "On Line " << txtLineNumber_ << ", invalid '" << opArguments_[0] << "' arguments found. There must be 7 arguments, and " << opArgCount << " were found. Here is the syntax := \"" << opArguments_[0] << " start_bit=[value] bit_count=[value] value=[value]\" ... "
																																																								  "The source instruction was \""
					   << vectorToString(opArguments_, " " /*delimieter*/) << ".\"\n\n"
																			  "For the number values, hex 0x### and binary b### syntax is allowed, otherwise decimal is inferred."
					   << __E__;
				__SS_THROW__;
			}

			uint16_t startBit;
			if (!getNumber(opArguments_[2], startBit))
			{
				__SS__ << "On Line (" << txtLineNumber_ << "), the "
					   << "start_bit value '" << opArguments_[2] << "' is not a valid number. "
					   << "Use 0x### to indicate hex and b### to indicate binary; otherwise, decimal is inferred." << __E__;
				__SS_THROW__;
			}

			if (startBit > 47)
			{
				__SS__ << "On Line (" << txtLineNumber_ << "), "
					   << "start_bit value must be an integer between 0 and 47." << __E__;
				__SS_THROW__;
			}

			uint16_t bitCount;
			if (!getNumber(opArguments_[4], bitCount))
			{
				__SS__ << "On Line (" << txtLineNumber_ << "), the "
					   << "bit_count value '" << opArguments_[4] << "' is not a valid number. "
					   << "Use 0x### to indicate hex and b### to indicate binary; otherwise, decimal is inferred." << __E__;
				__SS_THROW__;
			}

			if (startBit + bitCount > 48 || bitCount == 0)
			{
				__SS__ << "On Line (" << txtLineNumber_ << "), "
					   << "bit_count value must be a positive integer and fit within 48-bits range 0-to-47 with offset start_bit=" << startBit << ". The end bit was calculated as end_bit=" << startBit + bitCount - 1 << __E__;
				__SS_THROW__;
			}

			if (!getNumber(opArguments_[6], value))
			{
				__SS__ << "On Line (" << txtLineNumber_ << "), the "
					   << "bit_count value '" << opArguments_[6] << "' is not a valid number. "
					   << "Use 0x### to indicate hex and b### to indicate binary; otherwise, decimal is inferred." << __E__;
				__SS_THROW__;
			}

			if (value >= (uint64_t(1) << bitCount) &&
				opArguments_[6][0] != '~')  // only if not an inverted value
			{
				__SS__ << "On Line (" << txtLineNumber_ << "), "
					   << "the set value range is defined by bit_count, and so must be an integer between 0 and 2^"
					   << "(bit_count=" << bitCount << ") - 1. The value is " << value << " which is greater than or equal to max range = " << (uint64_t(1) << bitCount) << __E__;
				ss << "\n\n"
				   << "If an inverted value is intended, use '~' before the number to indicate inversion, and the value will be masked to fit within the bit_count range.";
				__SS_THROW__;
			}

			__COUTTV__(startBit);
			__COUTTV__(bitCount);
			__COUTTV__(value);
			uint64_t bitmask = 0;
			for (uint16_t i = startBit; i < startBit + bitCount; ++i)
				bitmask |= (uint64_t(1) << i);

			switch (instructionOpcode)
			{
				case CFO_INSTR::SET_MODE_BITS:  // treat SET_MODE_BITS as two ops in hardware: an AND and an OR

					__COUTT__ << "bitmask 0x" << std::hex << bitmask << __E__;
					value <<= startBit;         // shift then mask
					value &= bitmask;           // force bitcount in case of ~ inverted value
					modeClearMask_ = ~bitmask;  // CLEAR
					__COUTT__ << "modeClearMask_ 0x" << std::hex << modeClearMask_ << __E__;
					return value;  // SET

				case CFO_INSTR::AND_MODE_BITS:

					value <<= startBit;  // shift then mask
					value |= ~bitmask;   // force ignore outside of bitcount in case of ~ inverted value
					return value;        // AND

				case CFO_INSTR::OR_MODE_BITS:
				case CFO_INSTR::OR_SINGLESHOT_MODE_BITS:

					value <<= startBit;  // shift then mask
					value &= bitmask;    // force ignore outside of bitcount in case of ~ inverted value
					return value;        // OR / OR (single-shot)

				default: {
					__COUTTV__((int)instructionOpcode);
					__SS__ << "On Line " << txtLineNumber_ << ", invalid instruction '" << opArguments_[0] << "' encountered.'" << __E__;
					__SS_THROW__;
				}
			}
		}
		case CFO_INSTR::CLEAR_MODE_BITS: {
			if (opArgCount != 5 ||
				opArguments_[1] != "start_bit" ||
				opArguments_[3] != "bit_count")
			{
				__SS__ << "On Line " << txtLineNumber_ << ", invalid '" << opArguments_[0] << "' arguments found. There must be 5 arguments, and " << opArgCount << " were found. Here is the syntax := \"" << opArguments_[0] << " start_bit=[value] bit_count=[value]\" ... "
																																																								  "The source instruction was \""
					   << vectorToString(opArguments_, " " /*delimieter*/) << ".\"\n\n"
																			  "For the number values, hex 0x### and binary b### syntax is allowed, otherwise decimal is inferred."
					   << __E__;
				__SS_THROW__;
			}

			uint16_t startBit;
			if (!getNumber(opArguments_[2], startBit))
			{
				__SS__ << "On Line (" << txtLineNumber_ << "), the "
					   << "start_bit value '" << opArguments_[2] << "' is not a valid number. "
					   << "Use 0x### to indicate hex and b### to indicate binary; otherwise, decimal is inferred." << __E__;
				__SS_THROW__;
			}

			if (startBit > 47)
			{
				__SS__ << "On Line (" << txtLineNumber_ << "), "
					   << "start_bit value must be an integer between 0 and 47." << __E__;
				__SS_THROW__;
			}

			uint16_t bitCount;
			if (!getNumber(opArguments_[4], bitCount))
			{
				__SS__ << "On Line (" << txtLineNumber_ << "), the "
					   << "bit_count value '" << opArguments_[4] << "' is not a valid number. "
					   << "Use 0x### to indicate hex and b### to indicate binary; otherwise, decimal is inferred." << __E__;
				__SS_THROW__;
			}

			if (startBit + bitCount > 48 || bitCount == 0)
			{
				__SS__ << "On Line (" << txtLineNumber_ << "), "
					   << "bit_count value must be a positive integer and fit within 48-bits range 0:47 with offset start_bit=" << startBit << ". The end bit was calculated as end_bit=" << startBit + bitCount - 1 << __E__;
				__SS_THROW__;
			}

			uint64_t bitmask = 0;
			for (uint16_t i = startBit; i < startBit + bitCount; ++i)
				bitmask |= (uint64_t(1) << i);
			return ~bitmask;  // CLEAR
		}
		case CFO_INSTR::SET_MODE:  // treat SET_MODE as two ops in hardware: an AND and an OR
		{
			if (opArgCount != 2)
			{
				__SS__ << "On Line " << txtLineNumber_ << ", invalid '" << opArguments_[0] << "' arguments found. There must be 5 arguments, and " << opArgCount << " were found. Here is the syntax := \"" << opArguments_[0] << " [value]\" ... "
																																																								  "The source instruction was \""
					   << vectorToString(opArguments_, " " /*delimieter*/) << ".\"\n\n"
																			  "For the Event Mode value, hex 0x### and binary b### syntax is allowed, otherwise decimal is inferred."
					   << __E__;
				__SS_THROW__;
			}

			if (!getNumber(opArguments_[1], value))
			{
				__SS__ << "On Line (" << txtLineNumber_ << "), the "
					   << "start_bit value '" << opArguments_[1] << "' is not a valid number. "
					   << "Use 0x### to indicate hex and b### to indicate binary; otherwise, decimal is inferred." << __E__;
				__SS_THROW__;
			}

			// treat SET_MODE as two ops in hardware: an AND and an OR
			modeClearMask_ = 0;  // CLEAR
			return value;        // SET
		}
		default: {
			__COUTTV__((int)instructionOpcode);
			__SS__ << "On Line " << txtLineNumber_ << ", invalid instruction '" << opArguments_[0] << "' encountered.'" << __E__;
			__SS_THROW__;
		}
	}  // end primary switch statement

	return 0;  // default parameter value
}  // end calculateParameterAndErrorCheck()

//==============================================================================
// static template function (copied from ots::StringMacros)
//	for all numbers, but not bools (bools has a specialized template definition)
//	return false if string is not a number
template<class T>
bool getNumber(const std::string& s, T& retValue)
{
	__COUTVS__(2, s);

	// extract set of potential numbers and operators
	std::vector<std::string> numbers;
	std::vector<char> ops;

	getVectorFromString(s,
						numbers,
						/*delimiter*/ std::set<char>({'+', '-', '*', '/', '~'}),
						/*whitespace*/ std::set<char>({' ', '\t', '\n', '\r'}),
						&ops);

	__COUTVS__(2, vectorToString(numbers));
	__COUTVS__(2, vectorToString(ops));

	retValue = 0;  // initialize

	T tmpValue;

	unsigned int i = 0;
	unsigned int opsi = 0;
	unsigned int blankNumberCount = 0;
	bool verified;
	for (const auto& number : numbers)
	{
		if (number.size() == 0)
		{
			++blankNumberCount;
			continue;  // skip empty numbers
		}

		// verify that this number looks like a number
		//	for integer types, allow hex and binary
		//	for all types allow base10

		verified = false;

		__COUTVS__(2, number);

		// check integer types
		if (typeid(unsigned int) == typeid(retValue) || typeid(int) == typeid(retValue) || typeid(unsigned long long) == typeid(retValue) ||
			typeid(long long) == typeid(retValue) || typeid(unsigned long) == typeid(retValue) || typeid(long) == typeid(retValue) ||
			typeid(unsigned short) == typeid(retValue) || typeid(short) == typeid(retValue) || typeid(uint8_t) == typeid(retValue))
		{
			if (number.find("0x") == 0)  // indicates hex
			{
				__COUTS__(2) << "0x found" << __E__;
				for (unsigned int i = 2; i < number.size(); ++i)
				{
					if (!((number[i] >= '0' && number[i] <= '9') || (number[i] >= 'A' && number[i] <= 'F') || (number[i] >= 'a' && number[i] <= 'f')))
					{
						__COUT__ << "prob " << number[i] << __E__;
						return false;
					}
				}
				verified = true;
			}
			else if (number[0] == 'b')  // indicates binary
			{
				__COUTS__(2) << "b found" << __E__;

				for (unsigned int i = 1; i < number.size(); ++i)
				{
					if (!((number[i] >= '0' && number[i] <= '1')))
					{
						__COUT__ << "prob " << number[i] << __E__;
						return false;
					}
				}
				verified = true;
			}
		}

		// if not verified above, for all types, check base10
		if (!verified)
			for (unsigned int i = 0; i < number.size(); ++i)
				if (!((number[i] >= '0' && number[i] <= '9') || number[i] == '.' || number[i] == '+' || number[i] == '-'))
					return false;

		// at this point, this number is confirmed to be a number of some sort
		// so convert to temporary number
		if (typeid(double) == typeid(retValue))
			tmpValue = strtod(number.c_str(), 0);
		else if (typeid(float) == typeid(retValue))
			tmpValue = strtof(number.c_str(), 0);
		else if (typeid(unsigned int) == typeid(retValue) || typeid(int) == typeid(retValue) || typeid(unsigned long long) == typeid(retValue) ||
				 typeid(long long) == typeid(retValue) || typeid(unsigned long) == typeid(retValue) || typeid(long) == typeid(retValue) ||
				 typeid(unsigned short) == typeid(retValue) || typeid(short) == typeid(retValue) || typeid(uint8_t) == typeid(retValue))
		{
			__COUTVS__(2, number);
			if (number.size() > 2 && number[1] == 'x')  // assume hex value
				tmpValue = (T)strtol(number.c_str(), 0, 16);
			else if (number.size() > 1 && number[0] == 'b')            // assume binary value
				tmpValue = (T)strtol(number.substr(1).c_str(), 0, 2);  // skip first 'b' character
			else
				tmpValue = (T)strtol(number.c_str(), 0, 10);
		}
		else  // just try!
		{
			__COUTVS__(2, number);
			if (number.size() > 2 && number[1] == 'x')  // assume hex value
				tmpValue = (T)strtol(number.c_str(), 0, 16);
			else if (number.size() > 1 && number[0] == 'b')            // assume binary value
				tmpValue = (T)strtol(number.substr(1).c_str(), 0, 2);  // skip first 'b' character
			else
				tmpValue = (T)strtol(number.c_str(), 0, 10);
			// __SS__ << "Invalid type '" << StringMacros::demangleTypeName(typeid(retValue).name()) << "' requested for a numeric string. Data was '" << number
			//        << "'" << __E__;
			// __SS_THROW__;
		}

		__COUTVS__(2, tmpValue);

		// apply operation
		if (i == 0)  // first value, no operation, just take value
		{
			retValue = tmpValue;

			if (ops.size() == numbers.size() - blankNumberCount)  // then there is a leading operation, so apply
			{
				if (ops[opsi] == '-')  // only meaningful op is negative sign
					retValue *= -1;
				else if (ops[opsi] == '~')  // handle bit invert
					retValue = ~tmpValue;
				__COUTT__ << "Op " << (ops.size() ? ops[opsi] : '_') << " intermediate value = " << std::dec << retValue << " 0x" << std::hex << retValue << __E__;
				opsi++;  // jump to first internal op
			}
		}
		else  // there is some sort of op
		{
			if (0 && i == 1)  // display what we are dealing with
			{
				__COUTTV__(vectorToString(numbers));
				__COUTTV__(vectorToString(ops));
			}
			__COUTTV__(opsi);
			__COUTTV__(ops[opsi]);
			__COUTTV__(tmpValue);
			__COUTT__ << "Intermediate value = " << std::dec << retValue << " 0x" << std::hex << retValue << __E__;

			switch (ops[opsi])
			{
				case '+':
					retValue += tmpValue;
					break;
				case '-':
					retValue -= tmpValue;
					break;
				case '*':
					retValue *= tmpValue;
					break;
				case '/':
					retValue /= tmpValue;
					break;
				default:
					__SS__ << "Unrecognized operation '" << ops[opsi] << "' found!" << __E__ << "Numbers: " << vectorToString(numbers) << __E__
						   << "Operations: " << vectorToString(ops) << __E__;
					__SS_THROW__;
			}

			__COUTT__ << "Op " << (ops.size() ? ops[opsi] : '_') << " intermediate value = " << std::dec << retValue << " 0x" << std::hex << retValue << __E__;
			++opsi;
		}
		__COUTT__ << i << ": Op intermediate value = " << std::dec << retValue << " 0x" << std::hex << retValue << __E__;

		++i;  // increment index for next number/op

	}  // end number loop

	return true;  // number was valid and is passed by reference in retValue
}  // end static getNumber<T>()

//==============================================================================
// getVectorFromString (copied from ots::StringMacros)
//	extracts the list of elements from string that uses a delimiter
//		ignoring whitespace
//	optionally returns the list of delimiters encountered, which may be useful
//		for extracting which operator was used.
//
//
//	Note: lists are returned as vectors
//	Note: the size() of delimiters will be one less than the size() of the returned values
//		unless there is a leading delimiter, in which case vectors will have the same
// size.
void getVectorFromString(const std::string& inputString,
						 std::vector<std::string>& listToReturn,
						 const std::set<char>& delimiter,
						 const std::set<char>& whitespace,
						 std::vector<char>* listOfDelimiters
						 /* dont care about URI in compiler ,
						 bool                      decodeURIComponents */
)
{
	unsigned int i = 0;
	unsigned int j = 0;
	unsigned int c = 0;
	std::set<char>::iterator delimeterSearchIt;
	char lastDelimiter = 0;
	bool isDelimiter;
	// bool foundLeadingDelimiter = false;

	// __COUT__ << inputString << __E__;
	// __COUTV__(inputString.length());

	// go through the full string extracting elements
	// add each found element to set
	for (; c < inputString.size(); ++c)
	{
		// __COUT__ << (char)inputString[c] << __E__;

		delimeterSearchIt = delimiter.find(inputString[c]);
		isDelimiter = delimeterSearchIt != delimiter.end();

		// __COUT__ << (char)inputString[c] << " " << isDelimiter << __E__;

		if (whitespace.find(inputString[c]) != whitespace.end()  // ignore leading white space
			&& i == j)
		{
			++i;
			++j;
			// if(isDelimiter)
			//	foundLeadingDelimiter = true;
		}
		else if (whitespace.find(inputString[c]) != whitespace.end() && i != j)  // trailing white space, assume possible end of element
		{
			// do not change j or i
		}
		else if (isDelimiter)  // delimiter is end of element
		{
			// __COUT__ << "Set element found: " <<
			// 		inputString.substr(i,j-i) << " sz=" << inputString.substr(i,j-i).length() << std::endl;

			if (listOfDelimiters && listToReturn.size())  // || foundLeadingDelimiter))
														  // //accept leading delimiter
														  // (especially for case of
														  // leading negative in math
														  // parsing)
			{
				//__COUTV__(lastDelimiter);
				listOfDelimiters->push_back(lastDelimiter);
			}
			listToReturn.push_back(  // decodeURIComponents ? StringMacros::decodeURIComponent(inputString.substr(i, j - i)) :
				inputString.substr(i, j - i));

			// setup i and j for next find
			i = c + 1;
			j = c + 1;
		}
		else  // part of element, so move j, not i
			j = c + 1;

		if (isDelimiter)
			lastDelimiter = *delimeterSearchIt;
		//__COUTV__(lastDelimiter);
	}

	if (1)  // i != j) //last element check (for case when no concluding ' ' or delimiter)
	{
		// __COUT__ << "Last element found: " <<
		// 		inputString.substr(i,j-i) << std::endl;

		if (listOfDelimiters && listToReturn.size())  // || foundLeadingDelimiter))
													  // //accept leading delimiter
													  // (especially for case of leading
													  // negative in math parsing)
		{
			//__COUTV__(lastDelimiter);
			listOfDelimiters->push_back(lastDelimiter);
		}
		listToReturn.push_back(  // decodeURIComponents ? StringMacros::decodeURIComponent(inputString.substr(i, j - i)) :
			inputString.substr(i, j - i));
	}

	// assert that there is one less delimiter than values
	if (listOfDelimiters && listToReturn.size() - 1 != listOfDelimiters->size() && listToReturn.size() != listOfDelimiters->size())
	{
		__SS__ << "There is a mismatch in delimiters to entries (should be equal or one "
				  "less delimiter): "
			   << listOfDelimiters->size() << " vs " << listToReturn.size() << __E__ << "Entries: " << vectorToString(listToReturn) << __E__
			   << "Delimiters: " << vectorToString(*listOfDelimiters) << __E__;
		__SS_THROW__;
	}

}  // end getVectorFromString()

//==============================================================================
// static template function (copied from ots::StringMacros)
template<class T>
std::string vectorToString(const std::vector<T>& setToReturn, const std::string& delimeter /*= ", "*/)
{
	std::stringstream ss;
	bool first = true;
	for (auto& setValue : setToReturn)
	{
		if (first)
			first = false;
		else
			ss << delimeter;
		ss << setValue;
	}
	return ss.str();
}  // end vectorToString<T>()
