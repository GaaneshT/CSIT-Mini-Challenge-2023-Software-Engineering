#include <iostream>
#include <cpprest/http_listener.h>
#include <cpprest/json.h>
#include <mongocxx/client.hpp>
#include <mongocxx/instance.hpp>
#include <mongocxx/uri.hpp>
#include <bsoncxx/json.hpp>
#include <bsoncxx/builder/stream/document.hpp>
#include <chrono> 
#include <iomanip> 
#include <sstream>  
#include <regex>



using namespace web;
using namespace web::http;
using namespace web::http::experimental::listener;


mongocxx::instance inst{};  // Create a new instance

// Function to handle request to endpoint


bool IsValidDateFormat(const std::string& date) {
    // Regular expression pattern for yyyy-mm-dd format
    std::regex pattern("\\d{4}-\\d{2}-\\d{2}");

    // Check if the date matches the pattern
    if (!std::regex_match(date, pattern)) {
        return false;
    }

    // Parse the date using std::istringstream
    std::istringstream dateStream(date);
    int year, month, day;
    char separator1, separator2;
    dateStream >> year >> separator1 >> month >> separator2 >> day;

    // Check if the date was successfully parsed and is valid
    if (!dateStream || separator1 != '-' || separator2 != '-' || month < 1 || month > 12 || day < 1 || day > 31) {
        return false;
    }

    // Check if the date is a real calendar date
    std::tm timeinfo = {};
    timeinfo.tm_year = year - 1900;
    timeinfo.tm_mon = month - 1;
    timeinfo.tm_mday = day;

    std::time_t time = std::mktime(&timeinfo);

    return time != -1;
}





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

    // Check if the dates are valid
    if (!IsValidDateFormat(departureDate) || !IsValidDateFormat(returnDate)) {
        http_response response(status_codes::BadRequest);
        response.set_body("Invalid date format for Departure Date or Return Date");
        request.reply(response);
        return;
    }

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

    if (departureDate.empty() || returnDate.empty() || destination.empty()) {
        http_response response(status_codes::BadRequest);
        response.set_body("Missing required query parameters");
        request.reply(response);
        return;
    }

    // Check if any of the flights are missing
    if (departureDocs.empty() && returnDocs.empty()) {
        http_response response(status_codes::OK);
        response.set_body("[]"); // Return an empty list
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
    std::ostringstream responseStream;
    responseStream << "[ { ";
    responseStream << "\"City\": \"" << destination << "\", ";
    responseStream << "\"Departure Date\": \"" << departureDate << "\", ";
    responseStream << "\"Departure Airline\": \"" << cheapestDepartureDoc["airlinename"].get_utf8().value.to_string() << "\", ";
    responseStream << "\"Departure Price\": " << cheapestDeparturePrice << ", ";
    responseStream << "\"Return Date\": \"" << returnDate << "\", ";
    responseStream << "\"Return Airline\": \"" << cheapestReturnDoc["airlinename"].get_utf8().value.to_string() << "\", " ;
    responseStream << "\"Return Price\": " << cheapestReturnPrice;
    responseStream << " } ]";

    std::string jsonResponse = responseStream.str();

    // Send the JSON response
    http_response response(status_codes::OK);
    response.headers().set_content_type("application/json");
    response.set_body(jsonResponse);
    request.reply(response);
}


