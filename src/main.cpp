#include <iostream>
#include <iomanip>
#include <cmath>
#include <cassert>
#include <fstream>
#include <vector>
#include <stdexcept>
#include <exception>

#include "ProgramOptions.hpp"
#include "OclEngine.hpp"
#include "OclErrorHelper.hpp"

// ------------------------------------------------------------------------
int main(int argc, const char * argv[]) {
	ProgramOptions po(argc, argv);

	try{
		OclEngine oe(po.platform,po.device);
		oe.clinfo(std::cout);
		oe.RunBench1("membench_write",po.size,po.repeats);
	} catch (const cl::Error &e){
		std::cerr << "\n========================================" << std::endl;
		std::cerr << "OpenCL Error Detected!" << std::endl;
		std::cerr << "========================================" << std::endl;
		std::cerr << "Error Code: " << e.err() << std::endl;
		std::cerr << "Error Name: " << getOpenCLErrorString(e.err()) << std::endl;
		std::cerr << "Description: " << e.what() << std::endl;

		std::string help = getOpenCLErrorHelp(e.err());
		if (!help.empty()) {
			std::cerr << "\nTroubleshooting:\n" << help << std::endl;
		}
		std::cerr << "========================================" << std::endl;
		return 1;
	} catch (const std::invalid_argument &e){
		std::cerr << "Invalid argument: " << e.what() << std::endl;
		return 1;
	} catch (const std::runtime_error &e){
		std::cerr << "Runtime error: " <<  e.what() << std::endl;
		return 1;
	} catch (...){
		std::cerr << "Unknown error!"  << std::endl;
		return 1;
	}
	return 0;
}
