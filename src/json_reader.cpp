#include "json_builder.h"
#include "json_reader.h"
#include "svg.h"

#include <algorithm>
#include <cassert>
#include <sstream>

/*
 * Здесь можно разместить код наполнения транспортного справочника данными из JSON,
 * а также код обработки запросов к базе и формирование массива ответов в формате JSON
 */

namespace tc::io {

    using namespace std::literals;

    geo::Coordinates ParseCoordinates(const json::Node& request_node) {
        const json::Dict& request_dict = request_node.AsMap();
        return { request_dict.at("latitude"s).AsDouble(), request_dict.at("longitude"s).AsDouble() };
    }

    /**
     * Парсит маршрут.
     * Для кольцевого маршрута (A>B>C>A) возвращает массив названий остановок [A,B,C,A]
     * Для некольцевого маршрута (A-B-C-D) возвращает массив названий остановок [A,B,C,D,C,B,A]
     * string_view опирается на строки хранимые в json документе!
     */
    std::vector<std::string_view> ParseRoute(const json::Node& request_node) {
        const json::Array& stops = request_node.AsMap().at("stops"s).AsArray();
        bool is_roundtrip = request_node.AsMap().at("is_roundtrip"s).AsBool();

        std::vector<std::string_view> result;
        if (is_roundtrip) {
            result.reserve(stops.size());
        }
        else {
            result.reserve(stops.size() * 2 - 1);
        }

        for (const json::Node& node : stops) {
            result.push_back(node.AsString());
        }

        if (!is_roundtrip) {
            for (auto it = std::next(stops.crbegin()); it != stops.crend(); ++it) {
                result.push_back(it->AsString());
            }
        }

        return result;
    }

    std::vector<std::pair<std::string, int>> ParseDistances(const json::Node& request_node) {
        const json::Dict& road_distances = request_node.AsMap().at("road_distances"s).AsMap();
        std::vector<std::pair<std::string, int>> result;

        for (const std::pair<const std::string, json::Node>& kvp : road_distances) {
            result.emplace_back(std::make_pair(kvp.first, kvp.second.AsInt()));
        }

        return result;
    }

    json::Node PrintBusStat(const TransportCatalogue& catalogue, const json::Node& request_node) {
        assert(request_node.IsDict() && request_node.AsMap().at("type"s).AsString() == "Bus"s);
        const json::Dict& request_dict = request_node.AsMap();

        json::Builder builder{};

        builder.StartDict().Key("request_id"s).Value(request_dict.at("id"s).AsInt());
        BusPtr bus_ptr = catalogue.GetBus(request_dict.at("name"s).AsString());
        if (!bus_ptr) {
            return builder.Key("error_message"s).Value("not found"s).EndDict().Build();
        }
        RouteInfo info = catalogue.GetRouteInfo(bus_ptr);
        builder
            .Key("stop_count"s).Value(static_cast<int>(info.stops_count))
            .Key("unique_stop_count"s).Value(static_cast<int>(info.unique_stops_count))
            .Key("route_length"s).Value(static_cast<double>(info.distance))
            .Key("curvature"s).Value(info.curvature)
            .EndDict();

        return builder.Build();
    }

    json::Node PrintStopStat(const TransportCatalogue& catalogue, const json::Node& request_node) {
        assert(request_node.IsDict() && request_node.AsMap().at("type"s).AsString() == "Stop"s);
        const json::Dict& request_dict = request_node.AsMap();

        json::Builder builder{};

        builder.StartDict().Key("request_id"s).Value(request_dict.at("id"s).AsInt());
        StopPtr stop_ptr = catalogue.GetStop(request_dict.at("name"s).AsString());
        if (!stop_ptr) {
            return builder.Key("error_message"s).Value("not found"s).EndDict().Build();
        }

        const std::unordered_set<BusPtr>& buses_set = catalogue.GetBusesAtStop(stop_ptr);
        std::vector<BusPtr> buses(buses_set.begin(), buses_set.end());
        std::sort(buses.begin(), buses.end(), [](BusPtr lhs, BusPtr rhs) { return lhs->name < rhs->name; });

        builder.Key("buses"s).StartArray();
        for (BusPtr bus_ptr : buses) {
            builder.Value(bus_ptr->name);
        }

        return builder.EndArray().EndDict().Build();
    }

