#include <iostream>
#include <string>

#include "json_reader.h"
#include "map_renderer.h"
#include "request_handler.h"
#include "transport_catalogue.h"

using namespace std;

int main() {
    /*
     * Примерная структура программы:
     *
     * Считать JSON из stdin
     * Построить на его основе JSON базу данных транспортного справочника
     * Выполнить запросы к справочнику, находящиеся в массива "stat_requests", построив JSON-массив
     * с ответами Вывести в stdout ответы в виде JSON
     */

    tc::TransportCatalogue catalogue;
    tc::io::JsonReader reader;
    reader.LoadJson(cin);
    reader.ApplyCommands(catalogue);
    renderer::RenderSettings render_settings = reader.GetRenderSettings();
    renderer::MapRenderer renderer{ render_settings };
    tc::router::RoutingSettings routing_settings = reader.GetRoutingSettings();
    tc::router::TransportRouter router{ catalogue, routing_settings };
    //RequestHandler handler{ catalogue, renderer };
    //handler.RenderMap().Render(cout);
    reader.SaveStats(catalogue, cout, renderer, router);
}