#include <iostream>
#include <string>
#include <iomanip>
#include <ctime>
#include <sstream>

using namespace std;

// Hàm HashEngine mô phỏng lại logic djb2 từ MQL5
string HashEngine(string data) {
    unsigned int hash = 5381;
    for (size_t i = 0; i < data.length(); i++) {
        // (hash << 5) + hash tương đương với hash * 33
        hash = ((hash << 5) + hash) + (unsigned char)data[i];
    }

    // Trả về chuỗi Hex in hoa 8 ký tự
    stringstream ss;
    ss << hex << uppercase << setw(8) << setfill('0') << hash;
    return ss.str();
}

int main() {
    // Cấu hình đầu vào
    long long accID;
    string SecretSalt = "20042000";

    cout << "--- CHUONG TRINH TAO KEY XAC THUC ---" << endl;
    cout << "Nhap Account ID: ";
    cin >> accID;

    // Lấy thời gian thực từ hệ thống
    time_t t = time(0);
    tm* now = localtime(&t);
    int currentMonth = now->tm_mon + 1;    // tm_mon chay tu 0-11
    int currentYear = now->tm_year + 1900; // tm_year tinh tu nam 1900

    // format: accID + Salt + Month + Year
    stringstream dataStream;
    dataStream << accID << SecretSalt << currentMonth << currentYear;
    string timeData = dataStream.str();

    // Tạo Key
    string expectedKey = HashEngine(timeData);

    // Xuất kết quả
    cout << "\n-------------------------------------" << endl;
    cout << "Thoi gian hien tai: " << currentMonth << "/" << currentYear << endl;
    cout << "Chuoi build data  : " << timeData << endl;
    cout << "=> KEY CUA BAN LA  : " << expectedKey << endl;
    cout << "-------------------------------------" << endl;

    return 0;
}