#include "transport_catalogue.h"

namespace tc {

    Stop::Stop(std::string name, geo::Coordinates coordinates) : name(std::move(name)), coordinates(std::move(coordinates)) {
    }

    Bus::Bus(std::string name, std::vector<StopPtr> stops, StopPtr end_stop_ptr, bool is_roundtrip) :
        name(std::move(name)),
        stops(std::move(stops)),
        end_stop_ptr(end_stop_ptr),
        is_roundtrip(is_roundtrip) {
    }

    double GetStrightRouteLength(BusPtr bus_ptr) {
        double distance = 0.0;
        auto& stops = bus_ptr->stops;
        if (stops.size() < 2) {
            return 0.0;
        }
        auto pos = stops.begin();
        auto prev_pos = stops.begin();
        while (++pos != stops.end()) {
            distance += ComputeDistance((*(pos - 1))->coordinates, (*pos)->coordinates);
            prev_pos = pos;
        }
        return distance;
    }

    void TransportCatalogue::AddStop(const std::string& name, const geo::Coordinates coordinates) {
        Stop& stop = stops_.emplace_back(name, coordinates);
        stopname_to_stop_[stop.name] = &stop;
        stop_to_buses_[&stop] = {};
    }

    void TransportCatalogue::AddBus(const std::string& name, const std::vector<StopPtr>& stops, StopPtr end_stop_ptr, bool is_roundtrip) {
        Bus& bus = buses_.emplace_back(name, stops, end_stop_ptr, is_roundtrip);
        busname_to_bus_[bus.name] = &bus;
        for (StopPtr stop : bus.stops) {
            stop_to_buses_.at(stop).insert(&bus);
        }
    }

    void TransportCatalogue::SetDistance(StopPtr stop_from_ptr, StopPtr stop_to_ptr, int distance) {
        distances_[{stop_from_ptr, stop_to_ptr}] = distance;
    }

    int TransportCatalogue::GetDistance(StopPtr stop_from_ptr, StopPtr stop_to_ptr) const {
        auto it = distances_.find(std::make_pair(stop_from_ptr, stop_to_ptr));
        if (it == distances_.end()) {
            it = distances_.find(std::make_pair(stop_to_ptr, stop_from_ptr));
        }
        return it->second;
    }

    const std::deque<Stop>& TransportCatalogue::GetAllStops() const {
        return stops_;
    }

    const std::deque<Bus>& TransportCatalogue::GetAllBuses() const {
        return buses_;
    }

    StopPtr TransportCatalogue::GetStop(const std::string_view name) const {
        if (!stopname_to_stop_.count(name)) {
            return nullptr;
        }
        return stopname_to_stop_.at(name);
    }

    BusPtr TransportCatalogue::GetBus(const std::string_view name) const {
        if (!busname_to_bus_.count(name)) {
            return nullptr;
        }
        return busname_to_bus_.at(name);
    }
    const std::unordered_set<BusPtr>& TransportCatalogue::GetBusesAtStop(StopPtr stop_ptr) const {
        return stop_to_buses_.at(stop_ptr);
    }

    RouteInfo TransportCatalogue::GetRouteInfo(BusPtr bus_ptr) const {
        RouteInfo info{};
        double stright_distance = 0.0;

        auto& stops = bus_ptr->stops;
        info.stops_count = stops.size();
        info.unique_stops_count = info.stops_count;
        if (stops.size() < 2) {
            return info;
        }

        auto pos = stops.begin();
        auto prev_pos = stops.begin();
        std::unordered_set<StopPtr> unique_stops;
        unique_stops.insert(*pos);
        while (++pos != stops.end()) {
            unique_stops.insert(*pos);
            info.distance += GetDistance(*prev_pos, *pos);
            stright_distance += geo::ComputeDistance((*prev_pos)->coordinates, (*pos)->coordinates);
            prev_pos = pos;
        }

        info.unique_stops_count = unique_stops.size();
        info.curvature = info.distance / stright_distance;

        return info;
    }
}
