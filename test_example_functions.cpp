#include "test_example_functions.h"
 
void AddDocument(SearchServer& search_server, int document_id, const std::string& document, DocumentStatus status, const std::vector<int>& ratings){
    try{
        search_server.AddDocument(document_id, document, status, ratings);
    }catch(const std::invalid_argument& e){
        std::cout << "Can not add a document "s << document_id << ": "s << e.what() << std::endl;
    }
}