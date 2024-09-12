/*
 * Здесь можно разместить код наполнения транспортного справочника данными из JSON,
 * а также код обработки запросов к базе и формирование массива ответов в формате JSON
 */

#pragma once

#include <vector>

#include "json.h"
#include "map_renderer.h"
#include "transport_catalogue.h"
#include "transport_router.h"

namespace tc::io {

    class JsonReader {
    public:
        JsonReader();

        void LoadJson(std::istream& input);
        void ApplyCommands(tc::TransportCatalogue& catalogue) const;
        void SaveStats(const tc::TransportCatalogue& catalogue, std::ostream& output, const renderer::MapRenderer& renderer, const tc::router::TransportRouter& router) const;
        renderer::RenderSettings GetRenderSettings() const;
        router::RoutingSettings GetRoutingSettings() const;

    private:
        json::Document json_;
    };

} // namespace tc::io