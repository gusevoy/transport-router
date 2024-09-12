#include "request_handler.h"

/*
 * Здесь можно было бы разместить код обработчика запросов к базе, содержащего логику, которую не
 * хотелось бы помещать ни в transport_catalogue, ни в json reader.
 *
 * Если вы затрудняетесь выбрать, что можно было бы поместить в этот файл,
 * можете оставить его пустым.
 */

RequestHandler::RequestHandler(const tc::TransportCatalogue& db, const renderer::MapRenderer& renderer)
	: db_(db), renderer_(renderer) {
}

// Возвращает информацию о маршруте (запрос Bus)
std::optional<tc::RouteInfo> RequestHandler::GetRouteInfo(const std::string_view& bus_name) const {
	if (tc::BusPtr bus_ptr = db_.GetBus(bus_name)) {
		return db_.GetRouteInfo(bus_ptr);
	}
	else {
		return std::nullopt;
	}
}

// Возвращает маршруты, проходящие через
const std::unordered_set<tc::BusPtr>* RequestHandler::GetBusesByStop(const std::string_view& stop_name) const {
	if (tc::StopPtr stop_ptr = db_.GetStop(stop_name)) {
		return &db_.GetBusesAtStop(stop_ptr);
	}
	else {
		return nullptr;
	}
}

// Этот метод будет нужен в следующей части итогового проекта
svg::Document RequestHandler::RenderMap() const {
	const auto& buses = db_.GetAllBuses();
	return renderer_.RenderBuses(buses.begin(), buses.end());
}