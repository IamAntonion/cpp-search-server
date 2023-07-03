#include <algorithm>
#include <cmath>
#include <iostream>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>
#include <optional>
#include <numeric>
#include <deque>
 
using namespace std;
 
constexpr double RELEVANCE_DIFFERENCE = 1e-6;
const int MAX_RESULT_DOCUMENT_COUNT = 5;
 
string ReadLine() {
    string s;
    getline(cin, s);
    return s;
}
 
int ReadLineWithNumber() {
    int result;
    cin >> result;
    ReadLine();
    return result;
}
 
vector<string> SplitIntoWords(const string& text) {
    vector<string> words;
    string word;
    for (const char c : text) {
        if (c == ' ') {
            if (!word.empty()) {
                words.push_back(word);
                word.clear();
            }
        } else {
            word += c;
        }
    }
    if (!word.empty()) {
        words.push_back(word);
    }
 
    return words;
}
 
struct Document {
    Document() = default;
 
    Document(int id, double relevance, int rating)
        : id(id)
        , relevance(relevance)
        , rating(rating) {
    }
 
    int id = 0;
    double relevance = 0.0;
    int rating = 0;
};

ostream& operator<<(ostream& out, const Document& document) {
    out << "{ document_id = "s << document.id 
        << ", relevance = "s << document.relevance 
        << ", rating = "s << document.rating << " }"s;
    return out;
}
 
template <typename StringContainer>
set<string> MakeUniqueNonEmptyStrings(const StringContainer& strings) {
    set<string> non_empty_strings;
    for (const string& str : strings) {
        if (!str.empty()) {
            non_empty_strings.insert(str);
        }
    }
    return non_empty_strings;
}
 
enum class DocumentStatus {
    ACTUAL,
    IRRELEVANT,
    BANNED,
    REMOVED,
};
 
class SearchServer {
public:
    
    template <typename StringContainer>
    explicit SearchServer(const StringContainer& stop_words) : stop_words_(MakeUniqueNonEmptyStrings(stop_words)){
        for (const auto& stop_word : stop_words_) { 
            if (IsValidWord(stop_word) == false) { 
                throw invalid_argument("Some of the stop words contain invalid characters"s); 
            }
        }
    }
 
    explicit SearchServer(const string& stop_words_text) : SearchServer(SplitIntoWords(stop_words_text)){
        
    }
 
    bool AddDocument(int document_id, const string& document, DocumentStatus status,
                     const vector<int>& ratings) {
        if(document_id < 0){
            throw invalid_argument("The document cannot have a negative ID"s);
        }
        if(documents_.count(document_id)){
            throw invalid_argument("ID belongs to an already added document"s);
        }
        if(IsValidWord(document)==false){
            throw invalid_argument("Document text contains invalid characters"s);
        }
        const vector<string> words = SplitIntoWordsNoStop(document);
        const double inv_word_count = 1.0 / words.size();
        for (const string& word : words) {
            word_to_document_freqs_[word][document_id] += inv_word_count;
        }
        document_ids.push_back(document_id);
        documents_.emplace(document_id, DocumentData{ComputeAverageRating(ratings), status});
        return true;
    }
 
    template <typename DocumentPredicate>
    vector<Document> FindTopDocuments(const string& raw_query,
                                      DocumentPredicate document_predicate) const {
        const Query query = ParseQuery(raw_query);
        auto matched_documents = FindAllDocuments(query, document_predicate);
        sort(matched_documents.begin(), matched_documents.end(),
                [](const Document& lhs, const Document& rhs) {
            if (abs(lhs.relevance - rhs.relevance) < RELEVANCE_DIFFERENCE) {
                return lhs.rating > rhs.rating;
            } else {
                return lhs.relevance > rhs.relevance;
            }});
            if (matched_documents.size() > MAX_RESULT_DOCUMENT_COUNT) {
                matched_documents.resize(MAX_RESULT_DOCUMENT_COUNT);
            }
            return matched_documents;
    }
 
    vector<Document> FindTopDocuments(const string& raw_query, DocumentStatus status) const {
        return FindTopDocuments(raw_query, [status](int document_id, DocumentStatus document_status, int rating) {
        return document_status == status;});
    }
 
