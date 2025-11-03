#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <thread>
#include <mutex>
#include <random>
#include <chrono>
#include <string>
#include <iomanip>

using namespace std;
using namespace std::chrono;

class ConcurrentData {
private:
    static constexpr int M = 3;
    int data[M]{};
    mutable mutex mtx[M];

public:
    int read(int idx) {
        lock_guard lock(mtx[idx]);
        return data[idx];
    }

    void write(int idx, int value) {
        lock_guard lock(mtx[idx]);
        data[idx] = value;
    }

    operator string() const {
        scoped_lock lock(mtx[0], mtx[1], mtx[2]);
        ostringstream oss;
        oss << "[" << data[0] << ", " << data[1] << ", " << data[2] << "]";
        return oss.str();
    }
};

void executeFile(const string& filename, ConcurrentData& obj) {
    ifstream fin(filename);
    string cmd;
    int idx, val;
    while (fin >> cmd) {
        if (cmd == "read") {
            fin >> idx;
            obj.read(idx);
        }
        else if (cmd == "write") {
            fin >> idx >> val;
            obj.write(idx, val);
        }
        else if (cmd == "string") {
            string s = obj;
        }
    }
}

void generateFile(const string& filename, const vector<double>& probabilities, int count) {
    vector<string> ops = { "read0", "write0", "read1", "write1", "read2", "write2", "string" };
    vector<double> cumulative;
    double sum = 0;
    for (auto p : probabilities) {
        sum += p;
        cumulative.push_back(sum);
    }

    random_device rd;
    mt19937 gen(rd());
    uniform_real_distribution<> dis(0.0, 100.0);

    ofstream fout(filename);
    for (int i = 0; i < count; ++i) {
        double r = dis(gen);
        for (size_t j = 0; j < cumulative.size(); ++j) {
            if (r < cumulative[j]) {
                if (ops[j].starts_with("read"))
                    fout << "read " << ops[j].back() - '0' << "\n";
                else if (ops[j].starts_with("write"))
                    fout << "write " << ops[j].back() - '0' << " 1\n";
                else
                    fout << "string\n";
                break;
            }
        }
    }
}

void runExperiment(const string& label, const vector<double>& probs) {
    cout << "\n=== " << label << " ===\n";
    int numOps = 200'000;
    vector<string> files = { "a.txt", "b.txt", "c.txt" };

    for (auto& f : files)
        generateFile(f, probs, numOps);

    ConcurrentData obj;

    for (int threads = 1; threads <= 3; ++threads) {
        vector<jthread> pool;
        auto start = steady_clock::now();
        for (int i = 0; i < threads; ++i)
            pool.emplace_back(executeFile, files[i], ref(obj));
        auto end = steady_clock::now();

        double ms = duration<double, milli>(end - start).count();
        cout << threads << " thread(s): " << ms << " ms\n";
    }

    cout << "Final state: " << string(obj) << endl;
}

int main() {
    cout << fixed << setprecision(2);

    vector<double> probsA = { 5, 5, 30, 5, 24, 1, 30 };

    vector<double> probsB(7, 100.0 / 7.0);

    vector<double> probsC = { 60, 1, 1, 1, 1, 1, 35 };

    runExperiment("Variant A (given probabilities)", probsA);
    runExperiment("Variant B (uniform probabilities)", probsB);
    runExperiment("Variant C (strongly non-uniform probabilities)", probsC);

    return 0;
}
