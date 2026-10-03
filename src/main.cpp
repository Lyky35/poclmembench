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

// ------------------------------------------------------------------------
int main(int argc, const char * argv[]) {
	ProgramOptions po(argc, argv);

	try{
		OclEngine oe(po.platform,po.device);
		oe.clinfo(std::cout);
		return oe.RunBench1("membench_write",po.size,po.repeats);
	} catch (const std::invalid_argument &e){
		std::cerr << "Invalid argument:" << e.what() << std::endl;
		std::cout << "FAILED" << std::endl;
		return 1;
	} catch (const std::runtime_error &e){
		std::cerr << "Runtime error:" <<  e.what() << std::endl;
		std::cout << "FAILED" << std::endl;
		return 1;
	} catch (...){
		std::cerr << "Unknown error!"  << std::endl;
		std::cout << "FAILED" << std::endl;
		return 1;
	}
	return 0;
}