    vector<Document> FindTopDocuments(const string& raw_query) const {
        return FindTopDocuments(raw_query, DocumentStatus::ACTUAL);
    }
 
    int GetDocumentCount() const {
        return documents_.size();
    }
    
    int GetDocumentId(int index) const {
        return document_ids.at(index);
    }
 
    tuple<vector<string>, DocumentStatus> MatchDocument(const string& raw_query,
                                        int document_id) const {
        const Query query = ParseQuery(raw_query);
        vector<string> matched_words;
        for (const string& word : query.plus_words) {
            if (word_to_document_freqs_.count(word) == 0) {
                continue;
            }
            if (word_to_document_freqs_.at(word).count(document_id)) {
                matched_words.push_back(word);
            }
        }
        for (const string& word : query.minus_words) {
            if (word_to_document_freqs_.count(word) == 0) {
                continue;
            }
            if (word_to_document_freqs_.at(word).count(document_id)) {
                matched_words.clear();
                break;
            }
        }
        auto result = tuple{matched_words, documents_.at(document_id).status};
        return result;
    }
 
private:
    struct DocumentData {
        int rating;
        DocumentStatus status;
    };
    
    vector<int> document_ids;
    set<string> stop_words_;
    map<string, map<int, double>> word_to_document_freqs_;
    map<int, DocumentData> documents_;
 
    bool IsStopWord(const string& word) const {
        return stop_words_.count(word) > 0;
    }
 
    vector<string> SplitIntoWordsNoStop(const string& text) const {
        vector<string> words;
        for (const string& word : SplitIntoWords(text)) {
            if (!IsStopWord(word)) {
                words.push_back(word);
            }
        }
        return words;
    }
 
    static int ComputeAverageRating(const vector<int>& ratings) {
        if (ratings.empty()) return 0;
        return accumulate(ratings.begin(), ratings.end(), 0) / static_cast<int>(ratings.size()); 
    }
 
    struct QueryWord {
        string data;
        bool is_minus;
        bool is_stop;
    };
 
    QueryWord ParseQueryWord(string text) const {
        if(text.empty() == true){
            throw invalid_argument("Search query is empty"s);
        }
        if (text.back() == '-') {
            throw invalid_argument("Missing text after minus sign in search query"s);
        }
        
        bool is_minus = false;
        QueryWord result;
        
        if (text[0] == '-') {
            is_minus = true;
            text = text.substr(1);
        }
        if(IsValidWord(text)==false){
            throw invalid_argument("There are invalid characters in the search query words"s);
        }
        if (text[0] == '-'){
            throw invalid_argument("Presence of more than one minus before stop words"s); 
        }
        result = {text, is_minus, IsStopWord(text)};
        return result;
    }
 
    struct Query {
        set<string> plus_words;
        set<string> minus_words;
    };
 
    Query ParseQuery(const string& text) const {
        Query query;
        for (const string& word : SplitIntoWords(text)) {
            const QueryWord query_word = ParseQueryWord(word);
            if (!query_word.is_stop) {
                if (query_word.is_minus) {
                    query.minus_words.insert(query_word.data);
                } else {
                    query.plus_words.insert(query_word.data);
                }
            }
        }
        return query;
    }
 
    // Existence required
    double ComputeWordInverseDocumentFreq(const string& word) const {
        return log(GetDocumentCount() * 1.0 / word_to_document_freqs_.at(word).size());
    }
 
    template <typename DocumentPredicate>
    vector<Document> FindAllDocuments(const Query& query,
                                      DocumentPredicate document_predicate) const {
        map<int, double> document_to_relevance;
        for (const string& word : query.plus_words) {
            if (word_to_document_freqs_.count(word) == 0) {
                continue;
            }
            const double inverse_document_freq = ComputeWordInverseDocumentFreq(word);
            for (const auto [document_id, term_freq] : word_to_document_freqs_.at(word)) {
                const auto& document_data = documents_.at(document_id);
                if (document_predicate(document_id, document_data.status, document_data.rating)) {
                    document_to_relevance[document_id] += term_freq * inverse_document_freq;
                }
            }
        }
 
        for (const string& word : query.minus_words) {
            if (word_to_document_freqs_.count(word) == 0) {
                continue;
            }
            for (const auto [document_id, _] : word_to_document_freqs_.at(word)) {
                document_to_relevance.erase(document_id);
            }
        }
 
        vector<Document> matched_documents;
        for (const auto [document_id, relevance] : document_to_relevance) {
            matched_documents.push_back(
                {document_id, relevance, documents_.at(document_id).rating});
        }
        return matched_documents;
    }
    
