//-----------------------------------------------------------------------------
// Copyright (c) 2015-2018 Benjamin Buch
//
// https://github.com/bebuch/disposer-cli
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at https://www.boost.org/LICENSE_1_0.txt)
//-----------------------------------------------------------------------------
#include "log.hpp"

#include <boost/algorithm/string/replace.hpp>

#include <mutex>


namespace disposer_cli{


	std::weak_ptr< std::ostream > stdlog::weak_file_ptr{};

	void stdlog::exec()const noexcept try{
		using boost::algorithm::replace_all;
		auto line = io_tools::mask_non_print(os_.str()) + "\n";

		if(auto file = weak_file_ptr.lock()){
			static std::recursive_mutex mutex;
			std::lock_guard lock(mutex);
			*file << line;
		}

		replace_all(line, "(WARNING)", "\033[1;33m(WARNING)\033[0m");
		replace_all(line, "(FAILED)", "\033[0;31m(FAILED)\033[0m");
		replace_all(line, "(ERROR)", "\033[1;31m(ERROR)\033[0m");
		replace_all(line, "EXCEPTION CATCHED:",
			"\033[41;1mEXCEPTION CATCHED:\033[0m");
		replace_all(line, "EXCEPTION WHILE LOGGING:",
			"\033[1;31mEXCEPTION WHILE LOGGING:\033[0m");

		static std::recursive_mutex mutex;
		std::lock_guard lock(mutex);
		std::clog << line;
	}catch(std::exception const& e){
		std::cerr << "terminate with exception in stdlog.exec(): "
			<< e.what() << std::endl;
		std::terminate();
	}catch(...){
		std::cerr << "terminate with unknown exception in stdlog.exec()"
			<< std::endl;
		std::terminate();
	}



}


namespace logsys{


	std::function< std::unique_ptr< stdlogb >() > stdlogb::factory_object(
		[](){ return std::make_unique< disposer_cli::stdlog >(); }
	);


}
