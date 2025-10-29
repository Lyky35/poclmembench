#include <iostream>
#include <iomanip>
#include <fstream>
#include <stdexcept>
#include <string>
#include <regex>
#include "OclEngine.hpp"
#include "OclErrorHelper.hpp"

// -------------------------------------------------------------------------------
OclEngine::OclEngine(int platformNr, int deviceNr) {

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
	try {
		auto prg = CreateProgram();

		AllocBuffers(size);

		unsigned long mSIZE = size * (int) pow(2, 20);
		unsigned long mCOUNT = mSIZE / sizeof(cl_int4);

		std::cout << "Running Bench test on:" << device.getInfo<CL_DEVICE_NAME>() << std::endl;

		cl::Kernel kernel(prg, function.c_str());
		cl::CommandQueue queue(context, device, CL_QUEUE_PROFILING_ENABLE);

		int nr = 0;
		int memStart=0;
		for (auto &buffer : buffers) { // buffers loop
			try {
				kernel.setArg(0, buffer);
				cl_int liczba;
				// warmup
				std::cout << "Chunk: " << std::setw(3) << nr ;
				std::cout << " (" << std::setw(4) << memStart << "-"  << std::setw(4) << (memStart + size) << ")MB";
				memStart += size;
				cl::Event myEvent;
				queue.enqueueNDRangeKernel(kernel, cl::NDRange(0), cl::NDRange(mCOUNT));

				double tick2 = 0;
				for (int i = 0; i < repeats; i++) { // repeat loop (for each buffer)
					queue.enqueueNDRangeKernel(kernel, cl::NDRange(0), cl::NDRange(mCOUNT), cl::NullRange, NULL, &myEvent);
					myEvent.wait();
					tick2 += (myEvent.getProfilingInfo<CL_PROFILING_COMMAND_END>()
							- myEvent.getProfilingInfo<CL_PROFILING_COMMAND_START>());
				}
				queue.enqueueReadBuffer(buffer, CL_BLOCKING, 0, sizeof(liczba), &liczba);
				tick2 = tick2 / pow(10, 6) / repeats;
				std::cout << " Speed: " << std::fixed << std::setprecision(2) << mSIZE / tick2 / pow(2, 20) << " GByte/s ";
				std::cout << (liczba == 125 ? "OK" : "FALSE") << std::endl;
				nr++;
			} catch (const cl::Error &e) {
				std::cerr << "\n\nError occurred while processing chunk " << nr << ":" << std::endl;
				std::cerr << "  Error Code: " << e.err() << std::endl;
				std::cerr << "  Error Name: " << getOpenCLErrorString(e.err()) << std::endl;
				std::cerr << "  Description: " << e.what() << std::endl;

				std::string help = getOpenCLErrorHelp(e.err());
				if (!help.empty()) {
					std::cerr << "\n  " << help << std::endl;
				}

				// Re-throw to be caught by main
				throw;
			}
		}
		queue.finish();
		return 0;
	} catch (const cl::Error &e) {
		// Add context about where the error occurred
		std::cerr << "\nError context: Failed during benchmark execution" << std::endl;
		throw; // Re-throw to be handled by main
	}
}
// -------------------------------------------------------------------------------
size_t OclEngine::AllocBuffers(int chunkSize) {
	unsigned long mSIZE = chunkSize * (int)pow(2,20);
	cl_ulong mGlobalMemSize = device.getInfo<CL_DEVICE_GLOBAL_MEM_SIZE>();

	std::cout << "Allocating memory buffers (chunk size: " << chunkSize << " MB)..." << std::endl;

	for (unsigned int i = 0; i < mGlobalMemSize / (mSIZE - 1); i++) {
		try {
			cl::Buffer memBuf(context, 0, mSIZE);
			size_t check = memBuf.getInfo<CL_MEM_SIZE>();
			if (check != 0) {
				buffers.push_back(memBuf);
			} else {
				std::cerr << "Warning: Failed to allocate chunk " << i << " (got 0 size)" << std::endl;
				break; // Stop trying to allocate more
			}
		} catch (const cl::Error &e) {
			// This is expected when we run out of memory, so don't print scary error
			if (e.err() == -4 || e.err() == -61) { // CL_MEM_OBJECT_ALLOCATION_FAILURE or CL_INVALID_BUFFER_SIZE
				// Stop allocating silently
				break;
			} else {
				// Unexpected error, report it
				std::cerr << "\nUnexpected error during buffer allocation:" << std::endl;
				std::cerr << "  Error Code: " << e.err() << std::endl;
				std::cerr << "  Error Name: " << getOpenCLErrorString(e.err()) << std::endl;
				throw; // Re-throw unexpected errors
			}
		}
	}

	if (buffers.size() == 0) {
		throw std::runtime_error("Failed to allocate any memory buffers. Try reducing chunk size with -s parameter.");
	}

	std::cout << "allocated " << buffers.size() * mSIZE << " bytes " << std::fixed << std::setprecision(2)
			<< (buffers.size() * mSIZE) / pow(2, 30) << " GB" << std::endl;
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
	cl::Program prg(context, sources);

	auto err = prg.build("-cl-std=CL1.2");
	if (err) {
		std::cerr << "\n========================================" << std::endl;
		std::cerr << "OpenCL Program Build Failed!" << std::endl;
		std::cerr << "========================================" << std::endl;
		std::cerr << "Error Code: " << err << std::endl;
		std::cerr << "Error Name: " << getOpenCLErrorString(err) << std::endl;
		std::cerr << "\nBuild Log:\n" << prg.getBuildInfo<CL_PROGRAM_BUILD_LOG>(device) << std::endl;
		std::cerr << "========================================" << std::endl;

		// Throw an error to be caught by main
		throw cl::Error(err, "Failed to build OpenCL program");
	}
	return prg;
}
