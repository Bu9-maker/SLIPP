#include <iostream>
#include <string>
#include <vector>
#include <bitset>
#include <strkey.h>
using namespace std;
#define SLICE_BYTES_NUM 4
#define KEY_LEN 64
#define MAX_KEY 9187201950435737471 // 0x7F7F7F7F7F7F7F7F
#define MIN_KEY 2314885530818453536 // 0x2020202020202020

string printBinary(uint64_t num)
{
    bitset<64> binary(num);    // 将uint64_t类型的数字转换为bitset类型
    return binary.to_string(); // 输出二进制字符串
}
// 2^56=72057594037927936

vector<uint64_t> splitStringToUint64Array(string input)
{
    vector<uint64_t> result;
    uint64_t temp = 0;
    int count = 0;
    for (size_t i = 0; i < input.length(); i++)
    {
        if (count == SLICE_BYTES_NUM)
        { // 每读取SLICE_BYTES_NUM个字符转换为一个uint64_t类型
            result.push_back(temp);
            temp = 0;
            count = 0;
        }
        temp <<= 8;                             // 左移8位
        temp |= static_cast<uint8_t>(input[i]); // 将字符转换成对应的ASCII码值并加入到temp中
        count++;
    }
    if (count != 0)
    { // 处理剩余不足SLICE_BYTES_NUM个字符的情况
        while (count != SLICE_BYTES_NUM)
        {
            temp <<= 8;
            count++;
        }
        result.push_back(temp);
    }
    return result;
}

int main()
{
    string str("abcdefgh1234");
    string str2("abcdefghsadfwee1");
    // cin >> str;

    // test operator
    /*
    StrKey<KEY_LEN> strkey1(str);
    StrKey<KEY_LEN> strkey2("abcdg");
    StrKey<KEY_LEN> strkey3("abcdef");
    cout << (strkey1 < strkey2) << endl;
    cout << (strkey1 >= strkey2) << endl;
    cout << (strkey1 <= strkey2) << endl;
    cout << (strkey1 != strkey2) << endl;
    cout << (strkey1 == strkey3) << endl;
*/
    // test function to_model_key()
    /*
    vector<uint64_t> resVec(splitStringToUint64Array(str));
    cout << "The size of the array is: " << resVec.size() << endl;

    for (size_t i = 0; i < resVec.size(); ++i)
    {
        cout << "第 " << i + 1 << " 组数据：" << resVec[i] << endl;
        cout << "第 " << i + 1 << " 组数据：" << printBinary(resVec[i]) << endl;
    }
    */

    // StrKey<KEY_LEN> *strkeyp = new StrKey<KEY_LEN>("");
    // cout << strkeyp->max().to_model_key();
    //  cout << strkey << endl;
    //  array<uint64_t, KEY_LEN / 8> model_key = strkeyp->to_model_key();
    /*
    for (size_t i = 0; i < KEY_LEN / 8; i++)
    {
        cout << model_key[i] << endl;
    }
    */
    // StrKey<KEY_LEN> strkey4(strkey3);
    // cout << strkey4.to_string() << endl;
    /*
        StrKey<KEY_LEN> strkey1(str);
        StrKey<KEY_LEN> strkey2(str2);
        StrKey<KEY_LEN> strkey3(strkey1, strkey2);
        cout << strkey3.to_string() << endl;
    */
    // StrKey<KEY_LEN> sk1(str);
    // StrKey<KEY_LEN> sk2(sk1, 0);
    // cout << sk2.to_string() << endl;
    // StrKey<KEY_LEN> sk3(str2);
    // StrKey<KEY_LEN> sk4(sk3, 0);
    // cout << sk4.to_string() << endl;

    // double a = (double)7 / (MAX_KEY - MIN_KEY);
    // long double b = (long double)0 - (long double)7 * MIN_KEY / (MAX_KEY - MIN_KEY);
    // cout << a << endl
    //      << b << endl;

    // cout << a * 3688862942762792770 + b << endl;

    StrKey<KEY_LEN> sk1(str);
    StrKey<KEY_LEN> sk2(str2);
    cout << sk1.to_string() << endl
         << sk2.to_string() << endl;
    swap(sk1, sk2);
    cout << sk1.to_string() << endl
         << sk2.to_string() << endl;

    return (0);
}