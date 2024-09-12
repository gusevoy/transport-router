#pragma once

#include <string>
#include <vector>

#include "geo.h"

/*
 * В этом файле вы можете разместить классы/структуры, которые являются частью предметной области (domain)
 * вашего приложения и не зависят от транспортного справочника. Например Автобусные маршруты и Остановки. 
 *
 * Их можно было бы разместить и в transport_catalogue.h, однако вынесение их в отдельный
 * заголовочный файл может оказаться полезным, когда дело дойдёт до визуализации карты маршрутов:
 * визуализатор карты (map_renderer) можно будет сделать независящим от транспортного справочника.
 *
 * Если структура вашего приложения не позволяет так сделать, просто оставьте этот файл пустым.
 *
 */

namespace tc {

	struct Stop {
		Stop() = delete;
		Stop(std::string name, geo::Coordinates coordinates);
		std::string name;
		geo::Coordinates coordinates;
	};

	using StopPtr = const Stop*;

	struct Bus {
		Bus() = delete;
		Bus(std::string name, std::vector<StopPtr> stops, StopPtr end_stop_ptr, bool is_roundtrip);
		std::string name;
		std::vector<StopPtr> stops;
		StopPtr end_stop_ptr;
		bool is_roundtrip;
	};

	using BusPtr = const Bus*;

	struct RouteInfo {
		size_t stops_count;
		size_t unique_stops_count;
		int distance;
		double curvature;
	};

} // namespace tc