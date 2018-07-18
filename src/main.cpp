//-----------------------------------------------------------------------------
// Copyright (c) 2015-2018 Benjamin Buch
//
// https://github.com/bebuch/disposer-cli
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at https://www.boost.org/LICENSE_1_0.txt)
//-----------------------------------------------------------------------------
#include "log.hpp"

#include <disposer/disposer.hpp>

#include <logsys/log.hpp>
#include <logsys/stdlogb_factory_object.hpp>

#include <io_tools/name_generator.hpp>
#include <io_tools/time_to_dir_string.hpp>

#include <cxxopts.hpp>

#include <boost/filesystem.hpp>
#include <boost/stacktrace.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/dll.hpp>

#include <regex>
#include <iostream>
#include <future>
#include <csignal>


namespace disposer_cli{


	std::string const program_start_time = io_tools::time_to_dir_string();

	void signal_handler(int signum){
		std::signal(signum, SIG_DFL);
		{
			std::ofstream os(program_start_time + "_stacktrace.dump");
			if(os) os << boost::stacktrace::stacktrace();
		}
		std::cerr << boost::stacktrace::stacktrace();
		std::raise(SIGABRT);
	}


	std::unique_ptr< logsys::stdlogb > log_factory()noexcept try{
		return std::make_unique< disposer_cli::stdlog >();
	}catch(std::exception const& e){
		std::cerr << "terminate with exception in stdlogb factory: "
			<< e.what() << '\n';
		std::terminate();
	}catch(...){
		std::cerr << "terminate with unknown exception in stdlogb factory\n";
		std::terminate();
	}


}


