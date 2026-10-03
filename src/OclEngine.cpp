#include <iostream>
#include <iomanip>
#include <fstream>
#include <stdexcept>
#include <string>
#include <regex>
#include <cmath>
#include <cstdint>
#include <sstream>
#include <vector>
#include "OclEngine.hpp"
#include "Crc32.hpp"

namespace {

// value written by the membench_write kernel: dst[no] = (int4)125
const cl_int FILL_VALUE = 125;

const char* clErrorName(cl_int err) {
	switch (err) {
	case CL_SUCCESS: return "CL_SUCCESS";
	case CL_DEVICE_NOT_FOUND: return "CL_DEVICE_NOT_FOUND";
	case CL_DEVICE_NOT_AVAILABLE: return "CL_DEVICE_NOT_AVAILABLE";
	case CL_COMPILER_NOT_AVAILABLE: return "CL_COMPILER_NOT_AVAILABLE";
	case CL_MEM_OBJECT_ALLOCATION_FAILURE: return "CL_MEM_OBJECT_ALLOCATION_FAILURE";
	case CL_OUT_OF_RESOURCES: return "CL_OUT_OF_RESOURCES";
	case CL_OUT_OF_HOST_MEMORY: return "CL_OUT_OF_HOST_MEMORY";
	case CL_PROFILING_INFO_NOT_AVAILABLE: return "CL_PROFILING_INFO_NOT_AVAILABLE";
	case CL_MEM_COPY_OVERLAP: return "CL_MEM_COPY_OVERLAP";
	case CL_IMAGE_FORMAT_MISMATCH: return "CL_IMAGE_FORMAT_MISMATCH";
	case CL_IMAGE_FORMAT_NOT_SUPPORTED: return "CL_IMAGE_FORMAT_NOT_SUPPORTED";
	case CL_BUILD_PROGRAM_FAILURE: return "CL_BUILD_PROGRAM_FAILURE";
	case CL_MAP_FAILURE: return "CL_MAP_FAILURE";
	case CL_MISALIGNED_SUB_BUFFER_OFFSET: return "CL_MISALIGNED_SUB_BUFFER_OFFSET";
	case CL_EXEC_STATUS_ERROR_FOR_EVENTS_IN_WAIT_LIST: return "CL_EXEC_STATUS_ERROR_FOR_EVENTS_IN_WAIT_LIST";
	case CL_INVALID_VALUE: return "CL_INVALID_VALUE";
	case CL_INVALID_DEVICE_TYPE: return "CL_INVALID_DEVICE_TYPE";
	case CL_INVALID_PLATFORM: return "CL_INVALID_PLATFORM";
	case CL_INVALID_DEVICE: return "CL_INVALID_DEVICE";
	case CL_INVALID_CONTEXT: return "CL_INVALID_CONTEXT";
	case CL_INVALID_QUEUE_PROPERTIES: return "CL_INVALID_QUEUE_PROPERTIES";
	case CL_INVALID_COMMAND_QUEUE: return "CL_INVALID_COMMAND_QUEUE";
	case CL_INVALID_HOST_PTR: return "CL_INVALID_HOST_PTR";
	case CL_INVALID_MEM_OBJECT: return "CL_INVALID_MEM_OBJECT";
	case CL_INVALID_IMAGE_FORMAT_DESCRIPTOR: return "CL_INVALID_IMAGE_FORMAT_DESCRIPTOR";
	case CL_INVALID_IMAGE_SIZE: return "CL_INVALID_IMAGE_SIZE";
	case CL_INVALID_PROGRAM: return "CL_INVALID_PROGRAM";
	case CL_INVALID_PROGRAM_EXECUTABLE: return "CL_INVALID_PROGRAM_EXECUTABLE";
	case CL_INVALID_KERNEL_NAME: return "CL_INVALID_KERNEL_NAME";
	case CL_INVALID_KERNEL_DEFINITION: return "CL_INVALID_KERNEL_DEFINITION";
	case CL_INVALID_KERNEL: return "CL_INVALID_KERNEL";
	case CL_INVALID_ARG_INDEX: return "CL_INVALID_ARG_INDEX";
	case CL_INVALID_ARG_VALUE: return "CL_INVALID_ARG_VALUE";
	case CL_INVALID_ARG_SIZE: return "CL_INVALID_ARG_SIZE";
	case CL_INVALID_KERNEL_ARGS: return "CL_INVALID_KERNEL_ARGS";
	case CL_INVALID_WORK_DIMENSION: return "CL_INVALID_WORK_DIMENSION";
	case CL_INVALID_WORK_GROUP_SIZE: return "CL_INVALID_WORK_GROUP_SIZE";
	case CL_INVALID_WORK_ITEM_SIZE: return "CL_INVALID_WORK_ITEM_SIZE";
	case CL_INVALID_GLOBAL_OFFSET: return "CL_INVALID_GLOBAL_OFFSET";
	case CL_INVALID_EVENT_WAIT_LIST: return "CL_INVALID_EVENT_WAIT_LIST";
	case CL_INVALID_GLOBAL_WORK_SIZE: return "CL_INVALID_GLOBAL_WORK_SIZE";
	case CL_INVALID_BUFFER_SIZE: return "CL_INVALID_BUFFER_SIZE";
	default: return "CL_UNKNOWN_ERROR";
	}
}

std::string toHex(uint32_t value) {
	std::ostringstream os;
	os << "0x" << std::hex << std::setw(8) << std::setfill('0') << value;
	return os.str();
}

/**
 * Result of the crc/error/artifact verification of one chunk.
 */
struct ChunkCheck {
	cl_int err;
	const char* step;
	bool readOk;
	uint32_t crc;
	uint32_t expectedCrc;
	size_t artifacts;
	size_t firstBadWord;

