#ifndef OCLENGINE_H_
#define OCLENGINE_H_
#include <boost/program_options.hpp>
#define CL_HPP_TARGET_OPENCL_VERSION 120
#define CL_HPP_MINIMUM_OPENCL_VERSION 120
#include <ostream>
#include "cl2.hpp"

class OclEngine {
	cl::Platform platform;
	cl::Device device;
	cl::Context context;
	std::vector<cl::Buffer> buffers;
	bool checksFailed;
	size_t allocErrors;

public:
	/**
	 * Default constructor
	 */
	OclEngine();
	OclEngine(const int platform, const int device);

	/**
	 * Select chosen platform and device
	 * @param platform - this is platform (-1) means get default
	 * @param device - this is device (-1) means get default (GPU)
	 * @return
	 */
	bool select(const int platform, const int device);

	/**
	 * get readable info about detected platforms an devices
	 * @return string containing gathered info
	 */
	void clinfo(std::ostream &os);

	cl::Program CreateProgram();

	/**
	 * Run the memory benchmark. Every chunk is verified afterwards with a
	 * CRC check, an OpenCL error check and an artifact scan of the content.
	 * @param function kernel to run
	 * @param size chunk size in MB
	 * @param repeats number of timed kernel runs per chunk
	 * @return 0 when all chunks passed, 1 when FAILED was reported
	 */
	int RunBench1(const std::string &function, const int size, const int repeats);

	/**
	 * @return true when the last run finished without crc/error/artifact failures
	 */
	bool allChecksPassed() const {
		return !checksFailed;
	}

	size_t AllocBuffers(const int chunkSize);

	/**
	 * Destructor
	 */
	virtual ~OclEngine();
};

#endif /* OCLENGINE_H_ */
