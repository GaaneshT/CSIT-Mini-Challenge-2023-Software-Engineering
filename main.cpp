#include <iostream>
#include <cpprest/http_listener.h>
#include <cpprest/json.h>
#include <mongocxx/client.hpp>
#include <mongocxx/instance.hpp>
#include <mongocxx/uri.hpp>
#include <bsoncxx/json.hpp>
#include <bsoncxx/builder/stream/document.hpp>
#include <chrono>   // for date and time operations
#include <iomanip>  // for std::get_time
#include <sstream>  // for std::istringstream



using namespace web;
using namespace web::http;
using namespace web::http::experimental::listener;


mongocxx::instance inst{};  // Create a new instance

// Function to handle request to endpoint



void flightRequest(const http_request& request) {
    std::vector<bsoncxx::document::view> departureDocs;
    std::vector<bsoncxx::document::view> returnDocs;
    departureDocs.clear();
    returnDocs.clear();

    std::cout << "flightRequest function called." << std::endl;

    // Extract the query parameters
    utility::string_t departureDateKey = "departureDate";
    utility::string_t returnDateKey = "returnDate";
    utility::string_t destinationKey = "destination";

    std::map<utility::string_t, utility::string_t> queryParameters = web::uri::split_query(request.request_uri().query());

    utility::string_t departureDate;
    utility::string_t returnDate;
    utility::string_t destination;

    for (const auto& kvp : queryParameters) {
        const utility::string_t& key = kvp.first;
        const utility::string_t& value = kvp.second;

        if (key == departureDateKey) {
            departureDate = value;
        } else if (key == returnDateKey) {
            returnDate = value;
        } else if (key == destinationKey) {
            destination = value;
        }
    }

    std::string departureDateString = departureDate + "T00:00:00.000Z";
    std::string returnDateString = returnDate + "T00:00:00.000Z";

    std::cout << "departureDate: " << departureDateString << std::endl;
    std::cout << "returnDate: " << returnDateString << std::endl;
    std::cout << "destination: " << destination << std::endl;

    mongocxx::uri uri("mongodb+srv://userReadOnly:7ZT817O8ejDfhnBM@minichallenge.q4nve1r.mongodb.net/");
    mongocxx::client conn{uri};
    mongocxx::database db = conn["minichallenge"];
    mongocxx::collection flights = db["flights"];

    std::string departureFilter = "{ \"date\": { \"$eq\": { \"$date\": \"" + departureDateString + "\" } }, \"destcity\": \"" + destination + "\", \"srccity\": \"Singapore\" }";
    std::string returnFilter = "{ \"date\": { \"$eq\": { \"$date\": \"" + returnDateString + "\" } }, \"destcity\": \"Singapore\", \"srccity\": \"" + destination + "\" }";

    std::cout << "Departure filter: " << departureFilter << std::endl;
    std::cout << "Return filter: " << returnFilter << std::endl;

    mongocxx::options::find opts{};
    opts.projection(bsoncxx::builder::stream::document{} << "destcity" << 1 << "date" << 1 << "airlinename" << 1 << "price" << 1 << bsoncxx::builder::stream::finalize);

    bsoncxx::document::view_or_value departureFilterView = bsoncxx::from_json(departureFilter);
    bsoncxx::document::view_or_value returnFilterView = bsoncxx::from_json(returnFilter);

    mongocxx::cursor departureCursor = flights.find(departureFilterView, opts);
    for (const auto& doc : departureCursor) {
        departureDocs.push_back(doc);
    }

    mongocxx::cursor returnCursor = flights.find(returnFilterView, opts);
    for (const auto& doc : returnCursor) {
        returnDocs.push_back(doc);
    }

    std::cout << "Number of departure documents: " << departureDocs.size() << std::endl;
    std::cout << "Number of return documents: " << returnDocs.size() << std::endl;

    // Check if any of the required query parameters are missing
    if (departureDocs.empty() || returnDocs.empty()) {
        http_response response(status_codes::BadRequest);
        response.set_body("No flights found for the given parameters");
        request.reply(response);
        return;
    }

    // Find the cheapest departure and return flights
    bsoncxx::document::view cheapestDepartureDoc;
    bsoncxx::document::view cheapestReturnDoc;
    int cheapestDeparturePrice = INT_MAX;
    int cheapestReturnPrice = INT_MAX;

    for (const auto& departureDoc : departureDocs) {
        int departurePrice = departureDoc["price"].get_int32().value;
        if (departurePrice < cheapestDeparturePrice) {
            cheapestDepartureDoc = departureDoc;
            cheapestDeparturePrice = departurePrice;
        }
    }

    for (const auto& returnDoc : returnDocs) {
        int returnPrice = returnDoc["price"].get_int32().value;
        if (returnPrice < cheapestReturnPrice) {
            cheapestReturnDoc = returnDoc;
            cheapestReturnPrice = returnPrice;
        }
    }

    // Prepare the JSON response
    web::json::value responseJSON = web::json::value::array();

    web::json::value flightJSON;
    flightJSON["City"] = web::json::value::string(destination);
    flightJSON["Departure Date"] = web::json::value::string(departureDate);
    flightJSON["Departure Airline"] = web::json::value::string(cheapestDepartureDoc["airlinename"].get_utf8().value.to_string());
    flightJSON["Departure Price"] = web::json::value::number(cheapestDeparturePrice);
    flightJSON["Return Date"] = web::json::value::string(returnDate);
    flightJSON["Return Airline"] = web::json::value::string(cheapestReturnDoc["airlinename"].get_utf8().value.to_string());
    flightJSON["Return Price"] = web::json::value::number(cheapestReturnPrice);
    responseJSON[responseJSON.size()] = flightJSON;

    std::string jsonResponse = responseJSON.serialize();
    std::cout << "Response JSON: " << jsonResponse << std::endl;

    // Create the JSON response string
    std::stringstream responseStream;
    responseStream << jsonResponse;

    // Send the JSON response
    http_response response(status_codes::OK);
    response.headers().set_content_type("application/json");
    response.set_body(responseStream.str());
    request.reply(response);
}

void hotelsRequest(const http_request& request){
    // TO-DO
    

}

void handleRequest(const http_request& request) {
    auto path = request.request_uri().path();

    if (path == "/flight") {
        // Handle flights request
        flightRequest(request);
    } else if (path == "/hotel") {
        // Handle hotels request
        hotelsRequest(request);
    } else {
        // Invalid endpoint, return 404 Not Found
        http_response response(status_codes::NotFound);
        response.set_body("Invalid endpoint");
        request.reply(response);
    }
}
int main() {


    http_listener listener("http://localhost:8080");
    // Register handleRequest as the request handler for all endpoints
    listener.support(methods::GET, handleRequest);

    try {
        listener.open().wait();

        std::cout << "Listening on port 8080..." << std::endl;

        // Keep the main thread running
        while (true)
            ;
    } catch (const std::exception& e) {
        std::cout << "Error: " << e.what() << std::endl;
    }

    return 0;
}
// g++ -std=c++11 -I/usr/local/include/mongocxx/v_noabi -I/usr/local/include/bsoncxx/v_noabi -I/usr/include/cpprest -o output_file main.cpp -lmongocxx -lbsoncxx -lcpprest -lssl -lcrypto -Wl,-rpath,/usr/local/lib