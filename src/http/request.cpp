#include "request.h"
#include <sstream>
#include<iostream>

void HTTPRequest::parse(std::string raw_request){
    std::istringstream stream(raw_request);

    // note, the >> operator auto reads upto the first space;
    stream>>method; // "GET"
    stream>>path; // "/"
    stream>>version; //"HTTP/1.1"
    
    // std::cout<<"Method: "<<method<<"\n";
    // std::cout<<"Path: "<<path<<"\n";
}