    json::Node PrintMapStat(const TransportCatalogue& catalogue, const json::Node& request_node, const renderer::MapRenderer& renderer) {
        assert(request_node.IsDict() && request_node.AsMap().at("type"s).AsString() == "Map"s);
        const json::Dict& request_dict = request_node.AsMap();

        json::Builder builder{};

        builder.StartDict().Key("request_id"s).Value(request_dict.at("id"s).AsInt());
        std::ostringstream output_stream;
        const svg::Document& svg_doc = renderer.RenderBuses(catalogue.GetAllBuses().begin(), catalogue.GetAllBuses().end());
        svg_doc.Render(output_stream);
        builder.Key("map"s).Value(output_stream.str()).EndDict();

        return builder.Build();
    }

    json::Node PrintRouteStat(const TransportCatalogue& catalogue, const json::Node& request_node, const router::TransportRouter& router) {
        assert(request_node.IsDict() && request_node.AsMap().at("type"s).AsString() == "Route"s);
        const json::Dict& request_dict = request_node.AsMap();

        StopPtr stop_from = catalogue.GetStop(request_dict.at("from"s).AsString());
        StopPtr stop_to = catalogue.GetStop(request_dict.at("to"s).AsString());

        std::optional<router::Route> route = router.FindRoute(stop_from, stop_to);

        json::Builder builder{};
        builder.StartDict().Key("request_id"s).Value(request_dict.at("id"s).AsInt());
        if (!route.has_value()) {
            return builder.Key("error_message"s).Value("not found"s).EndDict().Build();
        }
        builder.Key("total_time"s).Value((*route).total_time);
        builder.Key("items"s).StartArray();
        for (std::shared_ptr<router::RouteStat> route_stat : (*route).items) {
            router::RouteType type = route_stat->GetType();
            if (type == router::RouteType::WAIT) {
                const router::WaitRouteStat& stat = *(static_cast<router::WaitRouteStat*>(route_stat.get()));
                builder.StartDict()
                    .Key("type"s).Value("Wait"s)
                    .Key("stop_name"s).Value(std::string{ stat.stop_name })
                    .Key("time"s).Value(stat.time)
                    .EndDict();
            }
            else if (type == router::RouteType::BUS) {
                const router::BusRouteStat& stat = *(static_cast<router::BusRouteStat*>(route_stat.get()));
                builder.StartDict()
                    .Key("type"s).Value("Bus"s)
                    .Key("bus"s).Value(std::string{ stat.bus_name })
                    .Key("span_count"s).Value(stat.span_count)
                    .Key("time"s).Value(stat.time)
                    .EndDict();
            }
        }
        builder.EndArray().EndDict();

        return builder.Build();
    }

    svg::Color ParseColor(const json::Node& node) {
        if (node.IsString()) {
            return { node.AsString() };
        }
        if (!node.IsArray()) {
            throw std::invalid_argument("Unable to parse color"s);
        }
        const json::Array& array = node.AsArray();
        if (array.size() == 3) {
            return svg::Rgb{ static_cast<uint8_t>(array[0].AsInt()), static_cast<uint8_t>(array[1].AsInt()), static_cast<uint8_t>(array[2].AsInt()) };
        }
        if (array.size() == 4) {
            return svg::Rgba{ static_cast<uint8_t>(array[0].AsInt()), static_cast<uint8_t>(array[1].AsInt()), static_cast<uint8_t>(array[2].AsInt()), array[3].AsDouble() };
        }
        throw std::invalid_argument("Unable to parse color"s);
    }

    svg::Point ParsePoint(const json::Node& node) {
        if (!node.IsArray() || node.AsArray().size() != 2) {
            throw std::invalid_argument("Unable to parse point"s);
        }
        const json::Array& array = node.AsArray();
        return { array[0].AsDouble(), array[1].AsDouble() };
    }

    JsonReader::JsonReader() : json_(nullptr) {
    }

    void JsonReader::LoadJson(std::istream& input) {
        json_ = json::Load(input);
        assert(json_.GetRoot().IsDict());
	}

