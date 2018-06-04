//-----------------------------------------------------------------------------
// Copyright (c) 2017-2018 Benjamin Buch
//
// https://github.com/bebuch/disposer-cli
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at https://www.boost.org/LICENSE_1_0.txt)
//-----------------------------------------------------------------------------
#ifndef _disposer_cli__log__hpp_INCLUDED_
#define _disposer_cli__log__hpp_INCLUDED_

#include <logsys/stdlogd.hpp>


namespace disposer_cli{


	struct stdlog: ::logsys::stdlogd{
		static std::weak_ptr< std::ostream > weak_file_ptr;

		void exec()const noexcept override;
	};


}


#endif
