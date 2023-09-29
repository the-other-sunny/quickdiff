#include <vector>
#include <tuple>
#include <string>
#include <sstream>
#include <iostream>
#include <fstream>
#include <filesystem>
#include <stdio.h>

#include <omp.h>
#include "utf8.h"
#include "difflib.h"

#if defined(_WIN32)
#include <io.h>
#include <fcntl.h>
#endif

namespace utils {
    using namespace std;

    template<class T>
    ostream& write(ostream& os, const T& a);

    template<>
    ostream& write<uint64_t>(ostream& os, const uint64_t& a)
    {
        os.write(reinterpret_cast<const char*>(&a), sizeof a);

        if (os.fail()) {
            throw runtime_error("Failed writing to output stream.");
        }

        return os;
    }

    template<>
    ostream& write<double>(ostream& os, const double& a)
    {
        os.write(reinterpret_cast<const char*>(&a), sizeof a);
        
        if (os.fail()) {
            throw runtime_error("Failed writing to output stream.");
        }

        return os;
    }

    template<>
    ostream& write<string>(ostream& os, const string& a)
    {
        os.write(a.data(), a.size());

        if (os.fail()) {
            throw runtime_error("Failed writing to output stream.");
        }

        return os;
    }

    // TODO: static_assert(std::is_fundamental<T>::value, "T must be a fundamental type");
    template<class T>
    T read_primitive(istream& is);

    template<>
    uint64_t read_primitive(istream& is)
    {
        uint64_t a;
        is.read(reinterpret_cast<char*>(&a), sizeof a);

        if (is.fail()) {
            throw runtime_error("Failed reading from input stream.");
        }
        
        return a;
    }

    template<>
    double read_primitive<double>(istream& is)
    {
        double a;
        is.read(reinterpret_cast<char*>(&a), sizeof a);

        if (is.fail()) {
            throw runtime_error("Failed reading from input stream.");
        }

        return a;
    }

    string read_bytes(istream& is, size_t count)
    {
        string s(count, '\00');
        is.read(s.data(), s.size());

        if (is.fail()) {
            throw runtime_error("Failed reading from input stream.");
        }

        return s;
    }
}

namespace {
    using namespace std;

    template <class RatioType>
    class RatioVect: public vector<RatioType> {
    public:
        using vector<RatioType>::vector; // inherit all vector constructors

        string serialize() const
        {
            ostringstream oss;
            
            utils::write<uint64_t>(oss, this->size());
            for (const auto& ratio : *this)
                utils::write<RatioType>(oss, ratio);
            
            return oss.str();
        }

        static RatioVect deserialize(const string& bin)
        {
            RatioVect ratios;

            istringstream iss(bin);
                    
            size_t count = utils::read_primitive<uint64_t>(iss);    
            while (count--) {
                auto ratio = utils::read_primitive<RatioType>(iss);
                ratios.push_back(ratio);
            }

            return ratios;
        }
    };
}

namespace {
    using namespace std;

    template<class T>
    double ratio(const T& a, const T& b)
    {
        return difflib::MakeSequenceMatcher(a, b).ratio();
    }
    
    using Couple = tuple<size_t, size_t>;
    
    template<class T=string>
    class Order {
    public:
        Order(): Order(vector<T>{}, vector<Couple>{}) {}

        Order(vector<T> contents, vector<Couple> couples)
          : contents{contents}
          , couples{couples}
        {}
        
        Order(string bin): Order(Order::deserialize(bin)) {}
        
        string serialize_contents() const;
        
        string serialize_couples() const
        {
            ostringstream oss;

            utils::write<uint64_t>(oss, size(couples));
            for (const auto& [a, b] : couples) {
                utils::write<uint64_t>(oss, a);
                utils::write<uint64_t>(oss, b);
            }

            return oss.str();
        }
        
        string serialize() const
        {
            return serialize_contents() + serialize_couples();
        }

        RatioVect<double> execute() const
        {
            RatioVect<double> ratios(couples.size());

            #pragma omp parallel for
            for (size_t i = 0; i < couples.size(); ++i) {
                const auto& [i_a, i_b] = couples[i];  
                const auto& a = contents[i_a];
                const auto& b = contents[i_b];
                
                ratios[i] = ratio(a, b);
            }

            return ratios; 
        }
        
        auto get_contents() const
        {
            return contents;
        }
        
        auto get_couples() const 
        {
            return couples;
        }

        static vector<T> deserialize_contents(istream& is);

