#include <execution>
#include <algorithm>
#include <list>
 
#include "process_queries.h"
 
std::vector<std::vector<Document>> ProcessQueries(const SearchServer& search_server,
    const std::vector<std::string>& queries){
    
    std::vector<std::vector<Document>> helper(queries.size());
    std::transform(std::execution::par, 
                   queries.begin(), 
                   queries.end(), 
                   helper.begin(),
                   [&search_server](std::string query){
                        return search_server.FindTopDocuments(query);
                    });
    return helper;
}

std::vector<Document> ProcessQueriesJoined(const SearchServer& search_server,
    const std::vector<std::string>& queries){
 
    return std::transform_reduce(std::execution::par,
                                 queries.begin(), 
                                 queries.end(),
                                 std::vector<Document>{},
                                 [](std::vector<Document> lhs, std::vector<Document> const& rhs){
                                     lhs.insert(lhs.end(), rhs.begin(), rhs.end());
                                     return lhs;
                                 },             
                                 [&search_server](const std::string& query){ 
                                     return search_server.FindTopDocuments(query); 
                                 });
}