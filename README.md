# CSIT Mini Challenge - Flight and Hotel API

This repository is part of my submission for the CSIT Mini Challenge. The codebase includes a C++ application that serves as a backend service to find the best deals on flights and hotels.

## Application Overview

The service exposes two endpoints, `/flight` and `/hotel`, which users can query to retrieve the cheapest flight and hotel options, respectively. It is designed to run on a local server and interact with a MongoDB database for data retrieval.

## Features

- **Flight Search**: Users can search for the cheapest flights from Singapore to a specified destination on a particular date.
- **Hotel Search**: Users can find the cheapest hotel options in a specified city within a given date range.

## Dependencies

- C++11 Standard
- C++ REST SDK (Casablanca)
- MongoDB C++ Driver (mongocxx)
- BSON Library (bsoncxx)

## Building the Project

To compile the application, use the g++ compiler with the following flags:

```bash
g++ -std=c++11 -I/usr/local/include/mongocxx/v_noabi -I/usr/local/include/bsoncxx/v_noabi -I/usr/include/cpprest -o SuppBot main.cpp -lmongocxx -lbsoncxx -lcpprest -lssl -lcrypto -Wl,-rpath,/usr/local/lib
```

## Running the Application

After compilation, you can start the server with:

```bash
./SuppBot
```

The server will start listening on port `8080`.

## API Usage

To search for flights or hotels, make GET requests to the server with the required query parameters:

```http
GET http://localhost:8080/flight?departureDate=YYYY-MM-DD&returnDate=YYYY-MM-DD&destination=CityName
GET http://localhost:8080/hotel?checkInDate=YYYY-MM-DD&checkOutDate=YYYY-MM-DD&destination=CityName
```

Replace `YYYY-MM-DD` with the actual date and `CityName` with the destination city name.

## Validation

The application includes date validation to ensure the correct format is used in queries.

## Contributing

Feel free to fork this project and submit pull requests with enhancements for the CSIT Mini Challenge.

## License

This project is licensed under the MIT License - see the LICENSE.md file for details.

## Acknowledgments

- CSIT Mini Challenge organizers for hosting this event.
- Contributors and maintainers of the C++ REST SDK and MongoDB C++ Driver.