void hotelsRequest(const http_request& request){
    // TO-DO
    std::vector<bsoncxx::document::view> hotelDocs;
    std::cout<< "hotelsRequest function called!"<< std::endl;
    // The key here is to match the query
    utility::string_t checkOutDateKey = "checkOutDate";
    utility::string_t checkInDateKey = "checkInDate";
    utility::string_t destinationKey = "destination";

    std::map<utility::string_t, utility::string_t> queryParameters = web::uri::split_query(request.request_uri().query());

    utility::string_t checkOutDate;
    utility::string_t checkInDate;
    utility::string_t destination;

    for (const auto& kvp : queryParameters) {
        const utility::string_t& key = kvp.first;
        const utility::string_t& value = kvp.second;

        if (key == checkOutDateKey) {
            checkOutDate = value;
        } else if (key == checkInDateKey) {
            checkInDate = value;
        } else if (key == destinationKey) {
            destination = value;
        }
    }
        if (!IsValidDateFormat(checkOutDate) || !IsValidDateFormat(checkInDate)) {
        http_response response(status_codes::BadRequest);
        response.set_body("Invalid date format for check In Date or check Out Date");
        request.reply(response);
        return;
    }
    std::string checkInDateString =  checkInDate+ "T00:00:00.000Z";
    std::string checkOutDateString = checkOutDate + "T00:00:00.000Z";
    // Testing - Passed
    std::cout<<"Check in date is : " << checkInDateString<<std::endl;
    std::cout<<"Check out date is : " << checkOutDateString<<std::endl;
    std::cout<<"Destination : " << destination<<std::endl;

    // Connect to the database
    mongocxx::uri uri("mongodb+srv://userReadOnly:7ZT817O8ejDfhnBM@minichallenge.q4nve1r.mongodb.net/");
    mongocxx::client conn{uri};
    mongocxx::database db = conn["minichallenge"];
    mongocxx::collection flights = db["hotels"];

    /*
    Filter
    {
  "date": {
    "$gte": ISODate(<CHECK IN DATE>),
	"$lte": ISODate(<CHECK OUT DATE>)
  },
  "city": "<Destination>"
}
    
    */
    
    std::string hotelFilter = "{ \"date\": { \"$gte\": { \"$date\": \"" + checkInDateString + "\" }, \"$lte\": { \"$date\": \"" + checkOutDateString + "\" } }, \"city\": \"" + destination + "\" }";
    std::cout << "filter is: "<< hotelFilter <<std::endl;

    mongocxx::options::find opts{};
    opts.projection(bsoncxx::builder::stream::document{} << "city" << 1 << "date" << 1 <<"hotelName" << 1 << "price" << 1 << bsoncxx::builder::stream::finalize);

    bsoncxx::document::view_or_value HotelFilterView = bsoncxx::from_json(hotelFilter);

    mongocxx::cursor Hotelcursor = flights.find(HotelFilterView, opts);
    for (const auto& doc : Hotelcursor) {
        hotelDocs.push_back(doc);
    }


    std::cout << "Number of hotel documents: " << hotelDocs.size() << std::endl;

    std::vector<std::string> hotelNames;
    std::unordered_map<std::string, int> hotelPrices;

    // Retrieve hotel names and initialize prices to 0
    for (const auto& doc : hotelDocs) {
        std::string hotelName = doc["hotelName"].get_utf8().value.to_string();
        int price = doc["price"].get_int32().value;

        // Check if the hotel name already exists in the map
        if (hotelPrices.find(hotelName) == hotelPrices.end()) {
            hotelNames.push_back(hotelName);
            hotelPrices[hotelName] = 0;
        }

        // Accumulate prices for the hotel
        hotelPrices[hotelName] += price;
    }

    // Find the cheapest hotel
    std::string cheapestHotel;
    int cheapestPrice = INT_MAX;
    for (const auto& hotel : hotelPrices) {
        std::string hotelName = hotel.first;
        int price = hotel.second;
        if (price < cheapestPrice) {
            cheapestHotel = hotelName;
            cheapestPrice = price;
        }
    }

    std::string checkInDateStr = utility::conversions::to_utf8string(checkInDate);
    std::string checkOutDateStr = utility::conversions::to_utf8string(checkOutDate);

    std::ostringstream responseStream;
    responseStream << "[";

    // Loop over each hotel and add it to the response
    bool isFirst = true;
    for (const auto& hotel : hotelPrices) {
        std::string hotelName = hotel.first;
        int price = hotel.second;

        // Add the hotel to the response only if it has the cheapest price
        if (price == cheapestPrice) {
            if (!isFirst) {
                responseStream << ",";
            }
            isFirst = false;

            responseStream << "{";
            responseStream << "\"City\": \"" << destination << "\",";
            responseStream << "\"Check In Date\": \"" << checkInDateStr << "\",";
            responseStream << "\"Check Out Date\": \"" << checkOutDateStr << "\",";
            responseStream << "\"Hotel\": \"" << hotelName << "\",";
            responseStream << "\"Price\": " << price;
            responseStream << "}";
        }
    }

    responseStream << "]";

    std::string jsonResponse = responseStream.str();

    // Send the JSON response
    http_response response(status_codes::OK);
    response.headers().set_content_type("application/json");
    response.set_body(jsonResponse);
    request.reply(response);


}

void handleRequest(const http_request& request) {
    std::cout << "Called handleRequest"<<std::endl;
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


    http_listener listener("http://0.0.0.0:8080");
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