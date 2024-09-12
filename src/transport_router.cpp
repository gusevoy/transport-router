#include "graph.h"
#include "router.h"
#include "transport_router.h"

#include <cassert>

namespace tc::router {

	void TransportRouter::AddStopsToGraph(graph::DirectedWeightedGraph<double>& graph) {
		size_t vertex_counter = 0;
		for (const Stop& stop : transport_catalogue_.GetAllStops()) {
			StopPtr stop_ptr = &stop;
			size_t hub_vertex_id = vertex_counter++;
			size_t terminal_vertex_id = vertex_counter++;
			stop_vertices_[stop_ptr] = std::make_pair(hub_vertex_id, terminal_vertex_id);
			size_t edge_id = graph.AddEdge({ hub_vertex_id, terminal_vertex_id, static_cast<double>(settings_.bus_wait_time) });
			edge_stats_[edge_id] = std::make_shared<WaitRouteStat>(stop_ptr->name, static_cast<double>(settings_.bus_wait_time));
		}
	}

	void TransportRouter::AddBussesToGraph(graph::DirectedWeightedGraph<double>& graph) {
		for (const Bus& bus : transport_catalogue_.GetAllBuses()) {
			if (bus.is_roundtrip) {
				SetRouteEdges(graph, bus.name, bus.stops.cbegin(), bus.stops.cend());
			}
			else {
				size_t end_stop_index = bus.stops.size() / 2;
				auto end_stop_ptr_it = bus.stops.cbegin() + end_stop_index;
				SetRouteEdges(graph, bus.name, bus.stops.cbegin(), end_stop_ptr_it + 1);
				SetRouteEdges(graph, bus.name, end_stop_ptr_it, bus.stops.cend());
			}
		}
	}

	graph::DirectedWeightedGraph<double> TransportRouter::ConstructGraph() {
		graph::DirectedWeightedGraph<double> graph{ transport_catalogue_.GetAllStops().size() * 2 };

		// добавляем в граф остановки - на каждую два узла и грань ожидания
		AddStopsToGraph(graph);

		// добавляем маршруты, каждая остановка - грани ко всем следующим остановкам до конечной (и обратно, для не кольцевых маршрутов)
		AddBussesToGraph(graph);

		return graph;
	}

	std::optional<Route> TransportRouter::FindRoute(StopPtr stop_from, StopPtr stop_to) const {
		size_t hub_stop_from_index = stop_vertices_.at(stop_from).first;
		size_t hub_stop_to_index = stop_vertices_.at(stop_to).first;
		std::optional<graph::Router<double>::RouteInfo> route_info = router_.BuildRoute(hub_stop_from_index, hub_stop_to_index);
		
		if (!route_info.has_value()) {
			return std::nullopt;
		}

		Route route;
		route.total_time = (*route_info).weight;
		for (size_t i : (*route_info).edges) {
			route.items.emplace_back(edge_stats_.at(i));
		}

		return route;
	}

} // namespace tc::router