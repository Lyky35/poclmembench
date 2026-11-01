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

//wait for one keypress before the program exits
//so the console keeps the benchmark output visible
static void pauseConsole() {
	std::cout << "Press any key to exit..." << std::flush;
	char key;
	std::cin.get(key);
}

// ------------------------------------------------------------------------
int main(int argc, const char * argv[]) {
	ProgramOptions po(argc, argv);

	try{
		OclEngine oe(po.platform,po.device);
		oe.clinfo(std::cout);
		int result = oe.RunBench1("membench_write",po.size,po.repeats);
		pauseConsole();
		return result;
	} catch (const std::invalid_argument &e){
		std::cerr << "Invalid argument:" << e.what() << std::endl;
		std::cout << "FAILED" << std::endl;
		pauseConsole();
		return 1;
	} catch (const std::runtime_error &e){
		std::cerr << "Runtime error:" <<  e.what() << std::endl;
		std::cout << "FAILED" << std::endl;
		pauseConsole();
		return 1;
	} catch (...){
		std::cerr << "Unknown error!"  << std::endl;
		std::cout << "FAILED" << std::endl;
		pauseConsole();
		return 1;
	}
	return 0;
}
