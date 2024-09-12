#pragma once

#include "domain.h"
#include "graph.h"
#include "ranges.h"
#include "router.h"
#include "transport_catalogue.h"

#include <memory>
#include <string_view>
#include <unordered_map>

namespace tc::router {

	using namespace ::std::string_literals;

	struct RoutingSettings {
		int bus_wait_time = 6;
		double bus_velocity = 40.0;
	};

	enum RouteType {
		WAIT,
		BUS
	};

	struct RouteStat {
		double time = 0.0;
		virtual RouteType GetType() const = 0;
	protected:
		virtual ~RouteStat() = default;
		RouteStat(double time) : time(time) {
		}
	};

	struct WaitRouteStat : RouteStat {

		WaitRouteStat(std::string_view stop_name, double time) : RouteStat(time), stop_name(stop_name) {
		}

		std::string_view stop_name;
		RouteType GetType() const override{
			return RouteType::WAIT;
		}
	};

	struct BusRouteStat : RouteStat {

		BusRouteStat(std::string_view bus_name, double time, int span_count) : RouteStat(time), bus_name(bus_name), span_count(span_count) {
		}

		std::string_view bus_name;
		int span_count;
		RouteType GetType() const override {
			return RouteType::BUS;
		}
	};

	struct Route {
		double total_time = 0.0;
		std::vector<std::shared_ptr<RouteStat>> items;
	};


	class TransportRouter {
	public:
		TransportRouter(const TransportCatalogue& transport_catalogue, RoutingSettings settings) :
			transport_catalogue_(transport_catalogue),
			settings_(std::move(settings)),
			graph_(std::move(ConstructGraph())),
			router_(graph_) {
		}

		std::optional<Route> FindRoute(StopPtr stop_from, StopPtr stop_to) const;

	private:

		template <typename ConstIt>
		void SetRouteEdges(graph::DirectedWeightedGraph<double>& graph, const std::string& bus_name, ConstIt stop_ptr_begin, ConstIt stop_ptr_end) {
			const double velocity_coefficient = 60.0 / 1000.0;

			for (ConstIt from_it = stop_ptr_begin; from_it != stop_ptr_end; ++from_it) {
				double time = 0.0;
				int span_count = 0;
				size_t terminal_stop_from_index = stop_vertices_.at(*from_it).second;
				for (ConstIt to_it = from_it + 1; to_it != stop_ptr_end; ++to_it) {
					if (*from_it == *to_it) {
						continue;
					}
					time += transport_catalogue_.GetDistance(*(to_it-1), *to_it) / settings_.bus_velocity * velocity_coefficient;
					size_t hub_stop_to_index = stop_vertices_.at(*to_it).first;
					size_t edge_id = graph.AddEdge({ terminal_stop_from_index, hub_stop_to_index, time });
					edge_stats_[edge_id] = std::make_shared<BusRouteStat>(bus_name, time, ++span_count);
				}
			}
		}

		void AddStopsToGraph(graph::DirectedWeightedGraph<double>& graph);
		void AddBussesToGraph(graph::DirectedWeightedGraph<double>& graph);
		graph::DirectedWeightedGraph<double> ConstructGraph();

		const TransportCatalogue& transport_catalogue_;
		RoutingSettings settings_;
		std::unordered_map<StopPtr, std::pair<size_t, size_t>> stop_vertices_; // в паре первое - hub, куда приезжают; второе - terminal, откуда уезжают через bus_wait_time
		std::unordered_map<size_t, std::shared_ptr<RouteStat>> edge_stats_;
		graph::DirectedWeightedGraph<double> graph_;
		graph::Router<double> router_;
	};

} // namespace tc::router