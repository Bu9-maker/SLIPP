#include <slipp.h>
#include <iostream>
#include <strkey.h>
#include <fstream>
#include <chrono>

using namespace std;

#define KEY_LEN 64

int main()
{

    // SLIPP<StrKey<KEY_LEN>, int> slipp;
    //  slipp.insert(string("abcdefgh12345678abc"), 1);
    //  slipp.insert(string("abcdefghabc12345678"), 3);
    //  slipp.insert(string("abcdefgh12345678123"), 2);
    //  cout << slipp.at(string("abcdefgh12345678123")) << endl;
    //  cout << slipp.at(string("abcdefgh12345678abc")) << endl;
    //  cout << slipp.at(string("abcdefghabc12345678")) << endl;
    //  slipp.show();

    vector<StrKey<KEY_LEN>> keys;
    ifstream file("./output_strings.txt");
    string line;
    while (getline(file, line))
    {
        keys.push_back(line);
    }
    file.close(); // close file
    // cout << "sort...\n";
    // sort(keys.begin(), keys.end());
    SLIPP<StrKey<KEY_LEN>, int> slipp;
    vector<StrKey<KEY_LEN>>::iterator it;
    int i = 0;
    for (it = keys.begin(); it != keys.end(); it++)
    {
        // cout << it->to_string() << endl;
        slipp.insert(*it, i++);
    }

    // FILE *fp = freopen("../src/example/show.txt", "w", stdout);
    // slipp.show();

    // for (it = keys.begin(); it != keys.end(); it++)
    // {
    //     cout << "exists(\"" << (*it).to_string() << "\")"
    //          << " = " << (slipp.exists(*it) ? "true" : "false") << endl;
    // }

    double sum = 0;
    double add = 1;

    // Start measuring time
    auto begin = std::chrono::high_resolution_clock::now();

    for (it = keys.begin(); it != keys.end(); it++)
    {
        // cout << "at(\"" << (*it).to_string() << "\")"
        //      << " = " << slipp.at(*it) << endl;
        slipp.at(*it);
    }

    // fclose(fp);

    auto end = std::chrono::high_resolution_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::nanoseconds>(end - begin);

    double t = (double)elapsed.count() / keys.size();
    cout << "throughput:" << t / 1000 << "Mops" << endl;

    return 0;
}
