#ifndef CFOINTERFACELIB_MU2ECOMPILER_HH
#define CFOINTERFACELIB_MU2ECOMPILER_HH

#include <algorithm>
#include <cctype>
#include <deque>
#include <fstream>
#include <locale>
#include <map>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

#ifndef __CLING__
#include "tracemf.h"
#endif

/// <summary>
/// The CFO Namespace
/// </summary>
namespace CFOLib
{

/// <summary>
/// The CFO Compiler class
/// </summary>
class CFO_Compiler
{
  public:
	/// <summary>
	/// Instruction names
	/// </summary>
	enum class CFO_INSTR : uint8_t
	{
		NOOP            = 0,  // also used for LABEL
		HEARTBEAT       = 1,
		MARKER          = 10,  // Event Marker 0x1C10-0x1CEF
		DATA_REQUEST    = 2,
		SET_TAG         = 3,
		INC_TAG         = 4,
		WAIT            = 5,  // before a MARKER op, to declare how long previous event window should last (i.e. NEXT RF-0, or a time in clocks)
		LOOP            = 6,
		DO_LOOP         = 7,
		REPEAT          = 8,
		END             = 9,
		GOTO            = 11,
		LABEL           = 12,
		CLEAR_MODE_BITS = 100,  // used by HEARTBEAT w/param 'event_mode = registered'
		SET_MODE_BITS   = 101,  // used by HEARTBEAT w/param 'event_mode = registered'
		AND_MODE_BITS   = 102,  // used by HEARTBEAT w/param 'event_mode = registered'
		OR_MODE_BITS             = 103,  // used by HEARTBEAT w/param 'event_mode = registered'
		OR_SINGLESHOT_MODE_BITS  = 111,  // single-shot OR; identical to OR but fires only once in hardware
		SET_MODE        = 200,  // used by HEARTBEAT w/param 'event_mode = registered'
		INVALID         = 0xFF,
	};

	static const std::string                                            MAIN_GOTO_LABEL;
	static const std::map<uint8_t, std::string> CODE_to_OP_TRANSLATION;
	static const std::map<std::string, CFOLib::CFO_Compiler::CFO_INSTR> OP_to_CODE_TRANSLATION;

  public:
	CFO_Compiler(){};
	~CFO_Compiler(){};

	std::string             processFile(const std::string& sourceCodeFile, const std::string& binaryOutputFile);
	const std::deque<char>& getBinaryOutput() { return output_; }

	static const uint64_t FPGAClock_;  // period of FPGA clock in ns
  private:
	bool isComment(const std::string& line);

	CFO_INSTR          parseInstruction(const std::string& instructionBuffer);
	const std::string& translateOpCode(CFO_INSTR);
	void               processOp(void);
	uint64_t           calculateParameterAndErrorCheck(CFO_INSTR);
	void               outParameter(uint64_t);

	std::vector<uint64_t /* line number */> loopStack_;
	std::map<std::string /* label */,
	         uint64_t /* line number */>
	                         labelMap_;
	uint64_t                 modeClearMask_;
	bool                     hasRequiredPlanEndOp_ = false;  // require at least one END, REPEAT, or GOTO MAIN command to give a handle on switch Run Plan buffers to the CFO firmware
	std::vector<std::string> opArguments_;

	size_t           txtLineNumber_, binLineNumber_;
	std::deque<char> output_;
};

}  // namespace CFOLib

#endif  // CFOINTERFACELIB_MU2ECOMPILER_HH
