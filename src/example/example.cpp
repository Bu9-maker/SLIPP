#include <slipp.h>
#include <iostream>
#include <strkey.h>
#include <fstream>

using namespace std;

#define KEY_LEN 64

int main()
{
    // SLIPP<StrKey<KEY_LEN>, int> slipp;

    // // insert key-values
    // slipp.insert(string("jdfkjo11df"), 1);
    // slipp.insert(string("abcdefghijklmn"), 3);
    // cout << "exists(\"abcdefghijklmn\") = " << (slipp.exists(string("abcdefghijklmn")) ? "true" : "false") << endl;
    // slipp.insert(string("abcdefghijklmnopqrst122"), 2);
    // cout << "exists(\"jdfkjo11df\") = " << (slipp.exists(string("jdfkjo11df")) ? "true" : "false") << endl;
    // cout << "exists(\"abcdefghijklmn\") = " << (slipp.exists(string("abcdefghijklmn")) ? "true" : "false") << endl;
    // cout << slipp.at(string("abcdefghijklmn")) << endl;

    // // show tree structure
    // slipp.show();

    vector<StrKey<KEY_LEN>> keys;
    ifstream file("../src/example/output_strings.txt");
    string line;
    while (getline(file, line))
    {
        keys.push_back(line);
    }
    file.close(); // close file
    // cout << "sort...\n";
    // sort(keys.begin(), keys.end());
    SLIPP<StrKey<KEY_LEN>, int, false> slipp;
    vector<StrKey<KEY_LEN>>::iterator it;
    int i = 0;
    for (it = keys.begin(); it != keys.end(); it++)
    {
        // cout << it->to_string() << endl;
        slipp.insert(*it, i++);
    }

    FILE *fp = freopen("../src/example/show.txt", "w", stdout);
    slipp.show();
    fclose(fp);

    // ofstream fout("../src/example/show.txt");
    // streambuf *pOld = cout.rdbuf(fout.rdbuf());
    // for (it = keys.begin(); it != keys.end(); it++)
    // {
    //     cout << slipp.at(*it) << endl;
    // }

    return 0;
}