        static vector<Couple> deserialize_couples(istream& is)
        {
            vector<Couple> couples;        
            size_t n_couples = utils::read_primitive<uint64_t>(is);
            while (n_couples--) {
                const size_t a = utils::read_primitive<uint64_t>(is);
                const size_t b = utils::read_primitive<uint64_t>(is);
                
                couples.push_back({a, b});
            }
            
            return couples;
        }

        static Order deserialize(const string& bin)
        {
            istringstream iss(bin);

            auto contents = Order<T>::deserialize_contents(iss);
            auto couples = Order<T>::deserialize_couples(iss);
            
            return { contents, couples };
        }
    private:
        vector<T> contents;
        vector<Couple> couples;
    };
    
    template<>
    vector<u32string> Order<u32string>::deserialize_contents(istream& iss)
    {
        vector<u32string> contents;
        
        size_t n_contents = utils::read_primitive<uint64_t>(iss);
        while (n_contents--) {
            const size_t n_content = utils::read_primitive<uint64_t>(iss);
            const auto content_bin = utils::read_bytes(iss, n_content);
            const auto content = utf8::utf8to32(content_bin);

            contents.push_back(content);
        }

        return contents;
    }

    template<>
    string Order<u32string>::serialize_contents() const
    {
        ostringstream oss;

        utils::write<uint64_t>(oss, size(contents));
        for (const auto& content : contents) {
            const string content_bin = utf8::utf32to8(content);

            utils::write<uint64_t>(oss, size(content_bin));
            utils::write<string>(oss, content_bin);
        }

        return oss.str();
    }
}

namespace {
    using namespace std;
    
    string read_from_file(const filesystem::path& path) {
        if (!filesystem::exists(path)) {
            throw runtime_error("File doesn't exist.");
        }

        ifstream input_file_stream(path, ios::binary);

        istreambuf_iterator<char> file_stream_it(input_file_stream);
        istreambuf_iterator<char> end_of_stream_it;
        string file_content(file_stream_it, end_of_stream_it);

        return file_content;
    }

    void write_to_file(const filesystem::path& path, const string& s) {
        if (!filesystem::exists(path)) {
            throw runtime_error("File doesn't exist.");
        }
        
        ofstream ofs(path, ios::binary);
        utils::write(ofs, s); 
    }

    string read_from_stdin()
    {
#if defined(_WIN32)
        // `stdin` is by default in text mode.
        // On Unix systems, there's no difference between the two.
        // On Windows the modes are different an we shall reopen the stream.
        if (_setmode(_fileno(stdin), _O_BINARY) == -1) {
            throw runtime_error("Unable to set stdin file mode to binary.");
        }
#endif
        
        string s;
 
        char c;
        while (cin.get(c)) {
            s.push_back(c);
        }

#if defined(_WIN32)
        if (_setmode(_fileno(stdin), _O_TEXT) == -1) {
            throw runtime_error("Unable to set stdin file mode to text.");
        }
#endif

        return s;
    }

    void write_to_stdout(const string& s)
    {
#if defined(_WIN32)
        // `stdin` is by default in text mode.
        // On Unix systems, there's no difference between the two.
        // On Windows the modes are different an we shall reopen the stream.
        if (_setmode(_fileno(stdout), _O_BINARY) == -1) {
            throw runtime_error("Unable to set stdout file mode to binary.");
        }
#endif

        utils::write(cout, s);

#if defined(_WIN32)
        if (_setmode(_fileno(stdout), _O_TEXT) == -1) {
            throw runtime_error("Unable to set stdout file mode to text.");
        }
#endif
    }
}

// Debug ===================================

#include <cassert>
#include <chrono>

void __general_test__() {
    using namespace std::chrono;

    auto serialized_order = read_from_file("./.tmp/test/order.bin");
    auto order = Order<u32string>::deserialize(serialized_order);

    assert(serialized_order == order.serialize());

    auto t1 = high_resolution_clock::now();
    auto ratios = order.execute();
    auto t2 = high_resolution_clock::now();

    auto serialized_expected_ratios = read_from_file("./.tmp/test/ratios_py.bin");
    auto expected_ratios = RatioVect<double>::deserialize(serialized_expected_ratios);

    assert(ratios == expected_ratios);
    
    auto duration_ns = duration_cast<nanoseconds>(t2 - t1).count();
    auto duration = duration_ns / 1e9;
    std::cout << duration << " s\n";    
}

// =========================================

void stdio_order_execution() {
    auto serialized_order = read_from_stdin();
    auto order = Order<u32string>::deserialize(serialized_order);
    auto ratios = order.execute();
    write_to_stdout(ratios.serialize());
}

int main() {
    stdio_order_execution();
    // __general_test__();
}
