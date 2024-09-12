#pragma once

#include "domain.h"
#include "svg.h"

#include <algorithm>
#include <string>
#include <vector>

/*
 * В этом файле вы можете разместить код, отвечающий за визуализацию карты маршрутов в формате SVG.
 * Визуализация маршртутов вам понадобится во второй части итогового проекта.
 * Пока можете оставить файл пустым.
 */

namespace renderer {

	using namespace ::std::string_literals;

	struct RenderSettings {

		double width = 1200.0;
		double height = 1200.0;

		double padding = 50.0;

		double line_width = 14.0;
		double stop_radius = 5.0;

		int bus_label_font_size = 20;
		svg::Point bus_label_offset = { 7.0, 15.0 };

		int stop_label_font_size = 20;
		svg::Point stop_label_offset = { 7.0, 15.0 };

		svg::Color underlayer_color = svg::Rgba{ 255, 255, 255, 0.85 };
		double underlayer_width = 3.0;

		std::vector<svg::Color> color_pallete = { "green"s, svg::Rgb{ 255, 160, 0 }, "red"s };
	};

	class MapRenderer {
	public:
		MapRenderer() = default;
		MapRenderer(RenderSettings settings);

		// first и last задают множество const tc::Bus*
		template <typename ConstBusPtrIt>
		svg::Document RenderBuses(ConstBusPtrIt first, ConstBusPtrIt last) const {
			std::vector<const tc::Bus*> bus_ptrs;
			bus_ptrs.reserve(std::distance(first, last));
			for (ConstBusPtrIt it = first; it != last; ++it) {
				bus_ptrs.push_back(&(*it));
			}
			std::sort(bus_ptrs.begin(), bus_ptrs.end(), [](const tc::Bus* lhs, const tc::Bus* rhs) { return lhs->name < rhs->name; });
			svg::Document svg_doc{};
			RenderVectorOfBuses(svg_doc, bus_ptrs);
			return svg_doc;
		}
		
	private:
		void RenderVectorOfBuses(svg::Document& svg_doc, const std::vector<const tc::Bus*>& bus_ptrs) const;
		RenderSettings settings_;
	};

} // namespace renderer