	void JsonReader::ApplyCommands(tc::TransportCatalogue& catalogue) const {
        json::Node base_requests_node = json_.GetRoot().AsMap().at("base_requests"s);
        assert(base_requests_node.IsArray());

        // Парсим и сохраняем остановки
        for (const json::Node& node : base_requests_node.AsArray()) {
            const json::Dict& request_dict = node.AsMap();
            if (request_dict.at("type"s).AsString() != "Stop"s) {
                continue;
            }
            catalogue.AddStop(request_dict.at("name"s).AsString(), ParseCoordinates(node));
        }

        // Парсим и сохраняем маршруты
        for (const json::Node& node : base_requests_node.AsArray()) {
            const json::Dict& request_dict = node.AsMap();
            if (request_dict.at("type"s).AsString() != "Bus"s) {
                continue;
            }
            std::vector<StopPtr> stop_ptrs;
            for (const auto& stop_name : ParseRoute(node)) {
                stop_ptrs.push_back(catalogue.GetStop(stop_name));
            }
            std::string_view end_stop_name = (*(request_dict.at("stops"s).AsArray().rbegin())).AsString();
            StopPtr end_stop_ptr = catalogue.GetStop(end_stop_name);
            catalogue.AddBus(request_dict.at("name"s).AsString(), stop_ptrs, end_stop_ptr, request_dict.at("is_roundtrip"s).AsBool());
        }

        // Парсим и сохраняем расстояния между остановками
        for (const json::Node& node : base_requests_node.AsArray()) {
            const json::Dict& request_dict = node.AsMap();
            if (request_dict.at("type"s).AsString() != "Stop"s) {
                continue;
            }
            StopPtr stop_from_ptr = catalogue.GetStop(request_dict.at("name"s).AsString());
            for (std::pair<std::string, int>& distance_pair : ParseDistances(node)) {
                StopPtr stop_to_ptr = catalogue.GetStop(distance_pair.first);
                catalogue.SetDistance(stop_from_ptr, stop_to_ptr, distance_pair.second);
            }
        }
	}

    void JsonReader::SaveStats(const tc::TransportCatalogue& catalogue, std::ostream& output, const renderer::MapRenderer& renderer, const router::TransportRouter& router) const {
        const auto& iterator = json_.GetRoot().AsMap().find("stat_requests"s);
        if (iterator == json_.GetRoot().AsMap().end()) {
            return;
        }
        json::Node stat_requests_node = iterator->second;
        assert(stat_requests_node.IsArray());

        json::Builder builder{};

        builder.StartArray();

        for (const json::Node& node : stat_requests_node.AsArray()) {
            const std::string& type = node.AsMap().at("type"s).AsString();
            if (type == "Bus"s) {
                builder.Value(PrintBusStat(catalogue, node).AsMap());
            }
            else if (type == "Stop"s) {
                builder.Value(PrintStopStat(catalogue, node).AsMap());
            }
            else if (type == "Map"s) {
                builder.Value(PrintMapStat(catalogue, node, renderer).AsMap());
            }
            else if (type == "Route"s) {
                builder.Value(PrintRouteStat(catalogue, node, router).AsMap());
            }
        }

        json::Document json_doc{ builder.EndArray().Build() };
        json::Print(json_doc, output);
    }

    renderer::RenderSettings JsonReader::GetRenderSettings() const {
        json::Dict render_settings_dict = json_.GetRoot().AsMap().at("render_settings"s).AsMap();

        renderer::RenderSettings settings{};

        settings.width = render_settings_dict.at("width"s).AsDouble();
        settings.height = render_settings_dict.at("height"s).AsDouble();

        settings.padding = render_settings_dict.at("padding"s).AsDouble();

        settings.line_width = render_settings_dict.at("line_width"s).AsDouble();
        settings.stop_radius = render_settings_dict.at("stop_radius"s).AsDouble();

        settings.bus_label_font_size = render_settings_dict.at("bus_label_font_size"s).AsInt();
        settings.bus_label_offset = ParsePoint(render_settings_dict.at("bus_label_offset"s));

        settings.stop_label_font_size = render_settings_dict.at("stop_label_font_size"s).AsInt();
        settings.stop_label_offset = ParsePoint(render_settings_dict.at("stop_label_offset"s));

        settings.underlayer_color = ParseColor(render_settings_dict.at("underlayer_color"s));
        settings.underlayer_width = render_settings_dict.at("underlayer_width"s).AsDouble();

        std::vector<svg::Color> color_pallete{};

        for (const json::Node& node : render_settings_dict.at("color_palette"s).AsArray()) {
            color_pallete.push_back(ParseColor(node));
        }

        settings.color_pallete = color_pallete;

        return settings;
    }

    router::RoutingSettings JsonReader::GetRoutingSettings() const {
        json::Dict routing_settings_dict = json_.GetRoot().AsMap().at("routing_settings"s).AsMap();

        router::RoutingSettings settings{};

        settings.bus_velocity = routing_settings_dict.at("bus_velocity"s).AsDouble();
        settings.bus_wait_time = routing_settings_dict.at("bus_wait_time"s).AsInt();

        return settings;
    }

} // namespace tc::io;