	ChunkCheck() :
			err(CL_SUCCESS), step(""), readOk(false), crc(0), expectedCrc(0),
			artifacts(0), firstBadWord(0) {
	}

	bool ok() const {
		return err == CL_SUCCESS && readOk && artifacts == 0
				&& crc == expectedCrc;
	}
};

}
// -------------------------------------------------------------------------------
OclEngine::OclEngine(int platformNr, int deviceNr) : checksFailed(false), allocErrors(0) {

	std::vector<cl::Platform> platforms;
	std::vector<cl::Device> devices;

	cl::Platform::get(&platforms);
	if (platforms.size() == 0) {
		throw std::runtime_error("No OpenCL Platforms found");
	}

	if (platformNr != -1) { //selected platform
		if ((int) platforms.size() < (platformNr - 1)) {
			throw std::runtime_error("Platform nr:" + std::to_string(platformNr) + " not found");
		} else {
			platform = platforms.at(platformNr);
		}
	} else { //take first
		platform = platforms.front();
	}

	platform.getDevices(CL_DEVICE_TYPE_ALL, &devices);
	if (devices.size() == 0) {
		throw std::runtime_error("No OpenCL Devices found");
	}

	if (deviceNr != -1) { //selected device
		if ((int) devices.size() < deviceNr - 1) {
			throw std::runtime_error("Device nr:" + std::to_string(deviceNr) + " not found");
		} else {
			device = devices.at(deviceNr);
		}
	} else { //take first
		device = devices.front();
	}

	context = cl::Context(device);
}
// -------------------------------------------------------------------------------
void OclEngine::clinfo(std::ostream &info) {
	std::vector<cl::Platform> platforms;
	cl::Platform::get(&platforms);
	int pNum = 0;
	for (auto &p : platforms) {
		std::string platver = p.getInfo<CL_PLATFORM_VERSION>();
		std::string platname = p.getInfo<CL_PLATFORM_NAME>();
		std::string platvend = p.getInfo<CL_PLATFORM_VENDOR>();
		info << "[" << pNum++ << "] Platform name: " << platname << " vendor:" << platvend << " version:" << platver;
		if (p == this->platform)
			info << "\033[1;32m <-- selected \033[0m";
		info << std::endl;
		std::vector<cl::Device> devices;
		p.getDevices(CL_DEVICE_TYPE_ALL, &devices);
		int dNum = 0;
		for (auto &device : devices) {
			info << "\t[" << dNum++ << "] Device Name: " << device.getInfo<CL_DEVICE_NAME>();
			if (device == this->device)
				info << "\033[1;32m <-- selected \033[0m";
			info << std::endl;
			info << "\t       Type: " << device.getInfo<CL_DEVICE_TYPE>();
			info << " (GPU: " << CL_DEVICE_TYPE_GPU << ", CPU: " << CL_DEVICE_TYPE_CPU << ")" << std::endl;
			info << "\t       Vendor: " << device.getInfo<CL_DEVICE_VENDOR>() << std::endl;
			info << "\t       Max Compute Units: " << device.getInfo<CL_DEVICE_MAX_COMPUTE_UNITS>() << std::endl;
			info << "\t       Global Memory: " << device.getInfo<CL_DEVICE_GLOBAL_MEM_SIZE>() << std::endl;
			info << "\t       Max Clock Frequency: " << device.getInfo<CL_DEVICE_MAX_CLOCK_FREQUENCY>() << std::endl;
			info << "\t       Max Alloc. Memory: " << device.getInfo<CL_DEVICE_MAX_MEM_ALLOC_SIZE>() << std::endl;
			info << "\t       Local Memory: " << device.getInfo<CL_DEVICE_LOCAL_MEM_SIZE>() << std::endl;
			info << "\t       Available: " << device.getInfo< CL_DEVICE_AVAILABLE>() << std::endl;
		}
	}

}
// -------------------------------------------------------------------------------
int OclEngine::RunBench1(const std::string& function, const int size, const int repeats) {
	checksFailed = false;
	allocErrors = 0;

	if (size <= 0 || repeats <= 0) {
		std::cout << "FAILED (invalid size or repeats)" << std::endl;
		checksFailed = true;
		return 1;
	}

	cl::Program prg = CreateProgram(); //throws when the kernel source fails to build

	if (AllocBuffers(size) == 0) {
		std::cout << "FAILED (no buffers allocated)" << std::endl;
		checksFailed = true;
		return 1;
	}

	unsigned long mSIZE = size * (unsigned long) pow(2, 20);
	unsigned long mCOUNT = mSIZE / sizeof(cl_int4);
	size_t wordCount = mSIZE / sizeof(cl_int);

	std::cout << "Running Bench test on:" << device.getInfo<CL_DEVICE_NAME>() << std::endl;

	cl_int err = CL_SUCCESS;
	cl::Kernel kernel(prg, function.c_str(), &err);
	if (err != CL_SUCCESS) {
		std::cout << "FAILED (kernel " << function << ": " << clErrorName(err) << ")" << std::endl;
		checksFailed = true;
		return 1;
	}

	cl::CommandQueue queue(context, device, CL_QUEUE_PROFILING_ENABLE, &err);
	if (err != CL_SUCCESS) {
		std::cout << "FAILED (command queue: " << clErrorName(err) << ")" << std::endl;
		checksFailed = true;
		return 1;
	}

	//host side reference content, the expected CRC of every chunk
	std::vector<cl_int> host(wordCount, FILL_VALUE);
	uint32_t expectedCrc = crc32::compute(host.data(), mSIZE);

	int nr = 0;
	int memStart = 0;
	size_t failedChunks = 0;
	for (auto &buffer : buffers) { // buffers loop
		ChunkCheck chk;
		chk.expectedCrc = expectedCrc;

		std::cout << "Chunk: " << std::setw(3) << nr ;
		std::cout << " (" << std::setw(4) << memStart << "-"  << std::setw(4) << (memStart + size) << ")MB";
		memStart += size;

		chk.err = kernel.setArg(0, buffer);
		chk.step = "setArg";

		// warmup
		if (chk.err == CL_SUCCESS) {
			chk.err = queue.enqueueNDRangeKernel(kernel, cl::NDRange(0), cl::NDRange(mCOUNT));
			chk.step = "warmup enqueueNDRangeKernel";
		}

		cl::Event myEvent;
		double tick2 = 0;
		for (int i = 0; i < repeats && chk.err == CL_SUCCESS; i++) { // repeat loop (for each buffer)
			chk.err = queue.enqueueNDRangeKernel(kernel, cl::NDRange(0), cl::NDRange(mCOUNT), cl::NullRange, NULL, &myEvent);
			chk.step = "enqueueNDRangeKernel";
			if (chk.err != CL_SUCCESS)
				break;
			chk.err = myEvent.wait();
			chk.step = "event wait";
			if (chk.err != CL_SUCCESS)
				break;
			cl_int startErr = CL_SUCCESS;
			cl_int endErr = CL_SUCCESS;
			cl_ulong tStart = myEvent.getProfilingInfo<CL_PROFILING_COMMAND_START>(&startErr);
			cl_ulong tEnd = myEvent.getProfilingInfo<CL_PROFILING_COMMAND_END>(&endErr);
			if (startErr != CL_SUCCESS || endErr != CL_SUCCESS) {
				chk.err = (startErr != CL_SUCCESS) ? startErr : endErr;
				chk.step = "profiling info";
				break;
			}
			tick2 += (tEnd - tStart);
		}

		//error check: read the whole chunk back to the host
		if (chk.err == CL_SUCCESS) {
			chk.err = queue.enqueueReadBuffer(buffer, CL_BLOCKING, 0, mSIZE, host.data());
			chk.step = "enqueueReadBuffer";
			chk.readOk = (chk.err == CL_SUCCESS);
		}

		//crc + artifact check of the read back content
		if (chk.readOk) {
			chk.crc = crc32::compute(host.data(), mSIZE);
			const cl_int *words = host.data();
			for (size_t w = 0; w < wordCount; w++) {
				if (words[w] != FILL_VALUE) {
					if (chk.artifacts == 0)
						chk.firstBadWord = w;
					chk.artifacts++;
				}
			}
		}

		double avgMs = tick2 / pow(10, 6) / repeats;
		std::cout << " Speed: ";
		if (chk.err == CL_SUCCESS && avgMs > 0)
			std::cout << std::fixed << std::setprecision(2) << mSIZE / avgMs / pow(2, 20) << " GByte/s ";
		else
			std::cout << "n/a ";
		if (chk.readOk)
			std::cout << "crc:" << toHex(chk.crc) << " ";
		if (chk.err != CL_SUCCESS)
			std::cout << "error:" << clErrorName(chk.err) << "@" << chk.step << " ";
		if (chk.artifacts > 0)
			std::cout << "artifacts:" << chk.artifacts << "(first@" << chk.firstBadWord << ") ";
		if (chk.readOk && chk.crc != chk.expectedCrc)
			std::cout << "crc_mismatch(expected:" << toHex(chk.expectedCrc) << ") ";
		std::cout << (chk.ok() ? "OK" : "FAILED") << std::endl;

		if (!chk.ok()) {
			failedChunks++;
			checksFailed = true;
		}
		nr++;
	}

	err = queue.finish();
	if (err != CL_SUCCESS) {
		std::cout << "FAILED (queue.finish: " << clErrorName(err) << ")" << std::endl;
		checksFailed = true;
	}

	if (!checksFailed) {
		std::cout << "RESULT: OK (" << buffers.size() << " chunks verified, crc:"
				<< toHex(expectedCrc);
		if (allocErrors > 0)
			std::cout << ", " << allocErrors << " chunk(s) skipped (allocation failed)";
		std::cout << ")" << std::endl;
		return 0;
	}
	std::cout << "RESULT: FAILED (";
	if (failedChunks > 0)
		std::cout << failedChunks << "/" << buffers.size() << " chunks with crc/error/artifact problems";
	if (err != CL_SUCCESS)
		std::cout << (failedChunks > 0 ? ", " : "") << "queue.finish:" << clErrorName(err);
	std::cout << ")" << std::endl;
	return 1;
}
// -------------------------------------------------------------------------------
size_t OclEngine::AllocBuffers(int chunkSize) {
	unsigned long mSIZE = chunkSize * (int)pow(2,20);
	cl_ulong mGlobalMemSize = device.getInfo<CL_DEVICE_GLOBAL_MEM_SIZE>();
	for (unsigned int i = 0; i < mGlobalMemSize / (mSIZE - 1); i++) {
//		std::cout << "Allocating chunk: " << std::setw(3) << i;
		cl_int err = CL_SUCCESS;
		cl::Buffer memBuf(context, 0, mSIZE, NULL, &err);
		if (err == CL_SUCCESS && memBuf() != NULL) {
			size_t check = memBuf.getInfo<CL_MEM_SIZE>();
			if (check == mSIZE) {
				buffers.push_back(memBuf);
//				std::cout << " ok: " << check << " bytes" << std::endl;
			} else {
				allocErrors++;
				std::cout << "Problem !!! buffer reports " << check << " bytes" << std::endl;
			}
		} else {
			allocErrors++;
			std::cout << "Problem !!! " << clErrorName(err) << std::endl;
		}
	}
	std::cout << "allocated " << buffers.size() * mSIZE << " bytes " << std::fixed << std::setprecision(2)
			<< (buffers.size() * mSIZE) / pow(2, 30) << " GB" << std::endl;
	if (allocErrors > 0)
		std::cout << "Problem !!! " << allocErrors << " chunk(s) could not be allocated" << std::endl;
	return buffers.size();
}

// -------------------------------------------------------------------------------
OclEngine::~OclEngine() {
}
// -------------------------------------------------------------------------------
cl::Program OclEngine::CreateProgram() {
	std::string src =
			R"(
__kernel void membench_write(__global int4 *dst){
	int no = get_global_id(0);
	dst[no] = (int4)125;
} )";

	cl::Program::Sources sources(1, src);
	cl_int err = CL_SUCCESS;
	cl::Program prg(context, sources, &err);
	if (err != CL_SUCCESS) {
		throw std::runtime_error(std::string("program creation failed: ") + clErrorName(err));
	}

	err = prg.build("-cl-std=CL1.2");
	if (err) {
		std::cout << "error:" << clErrorName(err) << " info:"
				<< prg.getBuildInfo<CL_PROGRAM_BUILD_LOG>(device) << std::endl;
		throw std::runtime_error(std::string("program build failed: ") + clErrorName(err));
	}
	return prg;
}
