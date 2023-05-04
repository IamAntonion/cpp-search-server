#include <algorithm>
#include <iostream>
#include <set>
#include <string>
#include <utility>
#include <vector>
#include <map>
#include <cmath>

using namespace std;

//максимальное количество документов на выходе
const int MAX_RESULT_DOCUMENT_COUNT = 5;

//чтение строки
string ReadLine() {
    string s;
    getline(cin, s);
    return s;
}

//чтение строк с пробелами
int ReadLineWithNumber() {
    int result = 0;
    cin >> result;
    ReadLine();
    return result;
}

// SplitIntoWords разбивает строку text на слова и возвращает их в виде вектора
// Слово - последовательность непробельных символов,
// разделённых одним или более пробелов.
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
};

class SearchServer {
public:
    //SetStopWords добавляет в контейнер типа данных string вычеркнутые слова
    void SetStopWords(const string& text) {
        for (const string& word : SplitIntoWords(text)) {
            stop_words_.insert(word);
        }
    }
    
    //
    void AddDocument(int document_id, const string& document) {
       
        //увеличиваем число найденных документов
        ++documents_count_;
        
        const vector<string> words = SplitIntoWordsNoStop(document);
        
        //tf="повтор слов"/"количество документов"
        const double word_count = 1.0/words.size();
        
        for(const string& word : words){
            word_to_document_freqs_[word][document_id] += word_count;
        }
    }

    vector<Document> FindTopDocuments(const string& raw_query) const {
        const Query query_words = ParseQuery(raw_query);
        vector<Document> matched_documents = FindAllDocuments(query_words);

        //сортировка по
        sort(matched_documents.begin(), matched_documents.end(),
             [](const Document& lhs, const Document& rhs) {
                 return lhs.relevance > rhs.relevance;
             });
        if (matched_documents.size() > MAX_RESULT_DOCUMENT_COUNT) {
            matched_documents.resize(MAX_RESULT_DOCUMENT_COUNT);
        }
        return matched_documents;
    }

private:
    struct Query {
        set<string> plus_word;
        set<string> minus_word;
    };

    map<string, map<int, double>> word_to_document_freqs_;
    set<string> stop_words_;
    int documents_count_ = 0;

    //если стоп слово есть в контейнере выводит 1
    bool IsStopWord(const string& word) const {
        return stop_words_.count(word) > 0;
    }

    //провека на наличие стоп слов 
    vector<string> SplitIntoWordsNoStop(const string& text) const {
        vector<string> words;
        for (const string& word : SplitIntoWords(text)) {
            if (!IsStopWord(word)) {
                words.push_back(word);
            }
        }
        return words;
    }

    //парсинг слов на наличие стоп слов и запрещенных слов(-)
    Query ParseQuery(const string& text) const {
        Query query;
        for (const string& word : SplitIntoWordsNoStop(text)) {
            if (word[0] == '-') {
                query.minus_word.insert(word.substr(1));
            } else {
                query.plus_word.insert(word);
            }
        }
        return query;
    }

    //нахождение plus_word, minus_word, так же нахождение idf
    vector<Document> FindAllDocuments(const Query query_words) const {
        
        map<int, double> document_to_relevance;
        
        //проверка через plus_word, нахождение idf для каждого word
        for (const auto& word : query_words.plus_word) {
            if(word_to_document_freqs_.count(word) == 0){
                continue;
            }
            double idf = CalculateIDF(word);
            for(const auto& [document_id, tf] : word_to_document_freqs_.at(word)){
                document_to_relevance[document_id] += tf*idf;
            }
        }
        
        //проверка через minus_word
        for (const auto& word : query_words.minus_word){
            if(word_to_document_freqs_.count(word) == 0){
                continue;
            }
            for(const auto& [document_id, var] : word_to_document_freqs_.at(word)){
                document_to_relevance.erase(document_id);
            }
        }
        
        vector<Document> matched_documents;
        
        for (auto [document_id, relevance] : document_to_relevance) {
            matched_documents.push_back({document_id, relevance});
        }
        
        return matched_documents;
    }
 
    //idf=log("количество документов"*1.0/"количество всех документов")
    double CalculateIDF(const string& word) const{
        double result;
        result = log(documents_count_*1.0/word_to_document_freqs_.at(word).size());
        return result;
    }
};

//создаем класс типа SearchServer
SearchServer CreateSearchServer() {
    SearchServer search_server;
    search_server.SetStopWords(ReadLine());

    const int document_count = ReadLineWithNumber();
    for (int document_id = 0; document_id < document_count; ++document_id) {
        search_server.AddDocument(document_id, ReadLine());
    }

    return search_server;
}

int main() {
    const SearchServer search_server = CreateSearchServer();

    const string query = ReadLine();
    for (const auto& [document_id, relevance] : search_server.FindTopDocuments(query)) {
        cout << "{ document_id = "s << document_id << ", "
             << "relevance = "s << relevance << " }"s << endl;
    }
}