int main(int argc, char** argv){
	using namespace std::literals::string_literals;
	using namespace std::literals::string_view_literals;
	namespace fs = boost::filesystem;

	// Set signal handler
	std::signal(SIGSEGV, &disposer_cli::signal_handler);
	std::signal(SIGABRT, &disposer_cli::signal_handler);

	logsys::stdlogb_factory_object = &disposer_cli::log_factory;

	cxxopts::Options option_config(argv[0], "disposer module system");

	option_config.add_options()
		("components-and-modules-dirs", "directories that containe "
			"components and modules to be loaded by the disposer",
			cxxopts::value< std::vector< std::string > >()
			->default_value({"components-and-modules"}), "Directory")
		("c,config", "Configuration file", cxxopts::value< std::string >(),
			"config.ini")
		("l,log", "Filename of the logfile; use ${date_time} as placeholder, "
			"depending on your operating system you might have to mask "
			"$ as \\$",
			cxxopts::value< std::string >()
				->default_value("${date_time}_disposer.log"),
			"disposer.log")
		("no-log", "Don't create a log file")
		("s,server", "Run until the enter key is pressed")
		("m,multithreading",
			"All N executions of a chain are stated instantly")
		("chain", "Execute a chain",
			cxxopts::value< std::vector< std::string > >(), "Name")
		("n,count", "Count of chain executions",
			cxxopts::value< std::vector< std::size_t > >()
			->default_value({"1"}), "Count")
		("list-components", "Print all component names")
		("list-modules", "Print all module names")
		("component-help", "Print the help text of the given component",
			cxxopts::value< std::vector< std::string > >(), "Component Name")
		("module-help", "Print the help text of the given module",
			cxxopts::value< std::vector< std::string > >(), "Module Name")
		("components-and-modules-help",
			"Print the help text of all modules and components");

	auto options = [&]{
			try{
				auto options = option_config.parse(argc, argv);
				if(
					options["list-components"].count() == 0 &&
					options["list-modules"].count() == 0 &&
					options["component-help"].count() == 0 &&
					options["module-help"].count() == 0 &&
					options["components-and-modules-help"].count() == 0
				){
					if(options["config"].count() == 0){
						throw std::logic_error("Need option ‘config‘");
					}
					bool const server = options["server"].count() > 0;
					bool const chain = options["chain"].count() > 0;
					if(!server && !chain){
						throw std::logic_error(
							"Need at least option ‘server‘ or option ‘chain‘");
					}
				}
				return options;
			}catch(std::exception const& e){
				std::cerr << e.what() << "\n\n";
				std::cout << option_config.help();
				std::exit(-1);
			}
		}();


	// defines the livetime of the logfile
	std::shared_ptr< std::ostream > logfile;
	if(options["no-log"].count() == 0){
		auto const filename_pattern = options["log"].as< std::string >();
		auto generator = io_tools::make_name_generator(
			filename_pattern, {false},
			std::make_pair("date_time"s,
				[](std::string const& str){ return str; }));

		auto const filename = generator(disposer_cli::program_start_time);
		logfile = std::make_shared< std::ofstream >(filename);
		if(!*logfile){
			throw std::runtime_error("Can not open log-file: " + filename);
		}

		// add the log file to the log system
		disposer_cli::stdlog::weak_file_ptr = logfile;
	}

	// modules must be deleted last, to access the destructors in shared libs
	std::list< boost::dll::shared_library > libraries;
	disposer::system system;

	if(!logsys::exception_catching_log([](
		logsys::stdlogb& os){ os << "loading modules"; },
	[&system, &libraries, mc_dirs =
		options["components-and-modules-dirs"]
			.as< std::vector< std::string > >()
	]{
		for(auto& mc_dir: mc_dirs){
			std::cout << "Search for DLLs in '" << mc_dir << "'" << std::endl;

			std::regex regex("lib.*\\.so");
			for(auto const& file: fs::directory_iterator(mc_dir)){
				if(
					!is_regular_file(file) ||
					!std::regex_match(file.path().filename().string(), regex)
				) continue;

				auto const lib_name = file.path().stem().string().substr(3);

				logsys::log([&lib_name](logsys::stdlogb& os){
					os << "load shared library '" << lib_name << "'";
				}, [&]{
					auto& library = libraries.emplace_back(file.path().string(),
						boost::dll::load_mode::rtld_deepbind);

					if(library.has("init")){
						library.get_alias<
								void(
									std::string const&,
									disposer::declarant&
								)
							>("init")(lib_name, system.directory().declarant());
					}else{
						logsys::log([&lib_name](logsys::stdlogb& os){
							os << "shared library '" << lib_name
								<< "' is nighter a component nor a module";
						});
					}
				});
			}
		}
	})) return 1;

	if(options["components-and-modules-help"].count() > 0){
		logsys::exception_catching_log(
			[](logsys::stdlogb& os){ os << "print help"; },
			[&system]{
				std::cout << system.directory().help();
			});

		return 0;
	}else if(
		options["list-components"].count() > 0 ||
		options["list-modules"].count() > 0
	){
		if(options["list-components"].count() > 0){
			auto components = system.directory().component_names();
			std::cout << "  * Components:\n";
			for(auto const& component: components){
				std::cout << "    * " << component << '\n';
			}
		}

		if(options["list-modules"].count() > 0){
			auto modules = system.directory().module_names();
			std::cout << "  * Modules:\n";
			for(auto const& module: modules){
				std::cout << "    * " << module << '\n';
			}
		}

		return 0;
	}else if(
		options["component-help"].count() > 0 ||
		options["module-help"].count() > 0
	){
		logsys::exception_catching_log(
			[](logsys::stdlogb& os){ os << "print help"; },
			[&system, &options]{
				if(options["component-help"].count() > 0){
					for(auto component: options["component-help"]
						.as< std::vector< std::string > >()
					){
						std::cout <<
							system.directory().component_help(component);
					}
				}

				if(options["module-help"].count() > 0){
					for(auto module: options["module-help"]
						.as< std::vector< std::string > >()
					){
						std::cout << system.directory().module_help(module);
					}
				}
			});

		return 0;
	}

	std::string const config = options["config"].as< std::string >();

	if(!logsys::exception_catching_log(
		[](logsys::stdlogb& os){ os << "loading config"; },
		[&system, &config]{
			system.load_config_file(config);
		})) return -1;

	if(options["chain"].count() > 0){
		logsys::exception_catching_log(
			[](logsys::stdlogb& os){ os << "exec chains"; },
			[&system, &options]{
				auto const multithreading =
					options["multithreading"].count() > 0;
				auto const exec_chains =
					options["chain"].as< std::vector< std::string > >();
				auto const exec_counts =
					options["count"].as< std::vector< std::size_t > >();
				for(std::size_t i = 0; i < exec_chains.size(); ++i){
					auto const exec_count =
						exec_counts.size() > i ? exec_counts[i] : 1;

					disposer::enabled_chain chain(system, exec_chains[i]);

					if(!multithreading){
						// single thread version
						for(std::size_t j = 0; j < exec_count; ++j){
							chain.exec();
						}
					}else{
						// multi threaded version
						std::vector< std::future< void > > tasks;
						tasks.reserve(exec_count);

						for(std::size_t j = 0; j < exec_count; ++j){
							tasks.push_back(std::async([&chain]{
									chain.exec();
								}));
						}

						for(auto& task: tasks){
							task.get();
						}
					}
				}
			});
	}

	if(options["server"].count() > 0){
		std::cout << "Hit Enter to exit!" << std::endl;
		std::cin.get();
	}
}
