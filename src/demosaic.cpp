//-----------------------------------------------------------------------------
// Copyright (c) 2017 Benjamin Buch
//
// https://github.com/bebuch/disposer_module
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at https://www.boost.org/LICENSE_1_0.txt)
//-----------------------------------------------------------------------------
#include "bitmap_vector.hpp"

#include <bitmap/pixel.hpp>

#include <disposer/module.hpp>

#include <boost/hana.hpp>
#include <boost/dll.hpp>

#include <cstdint>
#include <iostream>

#include "thread_pool.hpp"


namespace disposer_module{ namespace demosaic{


	namespace hana = boost::hana;
	namespace pixel = ::bitmap::pixel;


	using disposer::type_position_v;

	using type_list = disposer::type_list<
		std::int8_t,
		std::uint8_t,
		std::int16_t,
		std::uint16_t,
		std::int32_t,
		std::uint32_t,
		std::int64_t,
		std::uint64_t,
		float,
		double,
		long double,
		pixel::ga8,
		pixel::ga16,
		pixel::ga32,
		pixel::ga64,
		pixel::ga8u,
		pixel::ga16u,
		pixel::ga32u,
		pixel::ga64u,
		pixel::ga32f,
		pixel::ga64f,
		pixel::rgb8,
		pixel::rgb16,
		pixel::rgb32,
		pixel::rgb64,
		pixel::rgb8u,
		pixel::rgb16u,
		pixel::rgb32u,
		pixel::rgb64u,
		pixel::rgb32f,
		pixel::rgb64f,
		pixel::rgba8,
		pixel::rgba16,
		pixel::rgba32,
		pixel::rgba64,
		pixel::rgba8u,
		pixel::rgba16u,
		pixel::rgba32u,
		pixel::rgba64u,
		pixel::rgba32f,
		pixel::rgba64f
	>;


	struct parameter{
		std::size_t x_count;
		std::size_t y_count;
	};


	struct module: disposer::module_base{
		module(disposer::make_data const& data, parameter&& param):
			disposer::module_base(
				data,
				{slots.image},
				{signals.image_vector}
			),
			param(std::move(param))
			{}


		template < typename T >
		bitmap_vector< T > demosaic(bitmap< T > const& image)const;


		struct{
			disposer::container_input< bitmap, type_list >
				image{"image"};
		} slots;

		struct{
			disposer::container_output< bitmap_vector, type_list >
				image_vector{"image_vector"};
		} signals;


		void exec()override;
		void input_ready()override;


		parameter const param;
	};


	disposer::module_ptr make_module(disposer::make_data& data){
		if(data.is_first()) throw disposer::module_not_as_start(data);

		parameter param;

		data.params.set(param.x_count, "x_count");
		if(param.x_count == 0){
			throw std::logic_error("parameter x_count == 0");
		}

		data.params.set(param.y_count, "y_count");
		if(param.x_count == 0){
			throw std::logic_error("parameter x_count == 0");
		}

		return std::make_unique< module >(data, std::move(param));
	}


	template < typename T >
	bitmap_vector< T > module::demosaic(bitmap< T > const& image)const{
		auto const xc = param.x_count;
		auto const yc = param.y_count;

		if(image.width() % xc){
			throw std::logic_error(
				"image width is not divisible by parameter x_count");
		}

		if(image.height() % yc){
			throw std::logic_error(
				"image height is not divisible by parameter y_count");
		}

		std::size_t width = image.width() / xc;
		std::size_t height = image.height() / yc;
		std::size_t image_count = xc * yc;

		bitmap_vector< T > result;
		result.reserve(image_count);

		thread_pool pool;

		std::mutex mutex;
		pool(0, image_count,
			[&result, &mutex, width, height](std::size_t){
				auto image = bitmap< T >(width, height);

				// emplace_back is not thread safe
				std::lock_guard< std::mutex > lock(mutex);
				result.emplace_back(std::move(image));
			});

		pool(0, height,
			[&result, &image, xc, yc, width](std::size_t y){
				for(std::size_t iy = 0; iy < yc; ++iy){
					for(std::size_t x = 0; x < width; ++x){
						for(std::size_t ix = 0; ix < xc; ++ix){
							result[iy * xc + ix](x, y) =
								image(x * xc + ix, y * yc + iy);
						}
					}
				}
			});

		return result;
	}


	void module::exec(){
		for(auto const& [id, img]: slots.image.get()){
			(void)id;
			std::visit([this](auto const& vector){
				using value_type =
					typename std::decay_t< decltype(vector.data()) >
					::value_type;

				signals.image_vector.put< value_type >(demosaic(vector.data()));
			}, img);
		}
	}

	void module::input_ready(){
		signals.image_vector.enable_types(
			slots.image.enabled_types_transformed(
				[](auto type){
					return hana::type_c<
						std::vector< typename decltype(type)::type > >;
				}
			)
		);
	}


	void init(disposer::module_declarant& add){
		add("demosaic", &make_module);
	}

	BOOST_DLL_AUTO_ALIAS(init)


} }