    static bool IsValidWord(const string& word) {
        return none_of(word.begin(), word.end(), [](char c) {
            return c >= '\0' && c <' ';});
    }
};

template <typename Iterator>
class IteratorRange {
public:
    IteratorRange(Iterator begin, Iterator end)
        : first_(begin)
        , last_(end)
        , size_(distance(first_, last_)) {
    }
 
    Iterator begin() const {
        return first_;
    }
 
    Iterator end() const {
        return last_;
    }
 
    size_t size() const {
        return size_;
    }
 
private:
    Iterator first_, last_;
    size_t size_;
};

template <typename Iterator>
ostream& operator<<(ostream& out, const IteratorRange<Iterator>& range) {
    for (Iterator it = range.begin(); it != range.end(); ++it) {
        out << *it;
    }
    return out;
}

template <typename Iterator>
class Paginator {
public:
    Paginator(Iterator begin, Iterator end, size_t page_size) {
        for (size_t left = distance(begin, end); left > 0;) {
            const size_t current_page_size = min(page_size, left);
            const Iterator current_page_end = next(begin, current_page_size);
            pages_.push_back({begin, current_page_end});
 
            left -= current_page_size;
            begin = current_page_end;
        }
    }
 
    auto begin() const {
        return pages_.begin();
    }
 
    auto end() const {
        return pages_.end();
    }
 
    size_t size() const {
        return pages_.size();
    }
 
private:
    vector<IteratorRange<Iterator>> pages_;
};

class RequestQueue {
public:
    explicit RequestQueue(const SearchServer& search_server) : search_request(search_server){}
 
    template <typename DocumentPredicate>
    vector<Document> AddFindRequest(const string& raw_query, DocumentPredicate document_predicate) {
 
        vector<Document> helper = search_request.FindTopDocuments(raw_query, document_predicate);
        
        QueryResult query;
        query.query_result = (helper.empty()==false);
        
        if (!(requests_.size() < min_in_day_)){
            requests_.pop_front();
        } 
        requests_.push_back(query); 
        return helper;
    }
    
    vector<Document> AddFindRequest(const string& raw_query, DocumentStatus status) {
         return AddFindRequest(raw_query, [&status](int document_id, DocumentStatus document_status, int rating) {
            return document_status == status;});
    }
    
    vector<Document> AddFindRequest(const string& raw_query) {
        return AddFindRequest(raw_query, DocumentStatus::ACTUAL);
    }
    
    int GetNoResultRequests() const {
        return count_if(requests_.begin(), requests_.end(), [](QueryResult query) {return query.query_result == 0;});
    }
private:
    struct QueryResult {
        bool query_result;
    };
    const SearchServer& search_request;
    deque<QueryResult> requests_;
    const static int min_in_day_ = 1440;
}; 

// ==================== для примера =========================
template <typename Container>
auto Paginate(const Container& c, size_t page_size) {
    return Paginator(begin(c), end(c), page_size);
}

int main() {
    SearchServer search_server("and with"s);
    search_server.AddDocument(1, "funny pet and nasty rat"s, DocumentStatus::ACTUAL, {7, 2, 7});
    search_server.AddDocument(2, "funny pet with curly hair"s, DocumentStatus::ACTUAL, {1, 2, 3});
    search_server.AddDocument(3, "big cat nasty hair"s, DocumentStatus::ACTUAL, {1, 2, 8});
    search_server.AddDocument(4, "big dog cat Vladislav"s, DocumentStatus::ACTUAL, {1, 3, 2});
    search_server.AddDocument(5, "big dog hamster Borya"s, DocumentStatus::ACTUAL, {1, 1, 1});
    const auto search_results = search_server.FindTopDocuments("curly dog"s);
    int page_size = 2;
    const auto pages = Paginate(search_results, page_size);
    // Выводим найденные документы по страницам
    for (auto page = pages.begin(); page != pages.end(); ++page) {
        cout << *page << endl;
        cout << "Page break"s << endl;
    }
} 
