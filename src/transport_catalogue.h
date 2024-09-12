#pragma once

#include <deque>
#include <iterator>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "domain.h"
#include "geo.h"

namespace tc {

	struct StopPointersHasher {
		size_t operator()(const std::pair<StopPtr, StopPtr>& stop_pair) const {
			return (size_t)(stop_pair.first) + ((size_t)(stop_pair.second) * 5);
		}
	};

	class TransportCatalogue {
	public:
		void AddStop(const std::string& name, const geo::Coordinates coordinates);
		void AddBus(const std::string& name, const std::vector<StopPtr>& stops, StopPtr end_stop_ptr, bool is_roundtrip);
		void SetDistance(StopPtr stop_from_ptr, const StopPtr stop_to_ptr, int distance);
		int GetDistance(StopPtr stop_from_ptr, StopPtr stop_to_ptr) const;
		const std::deque<Stop>& GetAllStops() const;
		const std::deque<Bus>& GetAllBuses() const;
		StopPtr GetStop(const std::string_view name) const;
		BusPtr GetBus(const std::string_view name) const;
		const std::unordered_set<BusPtr>& GetBusesAtStop(StopPtr stop_ptr) const;
		RouteInfo GetRouteInfo(const BusPtr) const;

	private:
		std::deque<Stop> stops_;
		std::unordered_map<std::string_view, StopPtr> stopname_to_stop_;
		std::deque<Bus> buses_;
		std::unordered_map<std::string_view, BusPtr> busname_to_bus_;
		std::unordered_map<StopPtr, std::unordered_set<BusPtr>> stop_to_buses_;
		std::unordered_map<std::pair<StopPtr, StopPtr>, int, StopPointersHasher> distances_;
	};

} // namespace tc