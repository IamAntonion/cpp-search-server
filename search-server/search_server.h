#pragma once
#include <tuple>
#include <algorithm>
#include <cmath>
#include <iostream>
#include <map>
#include <set>
#include <stdexcept>
#include <execution>
#include <string>
#include <vector>
#include <type_traits>
#include <random>
#include <future> 

#include "read_input_functions.h"
#include "string_processing.h"
#include "document.h"
#include "log_duration.h" 
#include "concurrent_map.h"
 
using namespace std::string_literals;
using MatchTuple = std::tuple<std::vector<std::string_view>, DocumentStatus>;
 
class SearchServer {
public:
    template <typename StringContainer>
    SearchServer(const StringContainer& stop_words);
    SearchServer(const std::string& stop_words_text) : SearchServer(SplitIntoWords(std::string_view(stop_words_text))){}
    SearchServer(std::string_view& stop_words_text) : SearchServer(SplitIntoWords(stop_words_text)){}
    SearchServer() = default;
    
    void AddDocument(int document_id, std::string_view document, DocumentStatus status, const std::vector<int>& ratings);
    
    int GetDocumentCount() const;
    
    std::set<int> ::const_iterator begin() const;
    std::set<int> ::const_iterator end() const;
    
    const std::map<std::string_view, double>& GetWordFrequencies(int document_id) const;
    
    void RemoveDocument(int document_id);
    void RemoveDocument(const std::execution::sequenced_policy&, int document_id);
    void RemoveDocument(const std::execution::parallel_policy&, int document_id);
    
    MatchTuple MatchDocument(std::string_view raw_query, int document_id) const;
    MatchTuple MatchDocument(std::execution::sequenced_policy policy, std::string_view raw_query,
                                                                       int document_id) const;
    MatchTuple MatchDocument(std::execution::parallel_policy policy, std::string_view raw_query,
                                                                       const int& document_id) const;
    template <typename DocumentPredicate>
    std::vector<Document> FindTopDocuments(std::string_view raw_query, 
                                           DocumentPredicate document_predicate) const {
        return FindTopDocuments(std::execution::seq,
                                raw_query, 
                                document_predicate);
    }

    template <typename DocumentPredicate, typename Policy>
    std::vector<Document> FindTopDocuments(const Policy& policy,
                                                         std::string_view raw_query, 
                                                         DocumentPredicate document_predicate) const {
        const auto query = ParseQuery(raw_query, true);
        auto matched_documents = FindAllDocuments(policy,
                                                  query, 
                                                  document_predicate);
    
        sort(policy,
             matched_documents.begin(), 
             matched_documents.end(), 
             [this](const Document& lhs, const Document& rhs) {
                 if (std::abs(lhs.relevance - rhs.relevance) < EPSILON) {
                     return lhs.rating > rhs.rating;
                 } else {
                     return lhs.relevance > rhs.relevance;
                 }
             });
    
        if (matched_documents.size() > MAX_RESULT_DOCUMENT_COUNT) {
            matched_documents.resize(MAX_RESULT_DOCUMENT_COUNT);
        }
    
        return matched_documents;
    }

    std::vector<Document> FindTopDocuments(std::string_view raw_query, 
                                           DocumentStatus status) const {
        return FindTopDocuments(std::execution::seq, raw_query, status);
    }

    template <typename Policy>
    std::vector<Document> FindTopDocuments(const Policy& policy,
                                                         std::string_view raw_query, 
                                                         DocumentStatus status) const {
        return FindTopDocuments(policy,
                                raw_query,
                                [status](int document_id,
                                         DocumentStatus document_status,
                                         int rating) {
                                            return document_status == status;
            });
    }

    std::vector<Document> FindTopDocuments(std::string_view raw_query) const {
        return FindTopDocuments(std::execution::seq, 
                                raw_query,
                                DocumentStatus::ACTUAL);
    }

    template <typename Policy>
    std::vector<Document> FindTopDocuments(const Policy& policy,
                                           std::string_view raw_query) const {
        return FindTopDocuments(policy,
                                raw_query,
                                DocumentStatus::ACTUAL);
    }
 
private:
    struct DocumentData {
        std::string data_string_;
        int rating;
        DocumentStatus status;
    };
    
    const double EPSILON = 1e-6;
    const int MAX_RESULT_DOCUMENT_COUNT = 5;
    
    const std::set<std::string, std::less<>> stop_words_;
    std::map<std::string_view, std::map<int, double>> word_to_document_freqs_;
    std::map<int, DocumentData> documents_;
    std::set<int> document_ids_;
    std::map<int, std::map<std::string_view, double>> ids_of_docs_to_word_freqs_;
 
    bool IsStopWord(std::string_view word) const;
    static bool IsValidWord(std::string_view word);
    
    std::vector<std::string_view> SplitIntoWordsNoStop(std::string_view text) const;
    
    static int ComputeAverageRating(const std::vector<int>& ratings);
 
