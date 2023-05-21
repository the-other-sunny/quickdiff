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

namespace util {
    using namespace std;

    template<class T>
    ostream& write(ostream& os, const T& a);

    template<>
    ostream& write<uint64_t>(ostream& os, const uint64_t& a)
    {
        return os.write(reinterpret_cast<const char*>(&a), sizeof a);
    }

    template<>
    ostream& write<double>(ostream& os, const double& a)
    {
        return os.write(reinterpret_cast<const char*>(&a), sizeof a);
    }

    template<>
    ostream& write<string>(ostream& os, const string& a)
    {
        return os.write(a.data(), a.size());
    }

    // TODO: static_assert(std::is_fundamental<T>::value, "T must be a fundamental type");
    template<class T>
    T read_primitive(istream& is);

    template<>
    uint64_t read_primitive(istream& is)
    {
        uint64_t a;
        is.read(reinterpret_cast<char*>(&a), sizeof a);

        return a;
    }

    template<>
    double read_primitive<double>(istream& is)
    {
        double a;
        is.read(reinterpret_cast<char*>(&a), sizeof a);

        return a;
    }

    string read_bytes(istream& is, size_t count)
    {
        string s(count, '\00');
        is.read(s.data(), s.size());

        return s;
    }
}

namespace {
    using namespace std;

    template <class Ratio>
    class RatioVect: public vector<Ratio> {
    public:
        using vector<Ratio>::vector; // inherit all vector constructors

        string serialize() const
        {
            ostringstream oss;
            
            util::write<uint64_t>(oss, this->size());
            for (const auto& ratio : *this)
                util::write<Ratio>(oss, ratio);
            
            return oss.str();
        }

        static RatioVect deserialize(const string& bin)
        {
            RatioVect ratios;

            istringstream iss(bin);
                    
            size_t count = util::read_primitive<uint64_t>(iss);    
            while (count--) {
                auto ratio = util::read_primitive<Ratio>(iss);
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

            util::write<uint64_t>(oss, size(couples));
            for (const auto& [a, b] : couples) {
                util::write<uint64_t>(oss, a);
                util::write<uint64_t>(oss, b);
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
            size_t n_couples = util::read_primitive<uint64_t>(is);
            while (n_couples--) {
                const size_t a = util::read_primitive<uint64_t>(is);
                const size_t b = util::read_primitive<uint64_t>(is);
                
                couples.push_back({a, b});
            }
            
            return couples;
        }

        static Order deserialize(const string& bin)
        {
            istringstream iss(bin);
            
            return {
                Order::deserialize_contents(iss),
                Order::deserialize_couples(iss)
            };
        }
    private:
        vector<T> contents;
        vector<Couple> couples;
    };
    
    template<>
    vector<u32string> Order<u32string>::deserialize_contents(istream& iss)
    {
        vector<u32string> contents;
        
        size_t n_contents = util::read_primitive<uint64_t>(iss);
        while (n_contents--) {
            const size_t n_content = util::read_primitive<uint64_t>(iss);
            const auto content_bin = util::read_bytes(iss, n_content);
            const auto content = utf8::utf8to32(content_bin);

            contents.push_back(content);
        }

        return contents;
    }

    template<>
    string Order<u32string>::serialize_contents() const
    {
        ostringstream oss;

        util::write<uint64_t>(oss, size(contents));
        for (const auto& content : contents) {
            const string content_bin = utf8::utf32to8(content);

            util::write<uint64_t>(oss, size(content_bin));
            util::write<string>(oss, content_bin);
        }

        return oss.str();
    }
}

namespace {
    using namespace std;

    string read_all_file(const filesystem::path& path) {
        ostringstream buf;

        ifstream ifs(path, ios::binary);
        buf << ifs.rdbuf();

        return buf.str();
    }

    void write_to_file(const filesystem::path& path, const string& s) {
        ofstream ofs(path, ios::binary);
        ofs.write(s.data(), s.size());  
    }

    string read_all_stdin()
    {
        // `stdin` is by default in text mode.
        // On Unix systems, there's no difference between the two.
        // On Windows the modes are different an we shall reopen the stream.
        // freopen(nullptr, "rb", stdin);
    
        string s;
 
        char c;
        while(cin.get(c))
            s.push_back(c);
                
        return s;
    }

    void write_to_stdout(const string& s)
    {
        // `stdin` is by default in text mode.
        // On Unix systems, there's no difference between the two.
        // On Windows the modes are different an we shall reopen the stream.
        // freopen(nullptr, "wb", stdout);

        util::write(cout, s); 
    }
}

// Debug ===================================

#include <cassert>
#include <chrono>

void _general_test() {
    using namespace std::chrono;

    auto serialized_order = read_all_file("./tmp/order.bin");
    auto order = Order<u32string>::deserialize(serialized_order);

    assert(serialized_order == order.serialize());

    auto t1 = high_resolution_clock::now();
    auto ratios = order.execute();
    auto t2 = high_resolution_clock::now();

    auto serialized_expected_ratios = read_all_file("./tmp/ratios_py.bin");
    auto expected_ratios = RatioVect<double>::deserialize(serialized_expected_ratios);

    assert(ratios == expected_ratios);
    
    auto duration_ns = duration_cast<nanoseconds>(t2 - t1).count();
    auto duration = duration_ns / 1e9;
    std::cout << duration << "s\n";    
}

// =========================================

void stdio_order_execution() {
    auto serialized_order = read_all_stdin();
    auto order = Order<u32string>::deserialize(serialized_order);
    auto ratios = order.execute();
    write_to_stdout(ratios.serialize());
}

int main() {
    stdio_order_execution();
}