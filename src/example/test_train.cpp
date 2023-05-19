#include <iostream>
#include <string>
#include <vector>
#include <fstream>
#include <strkey.h>
#include <linearmodel.h>

using namespace std;
#define SLICE_BYTES_NUM 8
#define KEY_LEN 64
int conflict_count(vector<int> positions)
{
    int temp = -1;
    int count = 0;
    for (vector<int>::iterator it = positions.begin(); it != positions.end(); it++)
    {
        if (temp == *it)
        {
            count++;
        }
        else
        {
            temp = *it;
        }
    }
    return count;
}
void show_vector(vector<StrKey<KEY_LEN>> keys)
{
    vector<StrKey<KEY_LEN>>::iterator it;
    for (it = keys.begin(); it != keys.end(); it++)
    {
        cout << it->to_string() << endl;
    }
}

int main()
{
    vector<StrKey<KEY_LEN>> keys;
    vector<int> positions;
    ifstream file("output_strings.txt");
    string line;
    while (getline(file, line))
    {
        keys.push_back(line);
    }
    file.close(); // close file
    // show_vector(keys);
    // cout << "sort...\n";
    sort(keys.begin(), keys.end());
    // show_vector(keys);
    LinearModel<StrKey<KEY_LEN>> model(0, 0, 0);
    model.train(keys.data(), keys.size());
    vector<StrKey<KEY_LEN>>::iterator it;
    for (it = keys.begin(); it != keys.end(); it++)
    {
        cout << floor((model.predict_double(*it) / keys.size()) * 2000) << endl;
        int position = floor((model.predict_double(*it) / keys.size()) * 2000);
        positions.push_back(position < 0 ? 0 : position);
    }
    cout << "conflict:" << conflict_count(positions) << endl;
    return 0;
}