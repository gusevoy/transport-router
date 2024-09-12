#include "geo.h"
#include "map_renderer.h"

#include <cassert>
#include <unordered_set>

/*
 * В этом файле вы можете разместить код, отвечающий за визуализацию карты маршрутов в формате SVG.
 * Визуализация маршртутов вам понадобится во второй части итогового проекта.
 * Пока можете оставить файл пустым.
 */

namespace renderer {

    inline const double EPSILON = 1e-6;

    bool IsZero(double value) {
        return std::abs(value) < EPSILON;
    }

    class SphereProjector {
    public:
        // points_begin и points_end задают начало и конец интервала элементов geo::Coordinates
        template <typename PointInputIt>
        SphereProjector(PointInputIt points_begin, PointInputIt points_end,
            double max_width, double max_height, double padding)
            : padding_(padding) //
        {
            // Если точки поверхности сферы не заданы, вычислять нечего
            if (points_begin == points_end) {
                return;
            }

            // Находим точки с минимальной и максимальной долготой
            const auto [left_it, right_it] = std::minmax_element(
                points_begin, points_end,
                [](auto lhs, auto rhs) { return lhs.lng < rhs.lng; });
            min_lon_ = left_it->lng;
            const double max_lon = right_it->lng;

            // Находим точки с минимальной и максимальной широтой
            const auto [bottom_it, top_it] = std::minmax_element(
                points_begin, points_end,
                [](auto lhs, auto rhs) { return lhs.lat < rhs.lat; });
            const double min_lat = bottom_it->lat;
            max_lat_ = top_it->lat;

            // Вычисляем коэффициент масштабирования вдоль координаты x
            std::optional<double> width_zoom;
            if (!IsZero(max_lon - min_lon_)) {
                width_zoom = (max_width - 2 * padding) / (max_lon - min_lon_);
            }

            // Вычисляем коэффициент масштабирования вдоль координаты y
            std::optional<double> height_zoom;
            if (!IsZero(max_lat_ - min_lat)) {
                height_zoom = (max_height - 2 * padding) / (max_lat_ - min_lat);
            }

            if (width_zoom && height_zoom) {
                // Коэффициенты масштабирования по ширине и высоте ненулевые,
                // берём минимальный из них
                zoom_coeff_ = std::min(*width_zoom, *height_zoom);
            }
            else if (width_zoom) {
                // Коэффициент масштабирования по ширине ненулевой, используем его
                zoom_coeff_ = *width_zoom;
            }
            else if (height_zoom) {
                // Коэффициент масштабирования по высоте ненулевой, используем его
                zoom_coeff_ = *height_zoom;
            }
        }

        // Проецирует широту и долготу в координаты внутри SVG-изображения
        svg::Point operator()(geo::Coordinates coords) const {
            return {
                (coords.lng - min_lon_) * zoom_coeff_ + padding_,
                (max_lat_ - coords.lat) * zoom_coeff_ + padding_
            };
        }

    private:
        double padding_;
        double min_lon_ = 0;
        double max_lat_ = 0;
        double zoom_coeff_ = 0;
    };

	MapRenderer::MapRenderer(RenderSettings settings) : settings_(std::move(settings)) {
	}

    void RenderBusLines(svg::Document& svg_doc, const std::vector<const tc::Bus*>& bus_ptrs, const SphereProjector& projector, const RenderSettings& settings) {
        svg::Polyline polyline_template{};
        polyline_template
            .SetFillColor("none"s)
            .SetStrokeWidth(settings.line_width)
            .SetStrokeLineCap(svg::StrokeLineCap::ROUND)
            .SetStrokeLineJoin(svg::StrokeLineJoin::ROUND);

        auto color_iterator = settings.color_pallete.cbegin();
        auto color_iterator_end = settings.color_pallete.cend();
        assert(color_iterator != color_iterator_end);
        for (const tc::Bus* bus_ptr : bus_ptrs) {
            if (bus_ptr->stops.size() == 0) {
                continue;
            }
            svg::Polyline polyline{ polyline_template };
            polyline.SetStrokeColor(*color_iterator);
            if (++color_iterator == color_iterator_end) {
                color_iterator = settings.color_pallete.cbegin();
            }
            for (const tc::Stop* stop_ptr : bus_ptr->stops) {
                polyline.AddPoint(projector(stop_ptr->coordinates));
            }
            svg_doc.Add(polyline);
        }
    }