    struct QueryWord {
        std::string_view data;
        bool is_minus;
        bool is_stop;
    };
 
    QueryWord ParseQueryWord(std::string_view& text) const;
 
    struct Query {
        std::vector<std::string_view> plus_words;
        std::vector<std::string_view> minus_words;
    };
 
    Query ParseQuery(std::string_view& text, bool is_not_sort) const;
    double ComputeWordInverseDocumentFreq(std::string_view& word) const;
 
    template <typename DocumentPredicate>
    std::vector<Document> FindAllDocuments(const Query& query, 
                                           DocumentPredicate document_predicate) const;
    template <typename DocumentPredicate>
    std::vector<Document> FindAllDocuments(const std::execution::sequenced_policy&,
                                           const Query& query, 
                                           DocumentPredicate document_predicate) const;
    template <typename DocumentPredicate>
    std::vector<Document> FindAllDocuments(const std::execution::parallel_policy&,
                                           const Query& query, 
                                           DocumentPredicate document_predicate) const;
    
};
 
template <typename StringContainer>
SearchServer::SearchServer(const StringContainer& stop_words) : stop_words_(MakeUniqueNonEmptyStrings(stop_words)){
    if (!all_of(stop_words_.begin(), stop_words_.end(), IsValidWord)) {
        throw std::invalid_argument("Some of stop words are invalid"s);
    }
}
 
template <typename DocumentPredicate>
std::vector<Document> SearchServer::FindAllDocuments(const Query& query, 
                                                     DocumentPredicate document_predicate) const {
    return FindAllDocuments(std::execution::seq,
                            query, 
                            document_predicate);
}

template <typename DocumentPredicate>
std::vector<Document> SearchServer::FindAllDocuments(const std::execution::sequenced_policy&,
                                                     const Query& query, 
                                                     DocumentPredicate document_predicate) const {
    std::map<int, double> document_to_relevance;
    
    for (std::string_view word : query.plus_words) {
            if (word_to_document_freqs_.count(word) == 0) {
                continue;
            }
            const double inverse_document_freq = ComputeWordInverseDocumentFreq(word);
            for (const auto [document_id, term_freq] : word_to_document_freqs_.at(word)) {
                const auto& document_data = documents_.at(document_id);
                if (document_predicate(document_id, 
                                       document_data.status, 
                                       document_data.rating)) {
                    document_to_relevance[document_id] += term_freq * inverse_document_freq;
                }
            }
        }
 
        for (const auto word : query.minus_words) {
            if (word_to_document_freqs_.count(word) == 0) {
                continue;
            }
            for (const auto [document_id, _] : word_to_document_freqs_.at(word)) {
                document_to_relevance.erase(document_id);
            }
        }
 
        std::vector<Document> matched_documents;
        for (const auto [document_id, relevance] : document_to_relevance) {
            matched_documents.push_back({document_id, 
                                         relevance, 
                                         documents_.at(document_id).rating});
        }
 
        return matched_documents;
    }
 
template <typename DocumentPredicate>
std::vector<Document> SearchServer::FindAllDocuments(const std::execution::parallel_policy&,
                                                     const Query& query, 
                                                     DocumentPredicate document_predicate) const {
    const int BUCKET_COUNT = 101;
    ConcurrentMap<int, double> document_to_relevance(BUCKET_COUNT);
 
    const auto plus_func = [this, 
                       &document_predicate, 
                       &document_to_relevance] (std::string_view word) {
                           if (word_to_document_freqs_.count(word) == 0) {//
                                return;
                           }
                           const double inverse_document_freq = ComputeWordInverseDocumentFreq(word);
                           for (const auto& [document_id, term_freq] : word_to_document_freqs_.at(word)) {
                               const auto& document_data = documents_.at(document_id);//
                               if (document_predicate(document_id, 
                                                      document_data.status, 
                                                      document_data.rating)) {
                                   document_to_relevance[document_id].ref_to_value += term_freq * inverse_document_freq;
                                }
                            }
                        };
 
    for_each(std::execution::par, 
             query.plus_words.begin(), 
             query.plus_words.end(), 
             plus_func);
 
    const auto minus_erase_func = [&](std::string_view word) {
        if (word_to_document_freqs_.count(word) == 0) {
            return;
        }
        for (const auto& [document_id, _] : word_to_document_freqs_.at(word)) {
            document_to_relevance.Erase(document_id);
        }
    };
 
    for_each(std::execution::par, 
             query.minus_words.begin(), 
             query.minus_words.end(), 
             minus_erase_func);
    
    const auto& document_to_relevance_bom = document_to_relevance.BuildOrdinaryMap();
    
    std::vector<Document> matched_documents;
    for (const auto& [document_id, relevance] : document_to_relevance_bom) {
        matched_documents.push_back({document_id, 
                                     relevance, 
                                     documents_.at(document_id).rating });
    }
    
    return matched_documents;
}