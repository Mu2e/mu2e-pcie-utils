#pragma once
#include <vector>

#define BLOCK_COUNT_MAX 10000

/// <summary>
/// A small pseudo-Fragment used for testing
/// </summary>
class fragmentTester
{
  public:
	/// <summary>
	/// Construct a fragmentTester object with the given initial size
	/// </summary>
	/// <param name="bytes">Initial size of the fragmentTester object</param>
	explicit fragmentTester(size_t bytes)
	    : vals_(bytes)
	    , block_count(0) {}

	~fragmentTester() = default;

	/// <summary>
	/// Get the size of the data (excluding header information), in bytes
	/// </summary>
	/// <returns>Size of the data, in bytes</returns>
	size_t dataSize()
	{
		if(block_count == 0)
		{
			return 0;
		}
		return blocks[block_count - 1];
	}

	/// <summary>
	/// Get the size of the pseudo-Fragment, in bytes
	/// </summary>
	/// <returns>Size of the Fragment data, in bytes</returns>
	size_t fragSize() const { return vals_.size(); }

	/// <summary>
	/// Adds space in the data vector
	/// </summary>
	/// <param name="bytes">Amount of space to add</param>
	void addSpace(size_t bytes) { vals_.resize(vals_.size() + bytes); }

	/// <summary>
	/// Get a pointer to the start of the data
	/// </summary>
	/// <returns>Pointer to the start of the data</returns>
	uint8_t* dataBegin() { return reinterpret_cast<uint8_t*>(&vals_[0]); }

	/// <summary>
	/// Flag event completion, updates block size array and block counter
	/// </summary>
	/// <param name="bytes">Bytes in this block</param>
	void endSubEvt(size_t bytes)
	{
		if(block_count > 0)
		{
			blocks[block_count] = blocks[block_count - 1] + bytes;
		}
		else
		{
			blocks[0] = bytes;
		}
		block_count++;
	}

	/// <summary>
	/// Get the block count of the pseudo-Fragment
	/// </summary>
	/// <returns>The block count of the Fragment</returns>
	size_t hdr_block_count() const { return block_count; }

  private:
	std::vector<uint8_t> vals_;
	size_t               blocks[BLOCK_COUNT_MAX];
	size_t               block_count;
};