    void RenderBusNames(svg::Document& svg_doc, const std::vector<const tc::Bus*>& bus_ptrs, const SphereProjector& projector, const RenderSettings& settings) {
        svg::Text underlayer_template{};
        underlayer_template
            .SetOffset(settings.bus_label_offset)
            .SetFontSize(settings.bus_label_font_size)
            .SetFontFamily("Verdana"s)
            .SetFontWeight("bold"s)
            .SetFillColor(settings.underlayer_color)
            .SetStrokeColor(settings.underlayer_color)
            .SetStrokeWidth(settings.underlayer_width)
            .SetStrokeLineCap(svg::StrokeLineCap::ROUND)
            .SetStrokeLineJoin(svg::StrokeLineJoin::ROUND);

        svg::Text text_template{};
        text_template
            .SetOffset(settings.bus_label_offset)
            .SetFontSize(settings.bus_label_font_size)
            .SetFontFamily("Verdana"s)
            .SetFontWeight("bold"s);

        auto color_iterator = settings.color_pallete.cbegin();
        auto color_iterator_end = settings.color_pallete.cend();
        assert(color_iterator != color_iterator_end);
        for (const tc::Bus* bus_ptr : bus_ptrs) {
            size_t stops_count = bus_ptr->stops.size();
            if (stops_count == 0) {
                continue;
            }
            const tc::Stop* start_stop_ptr = bus_ptr->stops[0];
            svg::Point start_stop_point = projector(start_stop_ptr->coordinates);
            svg_doc.Add(svg::Text{ underlayer_template }.SetPosition(start_stop_point).SetData(bus_ptr->name));
            svg_doc.Add(svg::Text{ text_template }.SetPosition(start_stop_point).SetData(bus_ptr->name).SetFillColor(*color_iterator));
            if (bus_ptr->end_stop_ptr != start_stop_ptr) {
                svg::Point end_stop_point = projector(bus_ptr->end_stop_ptr->coordinates);
                svg_doc.Add(svg::Text{ underlayer_template }.SetPosition(end_stop_point).SetData(bus_ptr->name));
                svg_doc.Add(svg::Text{ text_template }.SetPosition(end_stop_point).SetData(bus_ptr->name).SetFillColor(*color_iterator));
            }
            if (++color_iterator == color_iterator_end) {
                color_iterator = settings.color_pallete.cbegin();
            }
        }
    }

    void RenderStopRings(svg::Document& svg_doc, const std::vector<const tc::Stop*>& stop_ptrs, const SphereProjector& projector, const RenderSettings& settings) {
        svg::Circle circle_template{};
        circle_template.SetRadius(settings.stop_radius).SetFillColor("white"s);
        for (const tc::Stop* stop_ptr : stop_ptrs) {
            svg_doc.Add(svg::Circle{ circle_template }.SetCenter(projector(stop_ptr->coordinates)));
        }
    }

    void RenderStopNames(svg::Document& svg_doc, const std::vector<const tc::Stop*>& stop_ptrs, const SphereProjector& projector, const RenderSettings& settings) {
        svg::Text underlayer_template{};
        underlayer_template
            .SetOffset(settings.stop_label_offset)
            .SetFontSize(settings.stop_label_font_size)
            .SetFontFamily("Verdana"s)
            .SetFillColor(settings.underlayer_color)
            .SetStrokeColor(settings.underlayer_color)
            .SetStrokeWidth(settings.underlayer_width)
            .SetStrokeLineCap(svg::StrokeLineCap::ROUND)
            .SetStrokeLineJoin(svg::StrokeLineJoin::ROUND);

        svg::Text text_template{};
        text_template
            .SetOffset(settings.stop_label_offset)
            .SetFontSize(settings.stop_label_font_size)
            .SetFontFamily("Verdana"s)
            .SetFillColor("black"s);

        for (const tc::Stop* stop_ptr : stop_ptrs) {
            svg::Point stop_point = projector(stop_ptr->coordinates);
            svg_doc.Add(svg::Text{ underlayer_template }.SetPosition(stop_point).SetData(stop_ptr->name));
            svg_doc.Add(svg::Text{ text_template }.SetPosition(stop_point).SetData(stop_ptr->name));
        }
    }

    void MapRenderer::RenderVectorOfBuses(svg::Document& svg_doc, const std::vector<const tc::Bus*>& bus_ptrs) const {
        std::unordered_set<const tc::Stop*> stops_set;
        std::vector<geo::Coordinates> all_coordinates;
        for (const tc::Bus* bus_ptr : bus_ptrs) {
            for (const tc::Stop* stop_ptr : bus_ptr->stops) {
                all_coordinates.push_back(stop_ptr->coordinates);
                stops_set.insert(stop_ptr);
            }
        }

        SphereProjector projector{ all_coordinates.begin(), all_coordinates.end(),
            settings_.width, settings_.height, settings_.padding };

        RenderBusLines(svg_doc, bus_ptrs, projector, settings_);
        RenderBusNames(svg_doc, bus_ptrs, projector, settings_);

        std::vector<const tc::Stop*> stops_vector{ stops_set.begin(), stops_set.end() };
        std::sort(stops_vector.begin(), stops_vector.end(), [](const tc::Stop* lhs, const tc::Stop* rhs) { return lhs->name < rhs->name; });
        RenderStopRings(svg_doc, stops_vector, projector, settings_);
        RenderStopNames(svg_doc, stops_vector, projector, settings_);
    }

} // namespace renderer