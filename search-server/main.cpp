/* Подставьте вашу реализацию класса SearchServer сюда */
#include <cassert>
#include <algorithm>
#include <cmath>
#include <iostream>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>
#include <numeric>

using namespace std;

const int MAX_RESULT_DOCUMENT_COUNT = 5;
const double RELEVANCE_DIFFERENCE = 1e-6;

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
    int id;
    double relevance;
    int rating;
};

enum class DocumentStatus {
    ACTUAL,
    IRRELEVANT,
    BANNED,
    REMOVED,
};

class SearchServer {
public:
    void SetStopWords(const string& text) {
        for (const string& word : SplitIntoWords(text)) {
            stop_words_.insert(word);
        }
    }

    void AddDocument(int document_id, const string& document, DocumentStatus status,
                     const vector<int>& ratings) {
        const vector<string> words = SplitIntoWordsNoStop(document);
        const double inv_word_count = 1.0 / words.size();
        for (const string& word : words) {
            word_to_document_freqs_[word][document_id] += inv_word_count;
        }
        documents_.emplace(document_id, DocumentData{ComputeAverageRating(ratings), status});
    }
    
    template <typename Split>
    vector<Document> FindTopDocuments(const string& raw_query, const Split split) const {
        const Query query = ParseQuery(raw_query);
        auto matched_documents = FindAllDocuments(query, split);

        sort(matched_documents.begin(), matched_documents.end(),
             [](const Document& lhs, const Document& rhs) {
                 if (abs(lhs.relevance - rhs.relevance) < RELEVANCE_DIFFERENCE) {
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
    
    vector<Document> FindTopDocuments(const string& raw_query, DocumentStatus status) const {
        auto result = FindTopDocuments(raw_query, [status] ([[maybe_unused]] int document_id, const DocumentStatus& doc_status, [[maybe_unused]] int rating) {
            return status == doc_status;
        });
        return result;
    }
    
    vector<Document> FindTopDocuments(const string& raw_query) const {
        auto result = FindTopDocuments(raw_query, DocumentStatus::ACTUAL);
        return result;
    }

    int GetDocumentCount() const {
        return documents_.size();
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
        return {matched_words, documents_.at(document_id).status};
    }

private:
    struct DocumentData {
        int rating;
        DocumentStatus status;
    };

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
        bool is_minus = false;
        // Word shouldn't be empty
        if (text[0] == '-') {
            is_minus = true;
            text = text.substr(1);
        }
        return {text, is_minus, IsStopWord(text)};
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

    template <typename Split>
    vector<Document> FindAllDocuments(const Query& query, const Split split) const {
        map<int, double> document_to_relevance;
        for (const string& word : query.plus_words) {
            if (word_to_document_freqs_.count(word) == 0) {
                continue;
            }
            const double inverse_document_freq = ComputeWordInverseDocumentFreq(word);
            for (const auto [document_id, term_freq] : word_to_document_freqs_.at(word)) {
                const auto& document_data = documents_.at(document_id);
                if (split(document_id, document_data.status, document_data.rating)) {
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
};
/*
   Подставьте сюда вашу реализацию макросов
   ASSERT, ASSERT_EQUAL, ASSERT_EQUAL_HINT, ASSERT_HINT и RUN_TEST
*/
template <typename T>
void AssertImpl(const T& expr, const string& expr_str, const string& file, const string& func, const int& line, const string& hint){
    if(!expr){
        cout << boolalpha;
        cout << file << "("s << line << "): "s << func << ": "s;
        cout << "ASSERT("s << expr_str << ") failed."s;
        if (!hint.empty()){
            cout << " Hint: "s << hint;
        }
        cout << endl;
        abort();        
    } 
}

#define ASSERT(expr) AssertImpl((expr), #expr, __FILE__, __FUNCTION__, __LINE__, ""s)
#define ASSERT_HINT(expr, hint) AssertImpl((expr), #expr, __FILE__, __FUNCTION__, __LINE__, (hint))

template <typename T, typename U>
void AssertEqualImpl(const T& t, const U& u, const string& t_str, const string& u_str, const string& file, const string& func, const int& line, const string& hint){
    if (t != u) {
        cout << boolalpha;
        cout << file << "("s << line << "): "s << func << ": "s;
        cout << "ASSERT_EQUAL("s << t_str << ", "s << u_str << ") failed: "s;
        cout << t << " != "s << u << "."s;
        if (!hint.empty()) {
            cerr << " Hint: "s << hint;
        }
        cout << endl;
        abort();
    }
}

#define ASSERT_EQUAL(a, b) AssertEqualImpl((a), (b), #a, #b, __FILE__, __FUNCTION__, __LINE__, ""s)
#define ASSERT_EQUAL_HINT(a, b, hint) AssertEqualImpl((a), (b), #a, #b, __FILE__, __FUNCTION__, __LINE__, (hint))

template <typename T>
void RunTestImpl (const T& t, const string& t_str) {
    t();
    cerr << t_str << " OK"s << endl;
}

#define RUN_TEST(func) RunTestImpl((func), #func)

// -------- Начало модульных тестов поисковой системы ----------

// Тест проверяет, что поисковая система исключает стоп-слова при добавлении документов
void TestExcludeStopWordsFromAddedDocumentContent() {
    const int doc_id = 42;
    const string content = "cat in the city"s;
    const vector<int> ratings = {1, 2, 3};
    {
        SearchServer server;
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        const auto found_docs = server.FindTopDocuments("in"s);
        ASSERT_EQUAL(found_docs.size(), 1u);
        const Document& doc0 = found_docs[0];
        ASSERT_EQUAL(doc0.id, doc_id);
    }

    {
        SearchServer server;
        server.SetStopWords("in the"s);
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        ASSERT_HINT(server.FindTopDocuments("in"s).empty(),
                    "Stop words must be excluded from documents"s);
    }
}

/*
Разместите код остальных тестов здесь
*/
// Проверка на добавление документов и проверка по поисковому запросу
void TestMatchQuery(){
    SearchServer search_server;
    int document_count = search_server.GetDocumentCount();
    ASSERT_EQUAL(document_count, 0);
    
    search_server.AddDocument(1, "dog in the city"s, DocumentStatus::ACTUAL, {2, 4, 6, 1});
    search_server.AddDocument(2, "red pig in the city"s, DocumentStatus::BANNED, {3, 1, 5, -1});
    search_server.AddDocument(3, "gray cat in the village"s, DocumentStatus::ACTUAL, {3, 1, 5, -1});
    
    document_count = search_server.GetDocumentCount();
    ASSERT_EQUAL(document_count, 3);
 
    vector<Document> document = search_server.FindTopDocuments("gray cat village"s);
    ASSERT_EQUAL(document.size(), 1);
    ASSERT_EQUAL(document[0].id, 3);
}

// Проверка на наличие минус слов
void TestMinusWords() {
    SearchServer search_server;
    int document_count = search_server.GetDocumentCount();
    ASSERT_EQUAL(document_count, 0);
    
    search_server.AddDocument(1, "tabby cat with big eyes"s, DocumentStatus::ACTUAL, {-1, -2, -5, -7, 11});
    search_server.AddDocument(2, "small dog and tabby cat"s, DocumentStatus::ACTUAL, {-1, -2, -5, -7, 11});
    
    document_count = search_server.GetDocumentCount();
    ASSERT_EQUAL(document_count, 2);
 
    vector<Document> found_documents = search_server.FindTopDocuments("-small -dog walks tabby cat"s);
    ASSERT_EQUAL(found_documents.size(), 1);
        
    ASSERT_EQUAL(found_documents[0].id, 1);
}

// Матчинг документа по поисковому запросу. Возвращает все слова из поискового запроса, если есть минус слово возварает пустой список
void TestMatchWords() {
    SearchServer search_server;
    
    search_server.AddDocument(1, "tabby cat with -big eyes"s, DocumentStatus::ACTUAL, {13});
    const auto [matched_words1, status1] = search_server.MatchDocument("Anton dog want icecream"s, 1);
    ASSERT(matched_words1.empty());
    
    search_server.SetStopWords("and"s);
    search_server.AddDocument(2, "small dog and tabby cat"s, DocumentStatus::ACTUAL, {10});
    const auto [matched_words2, status2] = search_server.MatchDocument("small dog"s, 2);
    ASSERT_EQUAL(matched_words2.size(), 2);
    ASSERT_EQUAL(matched_words2[0], "dog"s);
    ASSERT_EQUAL(matched_words2[1], "small"s);
    ASSERT_EQUAL(static_cast<int>(status2), static_cast<int>(DocumentStatus::ACTUAL));
    
    search_server.AddDocument(3, "tabby cat with small dog"s, DocumentStatus::IRRELEVANT, {13});
    const auto [matched_words3, status3] = search_server.MatchDocument("cat"s, 3);
    ASSERT_EQUAL(matched_words3.size(), 1);
    ASSERT_EQUAL(matched_words3[0], "cat"s);
    ASSERT_EQUAL(static_cast<int>(status3), static_cast<int>(DocumentStatus::IRRELEVANT));
    
}

// Сортировка по релевантности (в порядке убывания)
void TestSortByRelevance() {
    SearchServer search_server;
    
    search_server.AddDocument(1, "dog in the city NY"s, DocumentStatus::ACTUAL, {1, 2, 3});
    search_server.AddDocument(2, "cat in the city NY"s, DocumentStatus::ACTUAL, {2, 3, 4});
    search_server.AddDocument(3, "cat in the city Moscow"s, DocumentStatus::ACTUAL, {3, 4, 5});
    search_server.AddDocument(4, "dog in the city Moscow"s, DocumentStatus::ACTUAL, {4, 5, 6});
    search_server.AddDocument(5, "rabbit in the city NY"s, DocumentStatus::ACTUAL, {5, 6, 7});
  
    string query = "city Moscow dog"s;
 
    const auto found_documents = search_server.FindTopDocuments(query);
    ASSERT_EQUAL(found_documents.size(), 5);
    ASSERT_EQUAL(round(found_documents[0].relevance * 100) / 100, round(log(3 / 1) * 1 / 3 * 100) / 100);
    ASSERT_EQUAL(round(found_documents[1].relevance * 100) / 100, round(log(3 / 1) * 1 / 6 * 100) / 100);
    ASSERT_EQUAL(round(found_documents[2].relevance * 100) / 100, round(log(3 / 1) * 1 / 6 * 100) / 100);
}

// Вычисление среднего знаения рейтинга документа
void TestRatings() {
    SearchServer search_server;
    search_server.AddDocument(1, "this person has pint eyes"s, DocumentStatus::ACTUAL, {1, 1, 1});
    search_server.AddDocument(2, "this person has green eyes"s, DocumentStatus::ACTUAL, {2, 2, 2});
    search_server.AddDocument(3, "this person has blue eyes"s, DocumentStatus::ACTUAL, {4, 4, 4, 4});
    const auto found_docs = search_server.FindTopDocuments("eyes"s);
    ASSERT_EQUAL(found_docs.size(), 3);
    // SUM(rating)/size
    ASSERT_EQUAL(found_docs[0].rating, (4 + 4 + 4 + 4) / 4);
    ASSERT_EQUAL(found_docs[1].rating, (2 + 2 + 2) / 3);
    ASSERT_EQUAL(found_docs[2].rating, (1 + 1 + 1) / 3);
}

// Фильтрация с помощью предикта
void TestPredicate() {
    SearchServer search_server;
    search_server.AddDocument(1, "tabby cat with big eyes"s, DocumentStatus::REMOVED, {13});
    search_server.AddDocument(2, "small dog and tabby cat"s, DocumentStatus::ACTUAL, {20});
    vector<Document> found_document = search_server.FindTopDocuments("cat"s, []([[maybe_unused]] int document_id, 
                                                                               DocumentStatus status, 
                                                                               [[maybe_unused]] int rating) {
        return status == DocumentStatus::ACTUAL;});
    ASSERT_EQUAL(found_document.size(), 1);
    ASSERT_EQUAL(found_document[0].id, 2);
}

// Поиск по статусу
void TestStatus() {    
    SearchServer search_server;
    search_server.AddDocument(1, "this person has pink eyes"s, DocumentStatus::ACTUAL, {1, 2, 3, 4, 5});
    search_server.AddDocument(2, "pink teeth with blue braces"s, DocumentStatus::BANNED, {1, 2, 3, 4, 5});
    search_server.AddDocument(3, "pink mouse with red book"s, DocumentStatus::IRRELEVANT, {1, 2, 3, 4, 5});
    search_server.AddDocument(4, "pink pen with blue braces"s, DocumentStatus::REMOVED, {1, 2, 3, 4, 5});
 
    vector<Document> documents_actual = search_server.FindTopDocuments("pink eys"s, DocumentStatus::ACTUAL);
    ASSERT_EQUAL(documents_actual.size(), 1);
    ASSERT_EQUAL(documents_actual[0].id, 1);
    
    vector<Document> documents_banned = search_server.FindTopDocuments("pink eys"s, DocumentStatus::BANNED);
    ASSERT_EQUAL(documents_banned.size(), 1);
    ASSERT_EQUAL(documents_banned[0].id, 2);
}

void TestAccurateRelevance(){
    SearchServer search_server;
    search_server.AddDocument(1, "tabby cat with big eyes"s, DocumentStatus::ACTUAL, {1, 7, 13});
    
    search_server.AddDocument(2, "small dog and tabby bird"s, DocumentStatus::ACTUAL, {2, 4, 10});
 
    vector<Document> document = search_server.FindTopDocuments("cat"s);
    ASSERT_EQUAL(document.size(), 1);
    ASSERT_EQUAL(round(document[0].relevance * 100) / 100, round(log(2 / 1) * 1 / 5 * 100) / 100);
}

// Функция TestSearchServer является точкой входа для запуска тестов
void TestSearchServer() {
    RUN_TEST(TestExcludeStopWordsFromAddedDocumentContent);
    RUN_TEST(TestMatchQuery);
    RUN_TEST(TestMinusWords);
    RUN_TEST(TestMatchWords);
    RUN_TEST(TestSortByRelevance);
    RUN_TEST(TestRatings);
    RUN_TEST(TestPredicate);
    RUN_TEST(TestStatus);
    RUN_TEST(TestAccurateRelevance);
    // Не забудьте вызывать остальные тесты здесь
}

// --------- Окончание модульных тестов поисковой системы -----------

int main() {
    TestSearchServer();
    // Если вы видите эту строку, значит все тесты прошли успешно
    cout << "Search server testing finished"s << endl;
}
